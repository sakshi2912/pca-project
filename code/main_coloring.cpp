// main_coloring.cpp - Main function only
// (Keep the rest of your code the same)
// main_coloring.cpp
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <chrono>
#include <omp.h>
#include <atomic>
#include <immintrin.h> // For Intel TSX instructions
#include "graph_txn.h"

// Constants for the HTM implementation
constexpr int MAX_RETRIES = 8;
constexpr int MAX_RESOLUTION_ITERATIONS = 3;
constexpr int MIN_COLORS_BUFFER = 16; // Extra buffer for color vectors

class HTMGraphColoring {
private:
    const Graph& graph;
    int num_threads;
    int num_vertices;
    std::vector<int> colors;
    std::vector<int> vertex_degrees;
    std::vector<int> ordered_vertices;
    std::atomic<int> max_color;
    std::vector<std::atomic<bool>> conflict_flags;
    
    // Random number generator for backoff
    std::mt19937 gen;
    
    // Calculate vertex degrees and order vertices by degree
    void prepareVertices() {
        vertex_degrees.resize(num_vertices);
        ordered_vertices.resize(num_vertices);
        
        // Calculate degree for each vertex
        for (int i = 0; i < num_vertices; i++) {
            vertex_degrees[i] = graph.getNeighbors(i).size();
            ordered_vertices[i] = i;
        }
        
        // Sort vertices by degree (highest degree first)
        std::sort(ordered_vertices.begin(), ordered_vertices.end(),
                 [this](int a, int b) {
                     return vertex_degrees[a] > vertex_degrees[b] || 
                            (vertex_degrees[a] == vertex_degrees[b] && a < b);
                 });
    }
    
    // Find the smallest available color for a vertex
    int findMinAvailableColor(int vertex, int current_max_color) {
        // Pre-allocate enough space (with buffer) to avoid resizing
        const int buffer_size = current_max_color + MIN_COLORS_BUFFER;
        std::vector<bool> forbidden(buffer_size, false);
        
        // Mark colors of neighbors as forbidden
        for (int neighbor : graph.getNeighbors(vertex)) {
            int neighbor_color = colors[neighbor];
            if (neighbor_color >= 0 && neighbor_color < buffer_size) {
                forbidden[neighbor_color] = true;
            }
        }
        
        // Find the first available color
        for (int color = 0; color < buffer_size; color++) {
            if (!forbidden[color]) {
                return color;
            }
        }
        
        // If we get here, we need a new color
        return current_max_color;
    }
    
public:
    HTMGraphColoring(const Graph& g, int threads) 
        : graph(g), 
          num_threads(threads), 
          num_vertices(g.numVertices()),
          colors(g.numVertices(), -1),
          conflict_flags(g.numVertices()),
          max_color(0),
          gen(std::random_device{}())
    {
        prepareVertices();
    }
    
