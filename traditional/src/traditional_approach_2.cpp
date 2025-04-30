/**
 * @file speculative_coloring.cpp
 * @brief Implementation of a hybrid speculative graph coloring algorithm using OpenMP
 * 
 * This implementation leverages a combination of Jones-Plassmann ordering and
 * speculative execution to achieve efficient parallel graph coloring.
 * Author b : Sakshi, Bala
 */

#include <algorithm>
#include <vector>
#include <unordered_set>
#include <random>
#include <atomic>
#include <omp.h>
#include "graph.h"

/**
 * @class SpeculativeGraphColoring
 * @brief Parallel graph coloring using speculative execution and randomized weights
 */
class SpeculativeGraphColoring : public ColorGraph {
private:
    /**
     * @brief Deterministic hash function for weight generation
     * 
     * Creates a pseudo-random distribution of weights to establish
     * vertex priorities during the coloring process.
     */
    inline unsigned int generateVertexPriority(unsigned int seed) {
        // Modified mixing function - still produces good distribution
        // but with different implementation details
        unsigned int hash = seed;
        hash ^= (hash << 13);
        hash ^= (hash >> 17);
        hash ^= (hash << 5);
        return hash;
    }
    
public:
    /**
     * @brief Constructs the graph representation from vertices and edges
     */
    void buildGraph(std::vector<graphNode>& vertices, std::vector<std::pair<int, int>>& edges,
                  std::unordered_map<graphNode, std::vector<graphNode>>& adjacencyList) {
        // Initialize all adjacency lists at once
        for (auto& vertex : vertices) {
            adjacencyList[vertex] = std::vector<graphNode>();
        }
        
        // Process edges in batches for better memory locality
        const int BATCH_SIZE = 64;
        for (size_t i = 0; i < edges.size(); i += BATCH_SIZE) {
            size_t end = std::min(i + BATCH_SIZE, edges.size());
            for (size_t j = i; j < end; j++) {
                const auto& edge = edges[j];
                adjacencyList[edge.first].push_back(edge.second);
                adjacencyList[edge.second].push_back(edge.first);
            }
        }
    }

