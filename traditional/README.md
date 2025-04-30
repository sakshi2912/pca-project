# Traditional Graph Coloring

This project implements four different parallel graph coloring approaches, along with a sequential baseline for comparison. Graph coloring is a technique where each vertex in a graph is assigned a color such that no adjacent vertices share the same color.

## Implemented Approaches

1. **Basic Parallel Graph Coloring** (`trad_1`): A straightforward parallelization of the greedy coloring algorithm using OpenMP.

2. **Speculative Graph Coloring** (`trad_2`): Uses Jones-Plassmann ordering with random weights and speculative execution for better parallel performance.

3. **Work-Stealing Graph Coloring** (`trad_3`): Implements a dynamic work-stealing scheduler with distance-2 coloring for improved load balancing.

4. **High-Performance Graph Coloring** (`trad_4`): Uses degree-based vertex ordering, thread partitioning, and optimized conflict resolution.

5. **Sequential Baseline** (`seq`): A single-threaded implementation for comparison purposes.

## Building the Project

To build the project, use the provided Makefile:

```bash
make

# Running the executable

./traditional_graph_coloring -f input.txt -seq

./traditional_graph_coloring -f input.txt -trad_1

./traditional_graph_coloring -f input.txt -trad_2

./traditional_graph_coloring -f input.txt -trad_3

./traditional_graph_coloring -f input.txt -trad_4