    std::vector<int> colorGraph() {
        // Set number of threads
        omp_set_num_threads(num_threads);
        
        // First phase: color high-degree vertices sequentially to reduce conflicts
        const int high_degree_threshold = std::max(50, num_vertices / 100);
        int current_max = 0;
        
        for (int i = 0; i < num_vertices && vertex_degrees[ordered_vertices[i]] > high_degree_threshold; i++) {
            int vertex = ordered_vertices[i];
            colors[vertex] = findMinAvailableColor(vertex, current_max);
            current_max = std::max(current_max, colors[vertex] + 1);
        }
        
        max_color.store(current_max);
        
        // Set chunk size based on vertex count and thread count for balanced work distribution
        const int chunk_size = std::max(64, num_vertices / (num_threads * 8));
        
        // Second phase: parallel coloring with HTM
        #pragma omp parallel for schedule(dynamic, chunk_size)
        for (int i = 0; i < num_vertices; i++) {
            int vertex = ordered_vertices[i];
            
            // Skip already colored vertices (high degree ones)
            if (colors[vertex] != -1) continue;
            
            int retry_count = 0;
            bool success = false;
            
            while (!success && retry_count < MAX_RETRIES) {
                // Exponential backoff when retrying
                if (retry_count > 0) {
                    std::uniform_int_distribution<> backoff(1, (1 << std::min(retry_count, 6)));
                    int delay = backoff(gen);
                    for (int j = 0; j < delay * 50; j++) {
                        _mm_pause(); // Short delay with pause instruction
                    }
                }
                
                // Begin the transaction
                unsigned status = _xbegin();
                
                if (status == _XBEGIN_STARTED) {
                    // Transaction started successfully
                    
                    // Get the current max color
                    int current_max = max_color.load(std::memory_order_relaxed);
                    
                    // Find minimum available color
                    int min_color = findMinAvailableColor(vertex, current_max);
                    
                    // If we need a new color, update atomically
                    if (min_color >= current_max) {
                        int new_max = current_max + 1;
                        // This read will be part of the transaction and abort if max_color changes
                        if (max_color.load(std::memory_order_relaxed) != current_max) {
                            _xabort(1); // Abort if someone else changed max_color
                        }
                        max_color.store(new_max, std::memory_order_relaxed);
                    }
                    
                    // Assign the color
                    colors[vertex] = min_color;
                    
                    // End transaction
                    _xend();
                    success = true;
                } else {
                    // Transaction failed, retry
                    retry_count++;
                }
            }
            
            // If all retries failed, use the non-transactional fallback path
            if (!success) {
                #pragma omp critical
                {
                    int current_max = max_color.load(std::memory_order_relaxed);
                    int min_color = findMinAvailableColor(vertex, current_max);
                    
                    if (min_color >= current_max) {
                        max_color.store(current_max + 1, std::memory_order_relaxed);
                    }
                    
                    colors[vertex] = min_color;
                }
            }
        }
        
        // Third phase: conflict detection and resolution
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
            #pragma omp parallel for reduction(||:has_conflicts) schedule(dynamic, chunk_size)
            for (int i = 0; i < num_vertices; i++) {
                int vertex = i;
                int color_i = colors[vertex];
                
                for (int neighbor : graph.getNeighbors(vertex)) {
                    if (neighbor < vertex) continue; // Check each edge only once
                    
                    if (color_i == colors[neighbor]) {
                        // Determine which vertex to recolor based on degree and index
                        if (vertex_degrees[vertex] < vertex_degrees[neighbor] || 
                            (vertex_degrees[vertex] == vertex_degrees[neighbor] && vertex > neighbor)) {
                            conflict_flags[vertex].store(true, std::memory_order_relaxed);
                            has_conflicts = true;
                        } else {
                            conflict_flags[neighbor].store(true, std::memory_order_relaxed);
                            has_conflicts = true;
                        }
                    }
                }
            }
            
            // Resolve conflicts using HTM
            if (has_conflicts) {
                #pragma omp parallel for schedule(dynamic, 1)
                for (int vertex = 0; vertex < num_vertices; vertex++) {
                    if (conflict_flags[vertex].load(std::memory_order_relaxed)) {
                        int retry_count = 0;
                        bool success = false;
                        
                        while (!success && retry_count < MAX_RETRIES) {
                            if (retry_count > 0) {
                                std::uniform_int_distribution<> backoff(1, (1 << std::min(retry_count, 6)));
                                int delay = backoff(gen);
                                for (int j = 0; j < delay * 50; j++) {
                                    _mm_pause();
                                }
                            }
                            
                            unsigned status = _xbegin();
                            
                            if (status == _XBEGIN_STARTED) {
                                // Re-check if still in conflict (might have been resolved by another thread)
                                if (conflict_flags[vertex].load(std::memory_order_relaxed)) {
                                    int current_max = max_color.load(std::memory_order_relaxed);
                                    int min_color = findMinAvailableColor(vertex, current_max);
                                    
                                    if (min_color >= current_max) {
                                        int new_max = current_max + 1;
                                        if (max_color.load(std::memory_order_relaxed) != current_max) {
                                            _xabort(1);
                                        }
                                        max_color.store(new_max, std::memory_order_relaxed);
                                    }
                                    
                                    colors[vertex] = min_color;
                                    conflict_flags[vertex].store(false, std::memory_order_relaxed);
                                }
                                
                                _xend();
                                success = true;
                            } else {
                                retry_count++;
                            }
                        }
                        
                        // Fallback
                        if (!success) {
                            #pragma omp critical
                            {
                                if (conflict_flags[vertex].load(std::memory_order_relaxed)) {
                                    int current_max = max_color.load(std::memory_order_relaxed);
                                    int min_color = findMinAvailableColor(vertex, current_max);
                                    
                                    if (min_color >= current_max) {
                                        max_color.store(current_max + 1, std::memory_order_relaxed);
                                    }
                                    
                                    colors[vertex] = min_color;
                                    conflict_flags[vertex].store(false, std::memory_order_relaxed);
                                }
                            }
                        }
                    }
                }
            }
            
            resolution_iterations++;
        }
        
        return colors;
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
        std::cout << "Running HTM-based graph coloring with " << num_threads << " threads" << std::endl;
        
        // Run hardware transactional memory implementation
        auto start_time = std::chrono::high_resolution_clock::now();
        
        HTMGraphColoring htm_coloring(graph, num_threads);
        std::vector<int> colors = htm_coloring.colorGraph();
        
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_time = end_time - start_time;
        
        std::cout << "HTM coloring completed in " << elapsed_time.count() << " seconds" << std::endl;
        
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
