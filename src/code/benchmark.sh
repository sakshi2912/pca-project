#!/bin/bash
# benchmark.sh - Script to run benchmarks on graph coloring algorithms

# Create directories if they don't exist
mkdir -p graphs
mkdir -p results

# Define thread counts to test
THREAD_COUNTS=(1 2 4 8 16 32 64 128 256)

# Generate test graphs of different sizes and types
echo "Generating test graphs..."

# Random graphs with different sizes and densities
python3 graph_generator.py random 2000 0.1 graphs/random_2000_01.graph
python3 graph_generator.py random 2000 0.5 graphs/random_2000_05.graph
python3 graph_generator.py random 2000 0.01 graphs/random_2000_001.graph
python3 graph_generator.py random 5000 0.005 graphs/random_5000_0005.graph

# Grid graphs
python3 graph_generator.py grid 50 50 graphs/grid_50x50.graph
python3 graph_generator.py grid 100 100 graphs/grid_100x100.graph
python3 graph_generator.py grid 500 500 graphs/grid_500x500.graph

# Scale-free graphs
python3 graph_generator.py scale-free 1000 3 graphs/scalefree_1000_3.graph
python3 graph_generator.py scale-free 2000 5 graphs/scalefree_2000_5.graph
python3 graph_generator.py scale-free 5000 8 graphs/scalefree_5000_8.graph

# Complete graphs
python3 graph_generator.py complete 100 graphs/complete_100.graph
python3 graph_generator.py complete 500 graphs/complete_500.graph

# Bipartite graphs
python3 graph_generator.py bipartite 100 100 0.5 graphs/bipartite_100_100_05.graph
python3 graph_generator.py bipartite 500 500 0.1 graphs/bipartite_500_500_01.graph

# Small-world graphs
python3 graph_generator.py small-world 100 4 0.1 graphs/smallworld_100_4_01.graph
python3 graph_generator.py small-world 1000 6 0.1 graphs/smallworld_1000_6_01.graph

echo "Graph generation complete."

# Run the sequential algorithm on all graphs
echo -e "\nRunning sequential algorithm..."
for graph in graphs/*.graph; do
    base=$(basename "$graph" .graph)
    echo "Processing $base"
    ./graphcolor_seq "$graph" "results/${base}.seq.result"
done

# Run the OpenMP algorithm with different thread counts
for threads in "${THREAD_COUNTS[@]}"; do
    echo -e "\nRunning OpenMP algorithm with $threads threads..."
    for graph in graphs/*.graph; do
        base=$(basename "$graph" .graph)
        echo "Processing $base"
        ./graphcolor_omp "$graph" "results/${base}.omp${threads}.result" $threads
    done
done

# Create a benchmark report
echo -e "\nCreating benchmark report..."
echo "========================== BENCHMARK SUMMARY ==========================" > results/benchmark_summary.txt
echo "Executed on $(date)" >> results/benchmark_summary.txt
echo "Machine: $(uname -a)" >> results/benchmark_summary.txt
echo -e "\nGRAPH COLORING PERFORMANCE RESULTS\n" >> results/benchmark_summary.txt

# Create the performance summary table
echo "COMPUTATION TIME (seconds)" >> results/benchmark_summary.txt
printf "%-25s %-15s" "Graph" "Sequential" >> results/benchmark_summary.txt
for threads in "${THREAD_COUNTS[@]}"; do
    printf " %-15s" "$threads Threads" >> results/benchmark_summary.txt
done
echo >> results/benchmark_summary.txt
echo "-----------------------------------------------------------------------------------------------------------------------------------" >> results/benchmark_summary.txt

for graph in graphs/*.graph; do
    base=$(basename "$graph" .graph)
    seq_compute=$(grep "# Computation time:" "results/${base}.seq.result" 2>/dev/null | awk '{print $4}')
    seq_compute=${seq_compute:-"N/A"}
    printf "%-25s %-15s" "$base" "$seq_compute" >> results/benchmark_summary.txt
    for threads in "${THREAD_COUNTS[@]}"; do
        compute=$(grep "# Computation time:" "results/${base}.omp${threads}.result" 2>/dev/null | awk '{print $4}' || echo "N/A")
        printf " %-15s" "$compute" >> results/benchmark_summary.txt
    done
    echo >> results/benchmark_summary.txt
done

# Create SPEEDUP 
# todo

# Create the colors table
echo -e "\nNUMBER OF COLORS USED" >> results/benchmark_summary.txt
printf "%-25s %-15s" "Graph" "Sequential" >> results/benchmark_summary.txt
for threads in "${THREAD_COUNTS[@]}"; do
    printf " %-15s" "$threads Threads" >> results/benchmark_summary.txt
done
echo >> results/benchmark_summary.txt
echo "-----------------------------------------------------------------------------------------------------------------------------------" >> results/benchmark_summary.txt

for graph in graphs/*.graph; do
    base=$(basename "$graph" .graph)
    seq_colors=$(grep "# Colors used:" "results/${base}.seq.result" 2>/dev/null | awk '{print $4}')
    seq_colors=${seq_colors:-"N/A"}
    printf "%-25s %-15s" "$base" "$seq_colors" >> results/benchmark_summary.txt
    for threads in "${THREAD_COUNTS[@]}"; do
        colors=$(grep "# Colors used:" "results/${base}.omp${threads}.result" 2>/dev/null | awk '{print $4}' || echo "N/A")
        printf " %-15s" "$colors" >> results/benchmark_summary.txt
    done
    echo >> results/benchmark_summary.txt
done

echo -e "\nResults saved to results/benchmark_summary.txt"
echo "Benchmark complete."
