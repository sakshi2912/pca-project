#!/bin/bash

# Test files
FILES=("sparse-50000.txt" "random-5000.txt" "corner-50000.txt" "components-50000.txt")

# Define thread counts to test
THREADS=(1 2 4 8 16 32 64 128 256)

# Store results in memory
declare -A TIME_RESULTS
declare -A COLOR_RESULTS
declare -A CORRECT_RESULTS
declare -A CACHE_REFS
declare -A CACHE_MISSES
declare -A L1_MISSES
declare -A LLC_MISSES

# Function to run a test
run_test() {
    local threads=$1
    local file=$2
    
    echo "Running STM with $threads threads on $file"
    export OMP_NUM_THREADS=$threads
    
    # Temporary file for perf output
    perf_output=$(mktemp)
    
    # Run with perf to measure cache misses
    echo "Running perf stat..."
    perf stat -e cache-references,cache-misses,L1-dcache-load-misses,L1-dcache-store-misses,LLC-load-misses,LLC-store-misses \
              -o $perf_output \
              ./color-transactional -stm -t $threads -f $file 2>&1
    
    # Capture the actual program output
    program_output=$(./color-transactional -stm -t $threads -f $file 2>&1)
    echo "$program_output"
    
    # Try multiple patterns to extract time
    time_s=$(echo "$program_output" | awk '
        /Elapsed time:/ {print $3}
        /Time spent:/ {print $3}
        /Total time:/ {print $3}
        /Execution time:/ {print $3}
    ' | head -1)
    
    # Default to 0 if no time found
    if [ -z "$time_s" ]; then 
        time_s=0
        echo "WARNING: Could not detect time output format for $file with $threads threads"
        echo "Program output was:"
        echo "$program_output"
    fi
    
    # Parse perf metrics - handle commas in numbers
    cache_refs=$(grep "cache-references" $perf_output | awk '{print $1}' | tr -d ',')
    cache_misses=$(grep "cache-misses" $perf_output | awk '{print $1}' | tr -d ',')
    l1_load_misses=$(grep "L1-dcache-load-misses" $perf_output | awk '{print $1}' | tr -d ',')
    llc_misses=$(grep "LLC-load-misses" $perf_output | awk '{print $1}' | tr -d ',')
    
    # Store cache metrics
    CACHE_REFS["$threads-$file"]=${cache_refs:-0}
    CACHE_MISSES["$threads-$file"]=${cache_misses:-0}
    L1_MISSES["$threads-$file"]=${l1_load_misses:-0}
    LLC_MISSES["$threads-$file"]=${llc_misses:-0}
    
    # Extract colors
    colors=$(echo "$program_output" | grep -E "Colored with|Number of colors" | awk '{print $NF}' | head -1)
    if [ -z "$colors" ]; then colors="0"; fi
    
    # Check correctness
    if echo "$program_output" | grep -q "Failed"; then
        correctness="FAIL"
    else
        correctness="PASS"
    fi
    
    # Store results
    TIME_RESULTS["$threads-$file"]=$time_s
    COLOR_RESULTS["$threads-$file"]=$colors
    CORRECT_RESULTS["$threads-$file"]=$correctness
    
    # Print immediate feedback
    printf "  %-6s %-15s %-8s %-8s %-12s\n" "$threads" "$file" "$time_s" "$colors" "$correctness"
    
    rm $perf_output
}

# Header for console output
echo "==================== STM Graph Coloring Benchmark ===================="
printf "%-6s %-15s %-8s %-8s %-12s\n" "Threads" "File" "Time(s)" "Colors" "Correctness"
echo "===================================================================="

# Main test loop
for file in "${FILES[@]}"; do
    echo "Processing file: $file"
    
    # Run STM with different thread counts
    for threads in "${THREADS[@]}"; do
        run_test "$threads" "$file"
    done
    
    echo "-----------------------------------------------------------------"
done

# Generate summary tables
echo -e "\n\n=== Performance Summary ==="
for file in "${FILES[@]}"; do
    echo -e "\nResults for $file:"
    printf "%-6s %-8s %-8s %-12s\n" "Threads" "Time(s)" "Colors" "Correctness"
    
    for threads in "${THREADS[@]}"; do
        printf "%-6s %-8s %-8s %-12s\n" "$threads" "${TIME_RESULTS["$threads-$file"]}" "${COLOR_RESULTS["$threads-$file"]}" "${CORRECT_RESULTS["$threads-$file"]}"
    done
done

# Generate time comparison table
echo -e "\n\n=== Time Comparison (seconds) ==="

# Print a header
printf "%-15s | " "File"
for threads in "${THREADS[@]}"; do
    printf "%-8s | " "$threads"
done
echo ""

# Print a separator
printf "%s\n" "$(printf '=' {1..72})"

# Print time data for each file
for file in "${FILES[@]}"; do
    printf "%-15s | " "$file"
    for threads in "${THREADS[@]}"; do
        time_s="${TIME_RESULTS["$threads-$file"]}"
        if [ -z "$time_s" ]; then
            printf "%-8s | " "N/A"
        else
            printf "%-8s | " "$time_s"
        fi
    done
    echo ""
done

# Generate speedup comparison table (vs single thread)
echo -e "\n\n=== Speedup Comparison (vs 1 Thread) ==="

# Print a header
printf "%-15s | " "File"
for threads in "${THREADS[@]}"; do
    if [ "$threads" -ne 1 ]; then
        printf "%-8s | " "$threads"
    fi
done
echo ""

# Print a separator
printf "%s\n" "$(printf '=' {1..72})"

# Print speedup data for each file
for file in "${FILES[@]}"; do
    printf "%-15s | " "$file"
    base_time="${TIME_RESULTS["1-$file"]}"
    for threads in "${THREADS[@]}"; do
        if [ "$threads" -ne 1 ]; then
            current_time="${TIME_RESULTS["$threads-$file"]}"
            if [[ -n "$base_time" && "$base_time" != "0" && -n "$current_time" && "$current_time" != "0" ]]; then
                speedup=$(echo "scale=2; $base_time / $current_time" | bc)
                printf "%-8s | " "${speedup}x"
            else
                printf "%-8s | " "N/A"
            fi
        fi
    done
    echo ""
done

# Generate cache miss analysis
echo -e "\n=== Cache Miss Analysis ==="
printf "%-6s %-15s %-12s %-12s %-12s\n" "Threads" "File" "L1 Miss (%)" "LLC Miss (%)" "Total Miss (%)"
for file in "${FILES[@]}"; do
    for threads in "${THREADS[@]}"; do
        cache_refs=${CACHE_REFS["$threads-$file"]}
        cache_misses=${CACHE_MISSES["$threads-$file"]}
        l1_misses=${L1_MISSES["$threads-$file"]}
        llc_misses=${LLC_MISSES["$threads-$file"]}
        
        # Calculate rates only if we have valid numbers
        if [[ $cache_refs -gt 0 ]]; then
            l1_rate=$(echo "scale=2; $l1_misses * 100 / $cache_refs" | bc)
            llc_rate=$(echo "scale=2; $llc_misses * 100 / $cache_refs" | bc)
            total_rate=$(echo "scale=2; $cache_misses * 100 / $cache_refs" | bc)
        else
            l1_rate="N/A"
            llc_rate="N/A"
            total_rate="N/A"
        fi
        
        printf "%-6s %-15s %-12s %-12s %-12s\n" "$threads" "$file" "$l1_rate" "$llc_rate" "$total_rate"
    done
done