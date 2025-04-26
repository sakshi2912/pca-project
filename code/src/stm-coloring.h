#ifndef STM_COLORING_H
#define STM_COLORING_H

#include "graph.h"
#include <vector>
#include <unordered_map>
#include <atomic>

// Maximum number of colors to use
#define MAX_COLORS 5000
enum class STMType {
    Libitm,
    TL2,
};

// Data structure for a vertex in the graph
struct VertexData {
    color current_color;
    std::atomic<int> status; // 0=uncolored, 1=tentative, 2=final
    
    VertexData() : current_color(-1), status(0) {}
};

class STMColorGraph : public ColorGraph {
private:
void repairConflicts(
    std::unordered_map<graphNode, std::vector<graphNode>> &graph,
    std::unordered_map<graphNode, color> &colors,
    const std::unordered_map<graphNode, size_t> &node_to_index,
    const std::vector<graphNode> &ordered_nodes);

public:
    STMColorGraph(STMType type, int iterations, bool try_bipartite, int num_threads=0);
    virtual ~STMColorGraph();
    
    virtual void buildGraph(
        std::vector<graphNode>& nodes,
        std::vector<std::pair<graphNode, graphNode>>& pairs,
        std::unordered_map<graphNode, std::vector<graphNode>>& graph) override;
    
    virtual void colorGraph(
        std::unordered_map<graphNode, std::vector<graphNode>>& graph,
        std::unordered_map<graphNode, color>& colors) override;

protected:
    STMType stm_type;
    int max_iterations;
    bool detect_bipartite;
    color global_max_color;
    int num_threads;

    
    // Specialized coloring methods for different graph types
    void colorSparseGraph(
        std::unordered_map<graphNode, std::vector<graphNode>>& graph,
        std::unordered_map<graphNode, color>& colors);
        
    void colorLockFreeGraph(
        std::unordered_map<graphNode, std::vector<graphNode>>& graph,
        std::unordered_map<graphNode, color>& colors);
};

class LibITMColorGraph : public STMColorGraph {
public:
    LibITMColorGraph(int iterations, bool try_bipartite, int num_threads);
    
private:
    void optimisticColoring(size_t vertex,
                           const std::vector<std::vector<graphNode>>& neighbors,
                           std::vector<VertexData>& vertex_data);
    
    bool detectConflicts(const std::vector<std::vector<graphNode>>& neighbors,
                        std::vector<VertexData>& vertex_data);
    
    void resolveConflicts(const std::vector<std::vector<graphNode>>& neighbors,
                         std::vector<VertexData>& vertex_data);
};

class TL2ColorGraph : public STMColorGraph {
public:
    TL2ColorGraph(int iterations, bool try_bipartite, int num_threads=0);
    
private:
    void optimisticColoring(size_t vertex,
                           const std::vector<std::vector<graphNode>>& neighbors,
                           std::vector<VertexData>& vertex_data);
    
    bool detectConflicts(const std::vector<std::vector<graphNode>>& neighbors,
                        std::vector<VertexData>& vertex_data);
    
    void resolveConflicts(const std::vector<std::vector<graphNode>>& neighbors,
                         std::vector<VertexData>& vertex_data);
};

// Factory function to create the appropriate STM implementation
std::unique_ptr<ColorGraph> createSTMColorGraph(const char* stm_type, int iterations, bool try_bipartite, int num_threads);

#endif // STM_COLORING_H