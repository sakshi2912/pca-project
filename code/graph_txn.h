// graph_txn.h
#ifndef GRAPH_TXN_H
#define GRAPH_TXN_H

#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <memory>
#include <utility>

class Graph {
private:
    int num_vertices;
    int num_edges;
    std::vector<std::vector<int>> adjacency_lists;

public:
    // Constructor with safe initialization
    explicit Graph(int vertices) : num_vertices(vertices), num_edges(0) {
        if (vertices <= 0) {
            throw std::invalid_argument("Number of vertices must be positive");
        }
        
        // Pre-allocate with safe size
        adjacency_lists.resize(vertices);
    }
    
    // Reserve space for edges with bounds checking
    void reserveEdges(int avg_degree) {
        if (avg_degree <= 0) return;
        
        // Cap the reservation to avoid excessive memory usage
        int safe_degree = std::min(avg_degree, 1000);
        
        for (auto& adj : adjacency_lists) {
            adj.reserve(safe_degree);
        }
    }
    
    // Safe edge addition with bounds check
    void addEdge(int u, int v) {
        if (u < 0 || u >= num_vertices || v < 0 || v >= num_vertices) {
            throw std::out_of_range("Vertex index out of range");
        }
        
        adjacency_lists[u].push_back(v);
        if (u != v) { // Handle self-loops
            adjacency_lists[v].push_back(u);
        }
        num_edges++;
    }
    
    // Get neighbors with bounds checking
    const std::vector<int>& getNeighbors(int vertex) const {
        if (vertex < 0 || vertex >= num_vertices) {
            throw std::out_of_range("Vertex index out of range");
        }
        return adjacency_lists[vertex];
    }
    
    // Basic getters
    int numVertices() const { return num_vertices; }
    int numEdges() const { return num_edges; }
    
    // Optimize the graph safely
    void optimize() {
        #pragma omp parallel for schedule(dynamic, 64)
        for (int i = 0; i < num_vertices; i++) {
            std::sort(adjacency_lists[i].begin(), adjacency_lists[i].end());
        }
    }
};

// Function declaration for graph loading
Graph loadGraphFromFile(const std::string& filename);

#endif // GRAPH_TXN_H