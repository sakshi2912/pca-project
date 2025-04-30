#ifndef GRAPH_H
#define GRAPH_H

#include <memory>
#include <unordered_map>
#include <vector>

typedef int graphNode;
typedef int color;

class ColorGraph {
public:
  virtual void buildGraph(std::vector<graphNode> &nodes,
                          std::vector<std::pair<int, int>> &pairs,
                          std::unordered_map<graphNode, std::vector<graphNode>> &graph) = 0;
  virtual void colorGraph(std::unordered_map<graphNode, std::vector<graphNode>> &graph,
                          std::unordered_map<graphNode, color> &colors) = 0;
  virtual ~ColorGraph() = default;
};

// Function declarations for different implementations
std::unique_ptr<ColorGraph> createSeqColorGraph();
std::unique_ptr<ColorGraph> createBasicParallelColorGraph();
std::unique_ptr<ColorGraph> createSpeculativeGraphColoring();
std::unique_ptr<ColorGraph> createWorkStealingColorGraph();
std::unique_ptr<ColorGraph> createHighPerformanceColorGraph();
#endif // GRAPH_H