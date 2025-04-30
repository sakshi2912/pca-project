/**
 * @file high_performance_coloring.cpp
 * @brief A high-performance graph coloring algorithm optimized for parallel execution
 * 
 * This implementation uses multiple optimization techniques including:
 * - Degree-based vertex ordering with sequential coloring for high-degree vertices
 * - Thread-local workload distribution to minimize conflicts
 * - Efficient conflict detection and resolution
 * - Vector-based data structures for better cache performance
 * - Author : Sakshi, Balasubramanian S
 */

#include <algorithm>
#include <atomic>
#include <omp.h>
#include <vector>
#include <unordered_set>
#include "graph.h"

/**
 * @class HighPerformanceColorGraph
 * @brief Implements an optimized parallel graph coloring algorithm
 * 
 * This class uses a hybrid approach combining degree-based ordering,
 * workload partitioning, and efficient conflict resolution to achieve
 * high-performance parallel graph coloring.
 */
class HighPerformanceColorGraph : public ColorGraph {
private:
    /**
     * @brief Finds the minimum available color for a vertex
     * 
     * Uses pre-allocated arrays instead of hash sets for better performance.
     * Iterates through neighbors to mark used colors and finds the first
     * available color.
     * 
     * @param node The vertex to be colored
     * @param graph The graph adjacency list (vector representation)
     * @param colors Current color assignments for all vertices
     * @param used_colors Pre-allocated array for tracking used colors
     * @return The smallest available color for the vertex
     */
    int findMinAvailableColor(int node, const std::vector<std::vector<int>>& graph, 
                             const std::vector<int>& colors, std::vector<bool>& used_colors) {
        // Reset the used colors array for reuse
        std::fill(used_colors.begin(), used_colors.end(), false);
        
        // Mark colors used by all neighbors (both colored and uncolored)
        for (int neighbor : graph[node]) {
            if (colors[neighbor] >= 0) {
                // Dynamically grow the used_colors array if needed
                if (colors[neighbor] >= static_cast<int>(used_colors.size())) {
                    used_colors.resize(colors[neighbor] + 1, false);
                }
                used_colors[colors[neighbor]] = true;
            }
        }
        
        // Find the first color that is not used by any neighbor
        for (int color = 0; color < static_cast<int>(used_colors.size()); color++) {
            if (!used_colors[color]) {
                return color;
            }
        }
        
        // If all colors in the array are used, return the next available color
        return used_colors.size();
    }

public:
    /**
     * @brief Builds an adjacency list representation of the graph
     * 
     * Creates an undirected graph from the provided vertices and edges.
     * 
     * @param nodes List of graph vertices
     * @param pairs List of edges where each edge is a pair of vertex indices
     * @param graph The resulting adjacency list (output parameter)
     */
    void buildGraph(std::vector<graphNode>& nodes, std::vector<std::pair<int, int>>& pairs,
                  std::unordered_map<graphNode, std::vector<graphNode>>& graph) {
        // Pre-allocate empty adjacency lists for all vertices
        for (auto& node : nodes) {
            graph[node] = {};
        }
        
        // Add edges to the adjacency lists (undirected graph)
        for (const auto& pair : pairs) {
            graph[pair.first].push_back(pair.second);
            graph[pair.second].push_back(pair.first);
        }
    }

