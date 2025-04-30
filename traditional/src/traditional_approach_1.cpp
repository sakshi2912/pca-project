/**
 * @file traditional_approach_1.cpp (basic_parallel_coloring)
 * @brief Implementation of a basic parallel graph coloring algorithm using OpenMP
 * 
 * This file contains a parallel graph coloring implementation that uses a greedy
 * approach with conflict resolution. The algorithm proceeds in three phases:
 * 1. Initial coloring assignment
 * 2. Conflict detection and resolution 
 * 3. Color optimization to minimize the total number of colors used
 * Author : Sakshi , Balasubramanian S 
 */

#include <algorithm>
#include <unordered_set>
#include "graph.h"

/**
 * @class BasicParallelColorGraph
 * @brief Provides a simple OpenMP-based parallel graph coloring implementation
 * 
 * This class implements a greedy graph coloring algorithm parallelized using OpenMP.
 * It follows the minimum available color assignment strategy, with post-processing
 * to resolve conflicts and optimize color usage.
 */
class BasicParallelColorGraph : public ColorGraph {
public:
  /**
   * @brief Builds an adjacency list representation of the graph
   * 
   * Creates an adjacency list from a set of vertices and edges. Each vertex
   * maintains a list of all its adjacent vertices.
   * 
   * @param vertices List of graph vertices
   * @param edges List of edges where each edge is a pair of vertex indices
   * @param adjacencyList The resulting adjacency list (output parameter)
   */
  void buildGraph(std::vector<graphNode>& vertices, 
                  std::vector<std::pair<int, int>>& edges,
                  std::unordered_map<graphNode, std::vector<graphNode>>& adjacencyList) {
    // Initialize empty adjacency lists for each vertex
    for (auto& vertex : vertices) {
      adjacencyList[vertex] = {};
    }
    
    // Populate the adjacency lists by processing each edge
    size_t edgeCount = edges.size();
    for (size_t i = 0; i < edgeCount; i++) {
      int sourceVertex = edges[i].first;
      int targetVertex = edges[i].second;
      
      // Add each vertex to the other's adjacency list (undirected graph)
      adjacencyList[sourceVertex].push_back(targetVertex);
      adjacencyList[targetVertex].push_back(sourceVertex);
    }
  }

  /**
   * @brief Determines the minimum available color for a vertex
   * 
   * Finds the smallest color index that is not used by any of the vertex's
   * lower-indexed neighbors that have already been colored.
   * 
   * @param vertex The vertex to find a color for
   * @param adjacencyList The graph structure
   * @param vertexColors Map of currently assigned colors to vertices
   * @return The minimum available color index
   */
  int findMinimumAvailableColor(int vertex, 
                          std::unordered_map<graphNode, std::vector<graphNode>>& adjacencyList,
                          std::unordered_map<graphNode, color>& vertexColors) {
    // Track colors used by neighboring vertices
    std::unordered_set<int> neighborColors;
    
    // Collect colors of already-processed neighbors
    for (const auto& neighbor : adjacencyList[vertex]) {
      // Only consider neighbors with lower indices that have been colored
      if (neighbor < vertex && vertexColors.count(neighbor) > 0) {
        neighborColors.insert(vertexColors[neighbor]);
      }
    }
    
    // Find the smallest non-negative integer not in the set of used colors
    int candidateColor = 0;
    while (true) {
      if (neighborColors.find(candidateColor) == neighborColors.end()) {
        return candidateColor;
      }
      candidateColor++;
    }
  }

  /**
   * @brief Colors the graph using a parallel greedy algorithm
   * 
   * Performs graph coloring in three phases:
   * 1. Initial parallel coloring assignment
   * 2. Conflict detection and resolution
   * 3. Color optimization to reduce the total number of colors
   * 
   * @param adjacencyList The graph structure
   * @param vertexColors Map to store the assigned colors (output parameter)
   */
  void colorGraph(std::unordered_map<graphNode, std::vector<graphNode>>& adjacencyList,
                  std::unordered_map<graphNode, color>& vertexColors) {
    int vertexCount = static_cast<int>(adjacencyList.size());
    
    // Phase 1: Initialize all vertices with an uncolored state (-1)
    for (int i = 0; i < vertexCount; i++) {
      vertexColors[i] = -1;
    }
    
    // Phase 2: Perform initial parallel coloring
    // Use dynamic scheduling with chunk size 12 for better load balancing
    #pragma omp parallel for schedule(dynamic, 12)
    for (int i = 0; i < vertexCount; i++) {
      int assignedColor = findMinimumAvailableColor(i, adjacencyList, vertexColors);
      vertexColors[i] = assignedColor;
    }
    
    // Find the current maximum color used (for potential conflict resolution)
    int totalColors = 0;
    for (int i = 0; i < vertexCount; i++) {
      totalColors = std::max(totalColors, vertexColors[i] + 1);
    }
    
    // Phase 3: Resolve coloring conflicts that may have occurred during parallel execution
    #pragma omp parallel for shared(adjacencyList, vertexColors, totalColors)
    for (int i = 0; i < vertexCount; i++) {
      int vertexColor = vertexColors[i];
      
      // Check if this vertex has the same color as any of its neighbors
      for (auto& neighbor : adjacencyList[i]) {
        if (vertexColor == vertexColors[neighbor]) {
          // Conflict detected - assign a new unique color
          // Use atomic operation to prevent race conditions when updating totalColors
          #pragma omp atomic capture
          vertexColors[i] = totalColors++;
          break;
        }
      }
    }
    
    // Phase 4: Optimize coloring by reducing colors where possible
    #pragma omp parallel for shared(adjacencyList, vertexColors)
    for (int i = 0; i < vertexCount; i++) {
      int largestNeighbor = -1;
      int highestNeighborColor = -1;
      
      // Find the highest color among neighbors
      for (auto& neighbor : adjacencyList[i]) {
        largestNeighbor = std::max(largestNeighbor, neighbor);
        highestNeighborColor = std::max(highestNeighborColor, vertexColors[neighbor]);
      }
      
      // If all neighbors have lower indices and colors, we can optimize
      // by using a color just above the highest neighbor color
      if (largestNeighbor < i && highestNeighborColor < vertexColors[i]) {
        vertexColors[i] = highestNeighborColor + 1;
      }
    }
  }
};

std::unique_ptr<ColorGraph> createBasicParallelColorGraph() {
  return std::make_unique<BasicParallelColorGraph>();
}