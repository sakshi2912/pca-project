// graph_txn.cpp
#include "graph_txn.h"
#include <fstream>
#include <string>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <omp.h>
#include <limits>
#include <sstream>
#include <stdexcept>

// Safe node mapper with bounds checking
class NodeMapper {
private:
    std::vector<int> indices;
    int next_index;
    
public:
    explicit NodeMapper(int estimated_size) : next_index(0) {
        // Limit maximum size to avoid excessive memory usage
        int safe_size = std::min(estimated_size, 100000000);
        indices.resize(safe_size, -1);
    }
    
    // Map node ID to index with bounds checking
    int getOrCreate(int node_id) {
        // Ensure node_id is non-negative
        if (node_id < 0) {
            throw std::invalid_argument("Node ID cannot be negative");
        }
        
        // Resize if needed, with safety limits
        if (node_id >= static_cast<int>(indices.size())) {
            size_t new_size = std::min(
                std::max(static_cast<size_t>(node_id + 1), indices.size() * 2),
                static_cast<size_t>(std::numeric_limits<int>::max())
            );
            indices.resize(new_size, -1);
        }
        
        // Create mapping if needed
        if (indices[node_id] == -1) {
            indices[node_id] = next_index++;
        }
        
        return indices[node_id];
    }
    
    int count() const { return next_index; }
};

bool safeParseInt(const char* str, int& result) {
    if (!str || *str == '\0') return false;
    
    char* end;
    long val = strtol(str, &end, 10);
    
    // Check for conversion errors
    if (end == str || *end != '\0') return false;
    
    // Check for overflow/underflow
    if (val > std::numeric_limits<int>::max() || 
        val < std::numeric_limits<int>::min()) {
        return false;
    }
    
    result = static_cast<int>(val);
    return true;
}

bool processLine(const std::string& line, int& n1, int& n2) {
    // Skip comments and empty lines
    if (line.empty() || line[0] == '#' || line[0] == '%') {
        return false;
    }
    
    // Use string stream for safer parsing
    std::istringstream iss(line);
    if (!(iss >> n1 >> n2)) {
        return false;
    }
    
    return true;
}

Graph loadGraphFromFile(const std::string& filename) {
    try {
        // Get file information
        struct stat sb;
        if (stat(filename.c_str(), &sb) == -1) {
            throw std::runtime_error("Cannot stat file: " + filename);
        }
        
        // Prepare for edge collection
        std::vector<std::pair<int, int>> edges;
        size_t est_lines = sb.st_size / 20; // Rough estimate
        edges.reserve(std::min(est_lines, static_cast<size_t>(10000000)));
        
        int max_node_id = -1;
        
        // Standard file IO (safer than memory mapping)
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }
        
        // Process file line by line
        std::string line;
        while (std::getline(file, line)) {
            int n1, n2;
            if (processLine(line, n1, n2)) {
                // Validate node IDs
                if (n1 < 0 || n2 < 0) {
                    std::cerr << "Warning: Skipping negative node ID in line: " << line << std::endl;
                    continue;
                }
                
                edges.emplace_back(n1, n2);
                max_node_id = std::max(max_node_id, std::max(n1, n2));
                
                // Safety check for very large graphs
                if (edges.size() >= 100000000) {
                    std::cerr << "Warning: Graph is very large, truncating at 100M edges" << std::endl;
                    break;
                }
            }
        }
        
        // Create node mapper with safe size estimate
        NodeMapper mapper(max_node_id + 1);
        
        // Map node IDs to consecutive indices
        for (auto& edge : edges) {
            edge.first = mapper.getOrCreate(edge.first);
            edge.second = mapper.getOrCreate(edge.second);
        }
        
        // Create graph with proper vertex count
        int vertex_count = mapper.count();
        Graph graph(vertex_count);
        
        // Calculate safe average degree
        double avg_degree = 2.0 * edges.size() / vertex_count;
        int reserve_size = std::min(static_cast<int>(avg_degree * 1.1), 1000);
        graph.reserveEdges(reserve_size);
        
        std::cout << "Found " << vertex_count << " vertices and " << edges.size() << " edges" << std::endl;
        
        // Add all edges to graph
        for (const auto& edge : edges) {
            graph.addEdge(edge.first, edge.second);
        }
        
        // Optimize graph
        graph.optimize();
        
        return graph;
    }
    catch (const std::exception& e) {
        std::cerr << "Error loading graph: " << e.what() << std::endl;
        // Return an empty graph as fallback
        return Graph(1);
    }
}