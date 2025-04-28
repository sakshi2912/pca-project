// main_coloring.cpp
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include <omp.h>
#include <atomic>
#include <immintrin.h> // For Intel TSX instructions and prefetch
#include <x86intrin.h> // For __rdtsc()
#include <thread>      // For std::this_thread::sleep_for
#include "graph_txn.h"

// Constants for the HTM implementation
constexpr int MAX_RETRIES = 8;
constexpr int MAX_RESOLUTION_ITERATIONS = 3;
constexpr int MIN_COLORS_BUFFER = 16; // Extra buffer for color vectors
constexpr int CONTENTION_THRESHOLD = 4; // Threshold for identifying high contention vertices
constexpr int HIGH_DEGREE_MIN_THRESHOLD = 50; // Minimum high degree threshold
constexpr int PREFETCH_DISTANCE = 8;  // Prefetch distance for cache optimization
constexpr int FALLBACK_THRESHOLD = 3; // Consecutive abort threshold to trigger fallback
constexpr int VECTOR_BATCH_SIZE = 4;  // Size for vector coloring batch operations

struct VertexInfo {
    int color;
    int degree;
    bool processing_flag;
    char padding[5]; // Pad to 16 bytes for cache alignment
};

// Get the abort code from a TSX transaction
unsigned int _xabort_code = 0;

class OptimizedTSXGraphColoring {
    private:
        const Graph& graph;
        int num_threads;
        int num_vertices;
        std::vector<int> colors;
        std::vector<int> vertex_degrees;
        std::vector<int> ordered_vertices;
        std::atomic<int> max_color;
        std::vector<std::atomic<bool>> conflict_flags;
        std::vector<int> conflict_count;
        std::atomic<int> transaction_success_count{0};
        std::atomic<int> transaction_abort_count{0};
        
        // Fast vertex preparation with binning
        void prepareVertices() {
            vertex_degrees.resize(num_vertices);
            ordered_vertices.resize(num_vertices);
            
            // Calculate degree for each vertex
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < num_vertices; i++) {
                vertex_degrees[i] = graph.getNeighbors(i).size();
                ordered_vertices[i] = i;
            }
            
