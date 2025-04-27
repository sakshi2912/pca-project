// graph_txn.cpp
#include "graph_txn.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <iostream>

// Constants
constexpr size_t READ_BUFFER_SIZE = 1024 * 1024; // 1MB buffer for file reading

Graph loadGraphFromFile(const std::string& filename) {
    std::vector<int> nodes;
    std::vector<std::pair<int, int>> edges;
    std::unordered_map<int, int> node_to_index;
    
    std::cout << "Reading graph from file: " << filename << std::endl;
    
    // Use memory-mapped IO for large files
    struct stat sb;
    if (stat(filename.c_str(), &sb) == -1) {
        std::cerr << "Error: Cannot stat file " << filename << std::endl;
        exit(1);
    }
    
    // Map original node IDs to consecutive indices starting from 0
    int next_index = 0;
    int max_node_id = -1;
    
    if (sb.st_size > 100 * 1024 * 1024) { // Use mmap for files > 100MB
        std::cout << "Using memory-mapped I/O for large file" << std::endl;
        int fd = open(filename.c_str(), O_RDONLY);
        if (fd == -1) {
            std::cerr << "Error: Cannot open file " << filename << std::endl;
            exit(1);
        }
        
        void* mapped = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapped == MAP_FAILED) {
            close(fd);
            std::cerr << "Error: Cannot mmap file " << filename << std::endl;
            exit(1);
        }
        
        const char* fileContent = static_cast<const char*>(mapped);
        const char* end = fileContent + sb.st_size;
        const char* lineStart = fileContent;
        
        // Pre-allocate based on file size estimate
        size_t estLines = sb.st_size / 20; // Rough estimate
        nodes.reserve(estLines / 3);
        edges.reserve(estLines * 2 / 3);
        
        while (lineStart < end) {
            const char* lineEnd = lineStart;
            while (lineEnd < end && *lineEnd != '\n') lineEnd++;
            
            std::string line(lineStart, lineEnd);
            if (!line.empty() && line[0] != '#' && line[0] != '%') {
                std::istringstream iss(line);
                int n1, n2;
                if (iss >> n1 >> n2) {
                    // Register nodes if they haven't been seen before
                    if (node_to_index.find(n1) == node_to_index.end()) {
                        node_to_index[n1] = next_index++;
                        nodes.push_back(n1);
                        max_node_id = std::max(max_node_id, n1);
                    }
                    
                    if (node_to_index.find(n2) == node_to_index.end()) {
                        node_to_index[n2] = next_index++;
                        nodes.push_back(n2);
                        max_node_id = std::max(max_node_id, n2);
                    }
                    
                    edges.emplace_back(n1, n2);
                }
            }
            
            lineStart = lineEnd + 1;
            if (lineStart >= end) break;
        }
        
        munmap(mapped, sb.st_size);
        close(fd);
    } else {
        // For smaller files, use buffered IO
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file " << filename << std::endl;
            exit(1);
        }
        
        // Pre-allocate vectors based on file size
        size_t estLines = sb.st_size / 20;
        nodes.reserve(estLines / 3);
        edges.reserve(estLines * 2 / 3);
        
        std::string line;
        char buffer[READ_BUFFER_SIZE];
        
        while (file.getline(buffer, READ_BUFFER_SIZE)) {
            line = buffer;
            if (!line.empty() && line[0] != '#' && line[0] != '%') {
                std::istringstream iss(line);
                int n1, n2;
                if (iss >> n1 >> n2) {
                    // Register nodes if they haven't been seen before
                    if (node_to_index.find(n1) == node_to_index.end()) {
                        node_to_index[n1] = next_index++;
                        nodes.push_back(n1);
                        max_node_id = std::max(max_node_id, n1);
                    }
                    
                    if (node_to_index.find(n2) == node_to_index.end()) {
                        node_to_index[n2] = next_index++;
                        nodes.push_back(n2);
                        max_node_id = std::max(max_node_id, n2);
                    }
                    
                    edges.emplace_back(n1, n2);
                }
            }
        }
        
        file.close();
    }
    
    // Create the graph with the correct number of vertices
    int num_vertices = node_to_index.size();
    Graph graph(num_vertices);
    
    std::cout << "Found " << num_vertices << " vertices and " << edges.size() << " edges" << std::endl;
    
    // Add edges to the graph, translating to indices
    for (const auto& edge : edges) {
        int u = node_to_index[edge.first];
        int v = node_to_index[edge.second];
        
        try {
            graph.addEdge(u, v);
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to add edge (" << u << ", " << v << "): " 
                      << e.what() << std::endl;
        }
    }
    
    std::cout << "Graph loaded successfully" << std::endl;
    return graph;
}
