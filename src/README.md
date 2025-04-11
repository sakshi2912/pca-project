# Parallel Graph Coloring

This repository contains implementations of graph coloring algorithms for the parallel computing architecture project.

## Project Structure

- `sequential_coloring.cpp`: Sequential implementation of greedy graph coloring
- `openmp_coloring.cpp`: OpenMP parallelized implementation using independent set approach
- `graph_generator.py`: Python script to generate various types of test graphs
- `Makefile`: Build system for compiling and testing
- `benchmark.sh`: Script to run comprehensive benchmarks

## Building the Project

To build all executables:

```bash
make
```

## Generating Test Graphs

The project includes a Python script for generating test graphs of various types:

```bash
# Generate random graph with 100 vertices and edge probability 0.1
python3 graph_generator.py random 100 0.1 random_100_01.graph

# Generate 10x10 grid graph
python3 graph_generator.py grid 10 10 grid_10x10.graph

# Generate scale-free graph with 100 vertices and min 3 edges per new vertex
python3 graph_generator.py scale-free 100 3 scalefree_100_3.graph

# Generate complete graph with 50 vertices
python3 graph_generator.py complete 50 complete_50.graph

# Generate bipartite graph with 50 vertices in each partition and edge probability 0.3
python3 graph_generator.py bipartite 50 50 0.3 bipartite_50_50_03.graph

# Generate small-world graph with 100 vertices, mean degree 4, and rewiring probability 0.1
python3 graph_generator.py small-world 100 4 0.1 smallworld_100_4_01.graph
```

Or generate a standard set of test graphs:

```bash
make graphs
```

## Running the Algorithms

### Sequential Coloring

```bash
./graphcolor_seq <graph_file> [output_file]
```

### OpenMP Coloring

```bash
./graphcolor_omp <graph_file> [output_file] [num_threads]
```

## Running Benchmarks

To run comprehensive benchmarks across different graph types and thread counts:

```bash
./benchmark.sh
```

Results will be saved in the `results/` directory, with a summary in `results/summary.txt`.

## Implementation Details

### Sequential Greedy Algorithm

The sequential implementation uses a standard greedy approach where vertices are colored in order, choosing the smallest color that doesn't conflict with already colored neighbors.

### OpenMP Independent Set Approach

The OpenMP implementation uses an independent set approach:
1. Find vertices that can be colored simultaneously without conflicts
2. Color them with the same color
3. Repeat with remaining vertices

This approach effectively parallelizes the coloring process while maintaining correctness.

## File Format

The graph files use a simple edge list format:

```
<num_vertices> <num_edges>
<u1> <v1>
<u2> <v2>
...
```

Where each line after the header represents an undirected edge between vertices `u` and `v`.

## Project Goals

1. Implement sequential and OpenMP parallel graph coloring algorithms
2. Test performance and scalability across various graph types and sizes
3. Analyze the performance-quality tradeoff between coloring quality and speedup
4. Explore different parallelization strategies

Future work will include:
- Implementing additional parallel approaches, including domain decomposition
- Adding conflict resolution techniques for optimistic coloring
- Extending to distributed memory using MPI