            // Use binning approach for large graphs
            if (num_vertices > 10000) {
                // Find max degree for binning
                int max_degree = 0;
                for (int d : vertex_degrees) {
                    max_degree = std::max(max_degree, d);
                }
                
                // Create degree bins (using vector of vectors is faster than priority queue for large data)
                std::vector<std::vector<int>> degree_bins(max_degree + 1);
                for (int i = 0; i < num_vertices; i++) {
                    degree_bins[vertex_degrees[i]].push_back(i);
                }
                
                // Flatten bins to ordered_vertices (highest degree first)
                int idx = 0;
                for (int d = max_degree; d >= 0; d--) {
                    for (int v : degree_bins[d]) {
                        ordered_vertices[idx++] = v;
                    }
                }
            } else {
                // For smaller graphs, direct sort is fine
                std::sort(ordered_vertices.begin(), ordered_vertices.end(),
                         [this](int a, int b) {
                             return vertex_degrees[a] > vertex_degrees[b] || 
                                    (vertex_degrees[a] == vertex_degrees[b] && a < b);
                         });
            }
        }
        
        // Optimized minimum color finder using bit vector
        int findMinAvailableColor(int vertex, int current_max_color) {
            // Use stack allocation for small color sets to avoid heap allocation
            constexpr int STACK_BUFFER_SIZE = 1024;
            bool stack_forbidden[STACK_BUFFER_SIZE] = {false};
            
            // For larger color sets, use heap
            std::unique_ptr<bool[]> heap_forbidden;
            bool* forbidden = stack_forbidden;
            
            // Ensure we have enough space
            const int buffer_size = current_max_color + 16;
            if (buffer_size > STACK_BUFFER_SIZE) {
                heap_forbidden = std::make_unique<bool[]>(buffer_size);
                std::fill_n(heap_forbidden.get(), buffer_size, false);
                forbidden = heap_forbidden.get();
            }
            
            // Mark colors used by neighbors
            const auto& neighbors = graph.getNeighbors(vertex);
            for (int neighbor : neighbors) {
                int neighbor_color = colors[neighbor];
                if (neighbor_color >= 0 && neighbor_color < buffer_size) {
                    forbidden[neighbor_color] = true;
                }
            }
            
            // Find first available color
            for (int color = 0; color < buffer_size; color++) {
                if (!forbidden[color]) {
                    return color;                }
            }
            
            return current_max_color;
        }
        
        // Pre-compute color outside transaction to reduce transaction size
        inline int precomputeColor(int vertex) {
            int current_max = max_color.load(std::memory_order_relaxed);
            return findMinAvailableColor(vertex, current_max);
        }
        
        // Check if vertex is likely to have high contention
        inline bool isHighContentionVertex(int vertex) {
            // Simple contention heuristic based on vertex degree
            return vertex_degrees[vertex] > 100;
        }
        
        // Handle high contention vertices without transactions
        void colorHighContentionVertex(int vertex) {
            #pragma omp critical(high_contention)
            {
                int current_max = max_color.load(std::memory_order_relaxed);
                int min_color = findMinAvailableColor(vertex, current_max);
                
                if (min_color >= current_max) {
                    max_color.store(min_color + 1, std::memory_order_relaxed);
                }
                
                colors[vertex] = min_color;
            }
        }
        
        // Enhanced adaptive backoff with reduced wait times
        void enhancedBackoff(int retry_count) {
            // Use exponentially increasing but bounded backoff
            const int base_delay = std::min(10 * retry_count, 100);
            const int max_delay = 1000;
            
            int delay = std::min(base_delay, max_delay);
            for (int j = 0; j < delay; j++) {
                _mm_pause(); // Short pause instruction
            }
        }
        
    public:
        OptimizedTSXGraphColoring(const Graph& g, int threads) 
            : graph(g), 
              num_threads(threads), 
              num_vertices(g.numVertices()),
              colors(g.numVertices(), -1),
              conflict_flags(g.numVertices()),
              conflict_count(g.numVertices(), 0),
              max_color(0)
        {
            prepareVertices();
        }
        
        std::vector<int> colorGraph() {
            // Adjust thread count based on graph size
            int optimal_threads = num_threads;
            if (num_vertices < 1000) {
                optimal_threads = std::min(num_threads, 2); // Use fewer threads for small graphs
            } else if (num_vertices > 10000 && vertex_degrees[ordered_vertices[0]] > 1000) {
                // For very dense graphs, reduce threads to avoid contention
                optimal_threads = std::max(1, num_threads / 2);
            }
            
            omp_set_num_threads(optimal_threads);
            std::cout << "Using " << optimal_threads << " threads for optimized TSX coloring" << std::endl;
            
            // First phase: color high-degree vertices sequentially to reduce conflicts
            const int high_degree_threshold = std::max(50, num_vertices / 100);
            int current_max = 0;
            int high_degree_count = 0;
            
            for (int i = 0; i < num_vertices && vertex_degrees[ordered_vertices[i]] > high_degree_threshold; i++) {
                int vertex = ordered_vertices[i];
                colors[vertex] = findMinAvailableColor(vertex, current_max);
                current_max = std::max(current_max, colors[vertex] + 1);
                high_degree_count++;
            }
            
            max_color.store(current_max);
            
            std::cout << "Pre-colored " << high_degree_count << " high-degree vertices using " 
                      << current_max << " colors" << std::endl;
            
            // Set dynamic chunk size for better load balancing
            const int chunk_size = std::max(32, num_vertices / (optimal_threads * 16));
            
            // Second phase: parallel coloring with optimized HTM
            #pragma omp parallel for schedule(dynamic, chunk_size)
            for (int i = high_degree_count; i < num_vertices; i++) {
                int vertex = ordered_vertices[i];
                
                // Skip already colored vertices
                if (colors[vertex] != -1) continue;
                
                // For high-contention vertices, use non-transactional approach
                if (isHighContentionVertex(vertex)) {
                    colorHighContentionVertex(vertex);
                    continue;
                }
                
                // Pre-compute color outside transaction
                int precomputed_color = precomputeColor(vertex);
                
                // if color doesn't increase max, just assign it
                int current_max = max_color.load(std::memory_order_relaxed);
                if (precomputed_color < current_max) {
                    colors[vertex] = precomputed_color;
                    continue;
                }
                
                // Standard HTM approach with reduced retries
                int retry_count = 0;
                bool success = false;
                const int MAX_RETRIES = 4; // Reduced from 8
                
                while (!success && retry_count < MAX_RETRIES) {
                    if (retry_count > 0) {
                        enhancedBackoff(retry_count);
                    }
                    
                    unsigned status = _xbegin();
                    
                    if (status == _XBEGIN_STARTED) {
                        // Get current max color
                        int current_max = max_color.load(std::memory_order_relaxed);
                        
                        // Use precomputed color if valid
                        int min_color;
                        if (retry_count == 0 && precomputed_color < current_max) {
                            min_color = precomputed_color;
                        } else {
                            min_color = findMinAvailableColor(vertex, current_max);
                        }
                        
                        // If we need a new color, update max atomically
                        if (min_color >= current_max) {
                            if (max_color.load(std::memory_order_relaxed) != current_max) {
                                _xabort(1);
                            }
                            max_color.store(min_color + 1, std::memory_order_relaxed);
                        }
                        
                        // Assign color
                        colors[vertex] = min_color;
                        
                        _xend();
                        success = true;
                        transaction_success_count.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        retry_count++;
                        transaction_abort_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
                
                // Fallback if all retries failed
                if (!success) {
                    colorHighContentionVertex(vertex);
                }
            }
            
            // Report transaction statistics
            std::cout << "Transaction statistics: " 
                      << transaction_success_count.load() << " successful, "
                      << transaction_abort_count.load() << " aborted" << std::endl;
            
            // Third phase: conflict detection and resolution 
            const int MAX_RESOLUTION_ITERATIONS = 2;
            bool has_conflicts = true;
            int resolution_iterations = 0;
            
            while (has_conflicts && resolution_iterations < MAX_RESOLUTION_ITERATIONS) {
                has_conflicts = false;
                
                // Reset conflict flags
                #pragma omp parallel for
                for (int i = 0; i < num_vertices; i++) {
                    conflict_flags[i].store(false, std::memory_order_relaxed);
                }
                
                // Detect conflicts in parallel
                #pragma omp parallel
                {
                    bool local_conflicts = false;
                    
                    #pragma omp for schedule(dynamic, chunk_size)
                    for (int i = 0; i < num_vertices; i++) {
                        int vertex = i;
                        int color_i = colors[vertex];
                        
                        const auto& neighbors = graph.getNeighbors(vertex);
                        for (int neighbor : neighbors) {
                            if (neighbor < vertex) continue; // Check each edge only once
                            
                            if (color_i == colors[neighbor]) {
                                // Determine which vertex to recolor based on degree
                                if (vertex_degrees[vertex] <= vertex_degrees[neighbor]) {
                                    conflict_flags[vertex].store(true, std::memory_order_relaxed);
                                } else {
                                    conflict_flags[neighbor].store(true, std::memory_order_relaxed);
                                }
                                
                                local_conflicts = true;
                            }
                        }
                    }
                    
                    // Combine thread-local conflict indicators
                    if (local_conflicts) {
                        #pragma omp atomic write
                        has_conflicts = true;
                    }
                }
                
                // Count conflict vertices
                int conflict_vertices = 0;
                #pragma omp parallel for reduction(+:conflict_vertices)
                for (int i = 0; i < num_vertices; i++) {
                    if (conflict_flags[i].load(std::memory_order_relaxed)) {
                        conflict_vertices++;
                    }
                }
                
                if (has_conflicts) {
                    std::cout << "Iteration " << resolution_iterations + 1 
                              << ": Found " << conflict_vertices << " conflicts" << std::endl;
                    
                    // Resolve conflicts
                    #pragma omp parallel for schedule(dynamic, 1)
                    for (int vertex = 0; vertex < num_vertices; vertex++) {
                        if (conflict_flags[vertex].load(std::memory_order_relaxed)) {
                            // Just assign a unique color to avoid further conflicts
                            int new_color = max_color.fetch_add(1, std::memory_order_relaxed);
                            colors[vertex] = new_color;
                            conflict_flags[vertex].store(false, std::memory_order_relaxed);
                        }
                    }
                }
                
                resolution_iterations++;
            }
            
            return colors;
        }        
        // Get statistics about the coloring process
        void printColoringStats() const {
            // Calculate transaction success rate
            int total_txn = transaction_success_count.load() + transaction_abort_count.load();
            float success_rate = (float)transaction_success_count.load() / (total_txn > 0 ? total_txn : 1) * 100.0f;
            
            std::cout << "TSX Transaction Statistics:" << std::endl;
            std::cout << "  Success rate: " << success_rate << "%" << std::endl;
            
            // Calculate color frequency
            std::vector<int> color_counts;
            int max_c = *std::max_element(colors.begin(), colors.end()) + 1;
            color_counts.resize(max_c, 0);
            
            for (int color : colors) {
                color_counts[color]++;
            }
            
            std::cout << "Color distribution: ";
            int top_colors = std::min(5, max_c);
            for (int i = 0; i < top_colors; i++) {
                std::cout << "Color " << i << ": " << color_counts[i] << " vertices, ";
            }
            std::cout << "..." << std::endl;
            
            // Print conflict stats
            int conflicts_total = 0;
            int max_conflicts = 0;
            for (int count : conflict_count) {
                conflicts_total += count;
                max_conflicts = std::max(max_conflicts, count);
            }
            
            std::cout << "Conflict resolution stats: " 
                      << conflicts_total << " total conflicts, "
                      << max_conflicts << " max conflicts per vertex" << std::endl;
        }
    };

// Function to verify coloring is valid
bool verifyColoring(const Graph& graph, const std::vector<int>& colors) {
    int num_vertices = graph.numVertices();
    
    for (int vertex = 0; vertex < num_vertices; vertex++) {
        for (int neighbor : graph.getNeighbors(vertex)) {
            if (colors[vertex] == colors[neighbor]) {
                std::cout << "Invalid coloring: vertices " << vertex << " and " 
                          << neighbor << " both have color " << colors[vertex] << std::endl;
                return false;
            }
        }
    }
    
    return true;
}

// Get the number of colors used
int countColors(const std::vector<int>& colors) {
    if (colors.empty()) return 0;
    return *std::max_element(colors.begin(), colors.end()) + 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <graph_file> [num_threads]" << std::endl;
        return 1;
    }
    
    std::string filename = argv[1];
    int num_threads = (argc > 2) ? std::stoi(argv[2]) : omp_get_max_threads();
    
    // Make sure thread count is valid
    int max_threads = omp_get_max_threads();
    if (num_threads > max_threads) {
        std::cout << "Warning: Requested " << num_threads << " threads but system only supports " 
                  << max_threads << ". Using " << max_threads << " threads." << std::endl;
        num_threads = max_threads;
    }
      try {
        // Load the graph with the optimized code
        std::cout << "Loading graph from file: " << filename << std::endl;
        Graph graph = loadGraphFromFile(filename);
        
        std::cout << "Loaded graph with " << graph.numVertices() << " vertices and " 
                  << graph.numEdges() << " edges" << std::endl;
        std::cout << "Running optimized TSX-based graph coloring with " << num_threads << " threads" << std::endl;
        
        // Run hardware transactional memory implementation with TSX optimizations
        auto start_time = std::chrono::high_resolution_clock::now();
        
        OptimizedTSXGraphColoring tsx_coloring(graph, num_threads);
        std::vector<int> colors = tsx_coloring.colorGraph();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_time = end_time - start_time;
        
        std::cout << "Optimized TSX coloring completed in " << elapsed_time.count() << " seconds" << std::endl;
        
        // Print detailed TSX performance statistics
        tsx_coloring.printColoringStats();
        
        // Verify and report results
        bool is_valid = verifyColoring(graph, colors);
        int num_colors = countColors(colors);
        
        std::cout << "Coloring is " << (is_valid ? "valid" : "INVALID") << std::endl;
        std::cout << "Used " << num_colors << " colors" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}