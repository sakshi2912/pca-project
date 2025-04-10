/**
 * graphcolor_seq.cpp
 * Sequential implementation of greedy graph coloring
 */

 #include <iostream>
 #include <fstream>
 #include <vector>
 #include <string>
 #include <chrono>
 #include <algorithm>
 #include "cycleTimer.h"

// Graph class using STL containers
class Graph {
private:
    int num_vertices;
    int num_edges;
    std::vector<std::vector<int>> adjacency_list;

public:
    Graph(int vertices) : num_vertices(vertices), num_edges(0) {
        adjacency_list.resize(vertices);
    }

    void addEdge(int u, int v) {
        // Ensure valid vertices
        if (u < 0 || u >= num_vertices || v < 0 || v >= num_vertices) {
            std::cerr << "Invalid edge: (" << u << ", " << v << ")" << std::endl;
            return;
        }

        // Add undirected edge
        adjacency_list[u].push_back(v);
        adjacency_list[v].push_back(u);
        num_edges++;
    }

    int getNumVertices() const {
        return num_vertices;
    }

    int getNumEdges() const {
        return num_edges;
    }

    const std::vector<int>& getNeighbors(int vertex) const {
        return adjacency_list[vertex];
    }

    // Static method to read graph from file
    static Graph readFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error opening file: " << filename << std::endl;
            exit(1);
        }

        int num_vertices, num_edges;
        file >> num_vertices >> num_edges;

        Graph graph(num_vertices);

        int u, v;
        while (file >> u >> v) {
            // Check if file uses 1-based indexing
            if (u > 0 && v > 0 && u <= num_vertices && v <= num_vertices) {
                // Adjust for 0-based indexing in our implementation
                u--;
                v--;
            }
            graph.addEdge(u, v);
        }

        file.close();
        return graph;
    }
};

// Coloring class to manage vertex coloring
class Coloring {
    private:
        std::vector<int> colors;
        int max_color;
    
    public:
        Coloring(int num_vertices) : colors(num_vertices, -1), max_color(-1) {}
    
        void setColor(int vertex, int color) {
            colors[vertex] = color;
            max_color = std::max(max_color, color);
        }
    
        int getColor(int vertex) const {
            return colors[vertex];
        }
    
        int getNumColors() const {
            return max_color + 1;
        }
    
        const std::vector<int>& getColors() const {
            return colors;
        }
    
        // Write coloring to file
        void writeToFile(const std::string& filename, double init_time, double compute_time, double total_time) const {
            // Create directory if it doesn't exist
            size_t lastSlash = filename.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                std::string directory = filename.substr(0, lastSlash);
                // Create directory command - works on Linux/macOS
                std::string command = "mkdir -p " + directory;
                int result = system(command.c_str());
                if (result != 0) {
                    std::cerr << "Warning: Failed to create directory: " << directory << std::endl;
                }
            }
            
            std::ofstream file(filename);
            if (!file.is_open()) {
                std::cerr << "Error opening output file: " << filename << std::endl;
                return;
            }
    
            file << "# Graph coloring timing results\n";
            file << "# Initialization time: " << init_time << " seconds\n";
            file << "# Computation time: " << compute_time << " seconds\n";
            file << "# Total execution time: " << total_time << " seconds\n";
            file << "# Vertices: " << colors.size() << "\n";
            file << "# Colors used: " << getNumColors() << "\n";
            file << "# Format: vertex_id color\n";
    
            for (size_t i = 0; i < colors.size(); i++) {
                file << i << " " << colors[i] << "\n";
            }
    
            file.close();
        }
    
        // Print coloring statistics
        void printStats() const {
            std::cout << "Number of colors used: " << getNumColors() << std::endl;
    
            // For small graphs, print the full coloring
            // if (colors.size() <= 100) {
            //     std::cout << "Vertex coloring:" << std::endl;
            //     for (size_t i = 0; i < colors.size(); i++) {
            //         std::cout << "Vertex " << i << ": Color " << colors[i] << std::endl;
            //     }
            // }
        }
};


// Sequential greedy coloring algorithm
Coloring greedyColor(const Graph& graph) {
    int num_vertices = graph.getNumVertices();
    Coloring coloring(num_vertices);

    // Process vertices in order
    for (int u = 0; u < num_vertices; u++) {
        // Track which colors are used by neighbors
        std::vector<bool> used_colors(num_vertices, false);

        // Check colors of adjacent vertices
        const std::vector<int>& neighbors = graph.getNeighbors(u);
        for (int v : neighbors) {
            int color = coloring.getColor(v);
            if (color != -1) {
                used_colors[color] = true;
            }
        }

        // Find the first unused color
        int color = 0;
        while (color < num_vertices && used_colors[color]) {
            color++;
        }

        // Assign the color
        coloring.setColor(u, color);
    }

    return coloring;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <graph_file> [output_file]" << std::endl;
        return 1;
    }

    std::string graph_file = argv[1];
    std::string output_file = (argc > 2) ? argv[2] : "";

    // Initialize timers
    CycleTimer total_timer;
    CycleTimer init_timer;
    CycleTimer computation_timer;
    
    // Start total time measurement
    total_timer.startTimer();
    
    // Start initialization time measurement
    init_timer.startTimer();
    
    Graph graph = Graph::readFromFile(graph_file);
    
    // End initialization time measurement
    init_timer.stopTimer();
    
    std::cout << "Graph loaded: " << graph.getNumVertices() << " vertices, " << graph.getNumEdges() << " edges" << std::endl;
    std::cout << "Initialization time: " << init_timer.getElapsedTime() << " seconds" << std::endl;

    // Start computation time measurement
    computation_timer.startTimer();
    
    Coloring coloring = greedyColor(graph);
    
    computation_timer.stopTimer();
    total_timer.stopTimer();

    // Print results
    double computation_time = computation_timer.getElapsedTime();
    double total_time = total_timer.getElapsedTime();
    
    std::cout << "Greedy coloring completed in " << computation_time << " seconds" << std::endl;
    std::cout << "Total execution time: " << total_time << " seconds" << std::endl;
    std::cout << "Time breakdown: Initialization: " << init_timer.getElapsedTime() 
              << "s (" << (init_timer.getElapsedTime() / total_time * 100.0) << "%), "
              << "Computation: " << computation_time 
              << "s (" << (computation_time / total_time * 100.0) << "%)" << std::endl;
    
    coloring.printStats();

    // Write results to file 
    if (!output_file.empty()) {
        coloring.writeToFile(output_file, init_timer.getElapsedTime(), computation_time, total_time);
        std::cout << "Coloring written to " << output_file << std::endl;
    }

    return 0;
}