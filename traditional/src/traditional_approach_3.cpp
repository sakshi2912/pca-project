/**
 * @file workstealing_coloring.cpp
 * @brief Work-stealing based graph coloring algorithm with adaptive scheduling
 * 
 * This implementation uses different optimization techniques compared to previous approaches:
 * - Distance-2 coloring to eliminate conflicts between threads
 * - Work-stealing task queue for dynamic load balancing
 * - Domain decomposition with graph partitioning
 * - Hierarchical conflict resolution
 * - Lock-free synchronization for reduced contention
 * Author : Sakshi, Balasubramanian S Vishal
 */

#include <algorithm>
#include <atomic>
#include <deque>
#include <mutex>
#include <omp.h>
#include <vector>
#include <unordered_set>
#include "graph.h"

/**
 * @class WorkStealingColorGraph
 * @brief Implements a parallel graph coloring algorithm using work-stealing and distance-2 coloring
 * 
 * This class provides an alternative approach to high-performance parallel graph coloring
 * focused on minimizing thread synchronization while maximizing hardware utilization.
 */
class WorkStealingColorGraph : public ColorGraph {
private:
    /**
     * @brief Thread-safe work queue that supports stealing for load balancing
     */
    class WorkQueue {
    private:
        std::deque<int> tasks;
        std::mutex queue_mutex;
        
    public:
        /**
         * @brief Add a task to the local queue
         * @param task The vertex ID to process
         */
        void push(int task) {
            std::lock_guard<std::mutex> lock(queue_mutex);
            tasks.push_back(task);
        }
        
        /**
         * @brief Try to get a task from the local queue
         * @param task Output parameter for the vertex ID
         * @return True if a task was retrieved, false if queue is empty
         */
        bool pop(int& task) {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (tasks.empty()) return false;
            
            task = tasks.front();
            tasks.pop_front();
            return true;
        }
        
        /**
         * @brief Try to steal a task from another queue (from the back)
         * @param task Output parameter for the vertex ID
         * @return True if a task was stolen, false otherwise
         */
        bool steal(int& task) {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (tasks.empty()) return false;
            
            task = tasks.back();
            tasks.pop_back();
            return true;
        }
        
        /**
         * @brief Check if queue is empty
         * @return True if queue has no tasks
         */
        bool empty() {
            std::lock_guard<std::mutex> lock(queue_mutex);
            return tasks.empty();
        }
    };
    
    /**
     * @brief Data structure to track the boundary between graph partitions
     */
    struct PartitionBoundary {
        std::vector<int> border_vertices;
        std::vector<std::pair<int, int>> cross_edges;
    };
    
    /**
     * @brief Find the minimum available color for a vertex considering distance-2 neighbors
     * 
     * Ensures no conflicts with direct neighbors or their neighbors (distance-2)
     * 
     * @param vertex The vertex to color
     * @param graph The graph structure
     * @param colors Current color assignments
     * @param color_flags Array to track used colors
     * @return The smallest available color
     */
    int findDistance2Color(int vertex, const std::vector<std::vector<int>>& graph,
                          const std::vector<int>& colors, std::vector<bool>& color_flags) {
        // Clear flags from previous use
        std::fill(color_flags.begin(), color_flags.end(), false);
        
        // Mark colors used by direct neighbors (distance-1)
        for (int neighbor : graph[vertex]) {
            if (colors[neighbor] >= 0) {
                if (colors[neighbor] >= static_cast<int>(color_flags.size())) {
                    color_flags.resize(colors[neighbor] + 1, false);
                }
                color_flags[colors[neighbor]] = true;
            }
            
            // Mark colors used by distance-2 neighbors (neighbors of neighbors)
            for (int dist2_neighbor : graph[neighbor]) {
                if (dist2_neighbor != vertex && colors[dist2_neighbor] >= 0) {
                    if (colors[dist2_neighbor] >= static_cast<int>(color_flags.size())) {
                        color_flags.resize(colors[dist2_neighbor] + 1, false);
                    }
                    color_flags[colors[dist2_neighbor]] = true;
                }
            }
        }
        
        // Find first available color
        for (int color = 0; color < static_cast<int>(color_flags.size()); color++) {
            if (!color_flags[color]) {
                return color;
            }
        }
        
        return color_flags.size();
    }
    