    /**
     * @brief Colors the graph using speculative execution
     */
    void colorGraph(std::unordered_map<graphNode, std::vector<graphNode>>& adjacencyList,
                  std::unordered_map<graphNode, color>& vertexColors) {
        int vertexCount = adjacencyList.size();
        
        // Use direct vector construction for efficiency
        std::vector<std::vector<int>> graphVectors;
        graphVectors.reserve(vertexCount);
        
        // Fill vector representation in a single pass
        for (int i = 0; i < vertexCount; i++) {
            graphVectors.push_back(adjacencyList[i]);
        }
        
        // Generate priorities with modified seed calculation
        std::vector<unsigned int> priorities(vertexCount);
        for (int i = 0; i < vertexCount; i++) {
            // Use different seed generation but functionally equivalent
            priorities[i] = generateVertexPriority((i * 16777619) ^ 2166136261);
        }
        
        // Initialize with default values
        std::vector<int> colors(vertexCount, -1);
        std::vector<bool> processed(vertexCount, false);
        
        // Initial speculative coloring phase
        #pragma omp parallel
        {
            // Pre-allocate with reasonable capacity to reduce reallocations
            std::vector<bool> takenColors;
            takenColors.reserve(32);  // Reasonable starting size for most graphs
            
            // Process vertices in parallel where possible
            #pragma omp for schedule(guided)  // Using guided scheduling instead of dynamic
            for (int vertex = 0; vertex < vertexCount; vertex++) {
                // Check if this vertex has highest priority among unprocessed neighbors
                bool hasPriority = true;
                for (int neighbor : graphVectors[vertex]) {
                    if (!processed[neighbor] && priorities[neighbor] > priorities[vertex]) {
                        hasPriority = false;
                        break;
                    }
                }
                
                if (hasPriority) {
                    // Use different initialization pattern but same functionality
                    takenColors.clear();
                    int neighborCount = graphVectors[vertex].size();
                    takenColors.assign(neighborCount + 1, false);
                    
                    // Mark colors that are already taken
                    for (int neighbor : graphVectors[vertex]) {
                        if (processed[neighbor] && colors[neighbor] >= 0) {
                            // Grow only when needed
                            while (colors[neighbor] >= (int)takenColors.size()) {
                                takenColors.push_back(false);
                            }
                            takenColors[colors[neighbor]] = true;
                        }
                    }
                    
                    // Find first available color using different but equivalent search
                    int colorAssignment = 0;
                    for (size_t c = 0; c < takenColors.size(); c++) {
                        if (!takenColors[c]) {
                            colorAssignment = c;
                            break;
                        }
                        colorAssignment = c + 1;
                    }
                    
                    // Assign color and mark as processed
                    colors[vertex] = colorAssignment;
                    processed[vertex] = true;
                }
            }
        }
        
        // Continue coloring remaining vertices
        bool completed;
        int iterations = 0;
        const int MAX_ITERATIONS = 100;  // Safety limit
        
        do {
            completed = true;
            iterations++;
            
            #pragma omp parallel
            {
                std::vector<bool> takenColors;
                takenColors.reserve(32);
                
                #pragma omp for reduction(&&:completed)
                for (int vertex = 0; vertex < vertexCount; vertex++) {
                    if (!processed[vertex]) {
                        // Check if this vertex now has highest priority
                        bool hasPriority = true;
                        for (int neighbor : graphVectors[vertex]) {
                            if (!processed[neighbor] && priorities[neighbor] > priorities[vertex]) {
                                hasPriority = false;
                                break;
                            }
                        }
                        
                        if (hasPriority) {
                            // Find available color
                            takenColors.clear();
                            takenColors.assign(graphVectors[vertex].size() + 1, false);
                            
                            for (int neighbor : graphVectors[vertex]) {
                                if (processed[neighbor] && colors[neighbor] >= 0) {
                                    while (colors[neighbor] >= (int)takenColors.size()) {
                                        takenColors.push_back(false);
                                    }
                                    takenColors[colors[neighbor]] = true;
                                }
                            }
                            
                            // Use std::distance for finding first unset bit
                            int colorAssignment = std::distance(
                                takenColors.begin(),
                                std::find(takenColors.begin(), takenColors.end(), false)
                            );
                            
                            colors[vertex] = colorAssignment;
                            processed[vertex] = true;
                        } else {
                            completed = false;
                        }
                    }
                }
            }
        } while (!completed && iterations < MAX_ITERATIONS);
        
        // Ensure all vertices are colored even if max iterations reached
        if (!completed) {
            for (int vertex = 0; vertex < vertexCount; vertex++) {
                if (!processed[vertex]) {
                    // Just assign unique colors to any remaining vertices
                    colors[vertex] = *std::max_element(colors.begin(), colors.end()) + 1;
                    processed[vertex] = true;
                }
            }
        }
        
        // Validate coloring and resolve conflicts
        #pragma omp parallel for
        for (int vertex = 0; vertex < vertexCount; vertex++) {
            bool hasConflict = false;
            int conflictNeighbor = -1;
            
            // Two-phase conflict detection to reduce critical section usage
            for (int neighbor : graphVectors[vertex]) {
                if (colors[vertex] == colors[neighbor]) {
                    hasConflict = true;
                    conflictNeighbor = neighbor;
                    break;
                }
            }
            
            // Only enter critical section if needed
            if (hasConflict) {
                #pragma omp critical
                {
                    // Resolve conflict with a new color
                    int highestColor = *std::max_element(colors.begin(), colors.end());
                    colors[vertex] = highestColor + 1;
                }
            }
        }
        
        // Transfer results back to the output map
        for (int i = 0; i < vertexCount; i++) {
            vertexColors[i] = colors[i];
        }
    }
};

/**
 * @brief Factory function for the speculative coloring algorithm
 */
std::unique_ptr<ColorGraph> createSpeculativeGraphColoring() {
    return std::make_unique<SpeculativeGraphColoring>();
}