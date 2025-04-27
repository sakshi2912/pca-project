// graph_txn.h
#ifndef GRAPH_TXN_H
#define GRAPH_TXN_H

#include <vector>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <iostream>

class Graph {
private:
    int num_vertices;
    int num_edges;
    std::vector<std::vector<int>> adjacency_lists;

public:
    // Constructor for an empty graph with given number of vertices
    Graph(int vertices) : num_vertices(vertices), num_edges(0) {
        adjacency_lists.resize(vertices);
    }
    
    // Add an edge between vertices u and v
    void addEdge(int u, int v) {
        if (u < 0 || u >= num_vertices || v < 0 || v >= num_vertices) {
            throw std::out_of_range("Vertex index out of range");
        }
        
        // Check if the edge already exists
        if (std::find(adjacency_lists[u].begin(), adjacency_lists[u].end(), v) == adjacency_lists[u].end()) {
            adjacency_lists[u].push_back(v);
            adjacency_lists[v].push_back(u);
            num_edges++;
        }
    }
    
    // Get the neighbors of a vertex
    const std::vector<int>& getNeighbors(int vertex) const {
        if (vertex < 0 || vertex >= num_vertices) {
            throw std::out_of_range("Vertex index out of range");
        }
        return adjacency_lists[vertex];
    }
    
    // Get the number of vertices
    int numVertices() const {
        return num_vertices;
    }
    
    // Get the number of edges
    int numEdges() const {
        return num_edges;
    }
};

// Function declaration for graph loading
Graph loadGraphFromFile(const std::string& filename);

#endif // GRAPH_TXN_H