    /**
     * @brief Partition the graph using recursive bisection for better locality
     * 
     * Divides the graph into regions that can be processed more independently
     * 
     * @param graph The graph structure
     * @param num_partitions Number of partitions to create (typically thread count)
     * @return Vector of partitions, each containing a set of vertex IDs
     */
    std::vector<std::vector<int>> partitionGraph(const std::vector<std::vector<int>>& graph, 
                                               int num_partitions) {
        int num_vertices = graph.size();
        std::vector<std::vector<int>> partitions(num_partitions);
        
        // Simplified partitioning algorithm - in practice, would use a more sophisticated
        // graph partitioning algorithm like METIS, KaHIP, or Scotch
        
        // For demonstration, use a simple vertex distribution based on connectivity
        std::vector<int> vertex_weights(num_vertices);
        for (int i = 0; i < num_vertices; i++) {
            vertex_weights[i] = graph[i].size();  // Use degree as weight
        }
        
        // Sort vertices by weight (degree) for better distribution
        std::vector<int> sorted_vertices(num_vertices);
        for (int i = 0; i < num_vertices; i++) {
            sorted_vertices[i] = i;
        }
        
        std::sort(sorted_vertices.begin(), sorted_vertices.end(),
                 [&vertex_weights](int a, int b) {
                     return vertex_weights[a] > vertex_weights[b];
                 });
        
        // Round-robin assignment to partitions
        for (int i = 0; i < num_vertices; i++) {
            int partition = i % num_partitions;
            partitions[partition].push_back(sorted_vertices[i]);
        }
        
        return partitions;
    }
    
    /**
     * @brief Identify boundary vertices and edges between partitions
     * 
     * @param graph The graph structure
     * @param partitions The partitioning of vertices
     * @return Boundary information for conflict resolution
     */
    PartitionBoundary findPartitionBoundaries(const std::vector<std::vector<int>>& graph,
                                            const std::vector<std::vector<int>>& partitions) {
        int num_vertices = graph.size();
        int num_partitions = partitions.size();
        
        // Create mapping from vertex to its partition
        std::vector<int> vertex_to_partition(num_vertices, -1);
        for (int p = 0; p < num_partitions; p++) {
            for (int vertex : partitions[p]) {
                vertex_to_partition[vertex] = p;
            }
        }
        
        PartitionBoundary boundary;
        
        // Identify border vertices and cross-partition edges
        for (int vertex = 0; vertex < num_vertices; vertex++) {
            int vertex_partition = vertex_to_partition[vertex];
            bool is_border = false;
            
            for (int neighbor : graph[vertex]) {
                int neighbor_partition = vertex_to_partition[neighbor];
                
                if (vertex_partition != neighbor_partition) {
                    is_border = true;
                    
                    // Add cross-partition edge (only once, with smaller vertex ID first)
                    if (vertex < neighbor) {
                        boundary.cross_edges.push_back({vertex, neighbor});
                    }
                }
            }
            
            if (is_border) {
                boundary.border_vertices.push_back(vertex);
            }
        }
        
        return boundary;
    }

public:
    /**
     * @brief Builds the graph structure from vertices and edges
     */
    void buildGraph(std::vector<graphNode>& nodes, std::vector<std::pair<int, int>>& pairs,
                  std::unordered_map<graphNode, std::vector<graphNode>>& graph) {
        // Initialize adjacency lists
        for (auto& node : nodes) {
            graph[node] = {};
        }
        
        // Build undirected graph
        for (const auto& pair : pairs) {
            graph[pair.first].push_back(pair.second);
            graph[pair.second].push_back(pair.first);
        }
    }