    /**
     * @brief Colors the graph using an optimized parallel algorithm
     * 
     * The algorithm follows these key steps:
     * 1. Convert to a more efficient vector-based representation
     * 2. Sort vertices by degree (highest first) for better coloring
     * 3. Color high-degree vertices sequentially to reduce conflicts
     * 4. Partition remaining vertices across threads for load balancing
     * 5. Perform conflict detection and resolution in parallel
     * 6. Ensure final coloring correctness
     * 
     * @param graph The graph structure as an adjacency list
     * @param colors Output map to store the resulting vertex colors
     */
    void colorGraph(std::unordered_map<graphNode, std::vector<graphNode>>& graph,
                  std::unordered_map<graphNode, color>& colors) {
        int num_vertices = graph.size();
        int num_threads = omp_get_max_threads();
        
        // Convert to vector representation for better cache performance
        std::vector<std::vector<int>> vec_graph(num_vertices);
        for (int i = 0; i < num_vertices; i++) {
            vec_graph[i] = graph[i];
        }
        
        // Create vertex index array for degree-based ordering
        std::vector<int> vertices(num_vertices);
        for (int i = 0; i < num_vertices; i++) {
            vertices[i] = i;
        }
        
        // Sort vertices by degree (highest degree first)
        // This improves coloring efficiency as high-degree vertices are more constrained
        std::sort(vertices.begin(), vertices.end(), 
                 [&vec_graph](int a, int b) {
                     return vec_graph[a].size() > vec_graph[b].size();
                 });
        
        // Initialize color assignments to uncolored (-1)
        std::vector<int> vec_colors(num_vertices, -1);
        // Track the highest color used across all threads
        std::atomic<int> max_color{0};
        
        // PHASE 1: Sequential coloring of high-degree vertices
        // High-degree vertices can cause many conflicts if colored in parallel
        int high_degree_threshold = num_vertices / 100;  // Adaptive threshold based on graph size
        int high_degree_count = 0;
        
        for (int i = 0; i < num_vertices && 
             vec_graph[vertices[i]].size() > high_degree_threshold; i++) {
            int vertex = vertices[i];
            
            // Local array for tracking colors used by neighbors
            std::vector<bool> used_colors(max_color.load() + 1, false);
            
            // Find and assign minimum available color
            int vertex_color = findMinAvailableColor(vertex, vec_graph, vec_colors, used_colors);
            vec_colors[vertex] = vertex_color;
            
            // Update max color atomically if needed
            if (vertex_color >= max_color.load()) {
                max_color.store(vertex_color + 1);
            }
            
            high_degree_count++;
        }
        
        // PHASE 2: Thread-based load balancing for remaining vertices
        // Group vertices by thread to minimize inter-thread conflicts
        std::vector<std::vector<int>> thread_vertices(num_threads);
        
        for (int i = high_degree_count; i < num_vertices; i++) {
            // Assign each vertex to the thread with the least workload
            int min_thread = 0;
            int min_work = thread_vertices[0].size();
            
            for (int t = 1; t < num_threads; t++) {
                if (thread_vertices[t].size() < min_work) {
                    min_thread = t;
                    min_work = thread_vertices[t].size();
                }
            }
            
            thread_vertices[min_thread].push_back(vertices[i]);
        }
        
        // PHASE 3: Parallel coloring by thread with thread-local data
        // Each thread colors its assigned vertices independently
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            std::vector<bool> used_colors(max_color.load() + 1, false);
            
            for (int vertex : thread_vertices[thread_id]) {
                // Find and assign color
                int vertex_color = findMinAvailableColor(vertex, vec_graph, vec_colors, used_colors);
                vec_colors[vertex] = vertex_color;
                
                // Update max color if needed (using atomic compare-exchange for thread safety)
                if (vertex_color >= max_color.load()) {
                    int expected = max_color.load();
                    while (vertex_color >= expected &&
                          !max_color.compare_exchange_weak(expected, vertex_color + 1)) {
                        // Keep trying until successful update or another thread updates to higher value
                    }
                }
            }
        }
        
        // PHASE 4: Conflict detection and resolution
        // Iteratively resolve coloring conflicts up to a maximum number of iterations
        bool has_conflicts;
        int iterations = 0;
        const int MAX_ITERATIONS = 3;  // Limit iterations for performance
        
        std::vector<bool> conflict_flags(num_vertices, false);
        
        do {
            has_conflicts = false;
            std::fill(conflict_flags.begin(), conflict_flags.end(), false);
            
            // Detect conflicts between adjacent vertices
            #pragma omp parallel for reduction(||:has_conflicts)
            for (int i = 0; i < num_vertices; i++) {
                for (int neighbor : vec_graph[i]) {
                    if (i < neighbor && vec_colors[i] == vec_colors[neighbor]) {
                        // When conflict found, mark the lower-degree vertex for recoloring
                        // This heuristic preserves colors for more constrained vertices
                        if (vec_graph[i].size() <= vec_graph[neighbor].size()) {
                            conflict_flags[i] = true;
                        } else {
                            conflict_flags[neighbor] = true;
                        }
                        has_conflicts = true;
                    }
                }
            }
            
            // Resolve conflicts in parallel
            if (has_conflicts) {
                #pragma omp parallel for
                for (int i = 0; i < num_vertices; i++) {
                    if (conflict_flags[i]) {
                        std::vector<bool> used_colors(max_color.load() + 1, false);
                        int new_color = findMinAvailableColor(i, vec_graph, vec_colors, used_colors);
                        vec_colors[i] = new_color;
                        
                        // Update max color if needed (thread-safe)
                        if (new_color >= max_color.load()) {
                            int expected = max_color.load();
                            while (new_color >= expected &&
                                  !max_color.compare_exchange_weak(expected, new_color + 1)) {
                                // Keep trying if another thread updated it
                            }
                        }
                    }
                }
            }
            
            iterations++;
        } while (has_conflicts && iterations < MAX_ITERATIONS);
        
        // PHASE 5: Final validation and conflict resolution
        // If conflicts still exist after max iterations, resolve with unique colors
        if (has_conflicts) {
            #pragma omp parallel for
            for (int i = 0; i < num_vertices; i++) {
                for (int neighbor : vec_graph[i]) {
                    if (i < neighbor && vec_colors[i] == vec_colors[neighbor]) {
                        // Assign guaranteed unique color to resolve any remaining conflicts
                        #pragma omp critical
                        {
                            vec_colors[i] = max_color.fetch_add(1);
                        }
                    }
                }
            }
        }
        
        // Copy final coloring from vector back to the output map
        for (int i = 0; i < num_vertices; i++) {
            colors[i] = vec_colors[i];
        }
    }
};

/**
 * @brief Factory function that creates a new HighPerformanceColorGraph instance
 * 
 * @return A unique pointer to a new HighPerformanceColorGraph object
 */
std::unique_ptr<ColorGraph> createHighPerformanceColorGraph() {
    return std::make_unique<HighPerformanceColorGraph>();
}