#include "graph.h"
#include <atomic>
#include <vector>
#include <unordered_set>
#include <algorithm>
class TransactionalColorGraph : public ColorGraph {
private:
    struct VertexState {
        std::atomic<color> current_color;
        std::atomic<bool> in_conflict;
        
        VertexState() : current_color(-1), in_conflict(false) {}
    };

public:
    void buildGraph(std::vector<graphNode> &nodes, std::vector<std::pair<int, int>> &pairs,
                   std::unordered_map<graphNode, std::vector<graphNode>> &graph) override {
        for (auto &node : nodes) graph[node] = {};
        for (auto &edge : pairs) {
            graph[edge.first].push_back(edge.second);
            graph[edge.second].push_back(edge.first);
        }
    }

    void colorGraph(std::unordered_map<graphNode, std::vector<graphNode>> &graph,
                   std::unordered_map<graphNode, color> &colors) override {
        const int numNodes = static_cast<int>(graph.size());
        std::vector<VertexState> vertex_states(numNodes);
        std::atomic<int> max_color{0};

        // Phase 1: Optimistic coloring with degree ordering
        std::vector<int> ordered_vertices(numNodes);
        for (int i = 0; i < numNodes; i++) ordered_vertices[i] = i;
        
        // Sort by degree (descending)
        std::sort(ordered_vertices.begin(), ordered_vertices.end(),
            [&graph](int a, int b) { return graph[a].size() > graph[b].size(); });

        #pragma omp parallel for schedule(static)
        for (int idx = 0; idx < numNodes; idx++) {
            const int u = ordered_vertices[idx];
            std::vector<bool> forbidden(max_color + 1, false);
            
            for (const auto &nbor : graph[u]) {
                color c = vertex_states[nbor].current_color.load(std::memory_order_relaxed);
                if (c != -1 && c < forbidden.size()) forbidden[c] = true;
            }

            color selected = 0;
            while (selected < forbidden.size() && forbidden[selected]) selected++;
            
            if (selected == forbidden.size()) {
                #pragma omp critical
                {
                    selected = max_color.fetch_add(1, std::memory_order_relaxed) + 1;
                }
            }
            
            vertex_states[u].current_color.store(selected, std::memory_order_relaxed);
        }

        // Phase 2: Conflict resolution with guaranteed correctness
        bool has_conflicts = true;
        int iterations = 0;
        const int max_iterations = 5;
        
        while (has_conflicts && iterations++ < max_iterations) {
            has_conflicts = false;
            
            // Reset conflict flags
            #pragma omp parallel for schedule(static)
            for (int i = 0; i < numNodes; i++) {
                vertex_states[i].in_conflict.store(false, std::memory_order_relaxed);
            }

            // Detect conflicts
            #pragma omp parallel for schedule(static) reduction(||:has_conflicts)
            for (int u = 0; u < numNodes; u++) {
                color u_color = vertex_states[u].current_color.load(std::memory_order_relaxed);
                for (const auto &v : graph[u]) {
                    if (v > u) continue; // Check each edge once
                    
                    color v_color = vertex_states[v].current_color.load(std::memory_order_relaxed);
                    if (u_color == v_color) {
                        // Higher degree vertex keeps color (or higher ID if equal degree)
                        if (graph[u].size() > graph[v].size() || 
                           (graph[u].size() == graph[v].size() && u > v)) {
                            vertex_states[v].in_conflict.store(true, std::memory_order_relaxed);
                        } else {
                            vertex_states[u].in_conflict.store(true, std::memory_order_relaxed);
                        }
                        has_conflicts = true;
                    }
                }
            }

            // Resolve conflicts
            if (has_conflicts) {
                #pragma omp parallel for schedule(dynamic, 64)
                for (int u = 0; u < numNodes; u++) {
                    if (vertex_states[u].in_conflict.load(std::memory_order_relaxed)) {
                        std::vector<bool> forbidden(max_color + 1, false);
                        for (const auto &v : graph[u]) {
                            color c = vertex_states[v].current_color.load(std::memory_order_relaxed);
                            if (c != -1 && c < forbidden.size()) forbidden[c] = true;
                        }

                        color new_color = 0;
                        while (new_color < forbidden.size() && forbidden[new_color]) new_color++;
                        
                        if (new_color == forbidden.size()) {
                            #pragma omp critical
                            {
                                new_color = max_color.fetch_add(1, std::memory_order_relaxed) + 1;
                            }
                        }
                        
                        vertex_states[u].current_color.store(new_color, std::memory_order_relaxed);
                    }
                }
            }
        }

        // Write final colors
        for (int i = 0; i < numNodes; i++) {
            colors[i] = vertex_states[i].current_color.load(std::memory_order_relaxed);
        }
    }
};

std::unique_ptr<ColorGraph> createTransactionalColorGraph() {
    return std::make_unique<TransactionalColorGraph>();
}