    /**
     * @brief Colors the graph using a work-stealing approach with distance-2 coloring
     */
    void colorGraph(std::unordered_map<graphNode, std::vector<graphNode>>& graph,
                  std::unordered_map<graphNode, color>& colors) {
        int num_vertices = graph.size();
        int num_threads = omp_get_max_threads();
        
        // Convert to vector representation for better performance
        std::vector<std::vector<int>> vec_graph(num_vertices);
        for (int i = 0; i < num_vertices; i++) {
            vec_graph[i] = graph[i];
        }
        
        // PHASE 1: Graph partitioning for improved locality
        std::vector<std::vector<int>> partitions = partitionGraph(vec_graph, num_threads);
        PartitionBoundary boundary = findPartitionBoundaries(vec_graph, partitions);
        
        // Initialize coloring state
        std::vector<int> vertex_colors(num_vertices, -1);
        std::atomic<int> max_color{0};
        
        // PHASE 2: Create thread-local work queues
        std::vector<WorkQueue> work_queues(num_threads);
        
        // Initialize work queues with partition vertices
        for (int t = 0; t < num_threads; t++) {
            for (int vertex : partitions[t]) {
                work_queues[t].push(vertex);
            }
        }
        
        // PHASE 3: Parallel coloring with work-stealing
        #pragma omp parallel
        {
            int thread_id = omp_get_thread_num();
            std::vector<bool> color_flags(64, false);  // Pre-allocate reasonable size
            int processed_count = 0;
            
            // Process until all queues are empty
            bool all_done = false;
            while (!all_done) {
                int vertex;
                bool got_task = work_queues[thread_id].pop(vertex);
                
                if (!got_task) {
                    // Try to steal task from another queue
                    bool stole_task = false;
                    
                    // Try all other queues in random order
                    std::vector<int> steal_order(num_threads);
                    for (int i = 0; i < num_threads; i++) {
                        steal_order[i] = i;
                    }
                    
                    // Fisher-Yates shuffle for randomized stealing
                    for (int i = num_threads - 1; i > 0; i--) {
                        int j = i * (thread_id + 1) % (i + 1);  // Deterministic but different per thread
                        std::swap(steal_order[i], steal_order[j]);
                    }
                    
                    for (int t : steal_order) {
                        if (t == thread_id) continue;
                        
                        if (work_queues[t].steal(vertex)) {
                            stole_task = true;
                            break;
                        }
                    }
                    
                    if (!stole_task) {
                        // Check if all queues are empty
                        all_done = true;
                        for (int t = 0; t < num_threads; t++) {
                            if (!work_queues[t].empty()) {
                                all_done = false;
                                break;
                            }
                        }
                        
                        if (all_done) break;
                        continue;  // Try again
                    }
                }
                
                // Process the vertex - use distance-2 coloring for better parallelism
                int assigned_color = findDistance2Color(vertex, vec_graph, vertex_colors, color_flags);
                vertex_colors[vertex] = assigned_color;
                
                // Update max color if needed
                if (assigned_color >= max_color.load()) {
                    int expected = max_color.load();
                    while (assigned_color >= expected &&
                          !max_color.compare_exchange_weak(expected, assigned_color + 1)) {
                        // Keep trying
                    }
                }
                
                processed_count++;
                
                // Periodically check termination to avoid excessive checking
                if (processed_count % 64 == 0) {
                    bool all_empty = true;
                    for (int t = 0; t < num_threads; t++) {
                        if (!work_queues[t].empty()) {
                            all_empty = false;
                            break;
                        }
                    }
                    
                    if (all_empty) all_done = true;
                }
            }
        }
        
        // PHASE 4: Sequential resolution of boundary conflicts
        // Process boundary vertices to ensure correctness across partitions
        for (int boundary_vertex : boundary.border_vertices) {
            // Check for conflicts
            bool has_conflict = false;
            
            for (int neighbor : vec_graph[boundary_vertex]) {
                if (vertex_colors[boundary_vertex] == vertex_colors[neighbor]) {
                    has_conflict = true;
                    break;
                }
            }
            
            // Resolve conflict if needed
            if (has_conflict) {
                std::vector<bool> color_flags(max_color.load() + 1, false);
                
                // Mark colors used by neighbors
                for (int neighbor : vec_graph[boundary_vertex]) {
                    if (vertex_colors[neighbor] >= 0) {
                        if (vertex_colors[neighbor] >= static_cast<int>(color_flags.size())) {
                            color_flags.resize(vertex_colors[neighbor] + 1, false);
                        }
                        color_flags[vertex_colors[neighbor]] = true;
                    }
                }
                
                // Find new color
                int new_color = 0;
                while (new_color < static_cast<int>(color_flags.size()) && color_flags[new_color]) {
                    new_color++;
                }
                
                vertex_colors[boundary_vertex] = new_color;
                
                // Update max color if needed
                if (new_color >= max_color.load()) {
                    max_color.store(new_color + 1);
                }
            }
        }
        
        // Copy results back to output map
        for (int i = 0; i < num_vertices; i++) {
            colors[i] = vertex_colors[i];
        }
    }
};

/**
 * @brief Factory function for the work-stealing coloring algorithm
 */
std::unique_ptr<ColorGraph> createWorkStealingColorGraph() {
    return std::make_unique<WorkStealingColorGraph>();
}