#!/bin/bash

# Test files
FILES=("sparse-50000.txt" "random-5000.txt" "corner-50000.txt" "components-50000.txt" "complete-5000.txt")

# Define which thread counts to test and display
THREADS=(1 2 4 8 16 32 64)

# Define approaches - add or remove as needed
APPROACHES=("seq" "trad_1" "trad_2" "trad_3" "trad_4")  

# Store results in memory only, no files
declare -A TIME_RESULTS
declare -A COLOR_RESULTS
declare -A CORRECT_RESULTS
declare -A SPEEDUP_RESULTS

# Function to run a test
run_test() {
    local approach=$1
    local threads=$2
    local file=$3
    
    echo "Running $approach with $threads threads on $file"
    
    # Set threads for parallel approaches
    if [ "$approach" != "seq" ]; then
        export OMP_NUM_THREADS=$threads
    fi
    
    # Run and display output directly
    echo "------------ Start of $approach ($threads threads) output ------------"
    output=$(./traditional_graph_coloring -$approach -f $file 2>&1)
    echo "$output"
    echo "------------ End of $approach ($threads threads) output ------------"
    
    # Extract time using pattern matching - take the last one
    time_s=$(echo "$output" | grep "Time spent:" | tail -1 | awk '{print $3}')
    if [ -z "$time_s" ]; then time_s="0"; fi
    
    # Extract colors using pattern matching - take the last one
    colors=$(echo "$output" | grep "Colored with" | tail -1 | awk '{print $3}')
    if [ -z "$colors" ]; then colors="0"; fi
    
    # Check correctness
    if echo "$output" | grep -q "Failed"; then
        correctness="FAIL"
    else
        correctness="PASS"
    fi
    
    # Calculate speedup if this is not sequential
    speedup=""
    if [ "$approach" != "seq" ]; then
        # Get sequential time from memory
        seq_time=${TIME_RESULTS["seq-1-$file"]}
        if [[ -n "$seq_time" && "$seq_time" != "0" && -n "$time_s" && "$time_s" != "0" ]]; then
            speedup=$(echo "scale=2; $seq_time / $time_s" | bc)
        else
            speedup="N/A"
        fi
    fi
    
    # Store results in memory
    TIME_RESULTS["$approach-$threads-$file"]=$time_s
    COLOR_RESULTS["$approach-$threads-$file"]=$colors
    CORRECT_RESULTS["$approach-$threads-$file"]=$correctness
    SPEEDUP_RESULTS["$approach-$threads-$file"]=$speedup
    
    # Print immediate feedback
    printf "  %-8s %-6s %-15s %-8s %-8s %-12s %-8s\n" "$approach" "$threads" "$file" "$time_s" "$colors" "$correctness" "${speedup}x"
}

# Header for console output
echo "==================== Graph Coloring Benchmark ===================="
printf "%-8s %-6s %-15s %-8s %-8s %-12s %-8s\n" "Approach" "Threads" "File" "Time\(s\)" "Colors" "Correctness" "Speedup"
echo "================================================================="

# Main test loop
for file in "${FILES[@]}"; do
    echo "Processing file: $file"
    
    # First run sequential as baseline
    approach="seq"
    run_test "$approach" 1 "$file"
    
    # Run all non-sequential approaches with different thread counts
    for approach in "${APPROACHES[@]}"; do
        if [ "$approach" != "seq" ]; then
            for threads in "${THREADS[@]}"; do
                if [ "$threads" -ne 1 ]; then  # Skip 1 thread for non-seq approaches
                    run_test "$approach" "$threads" "$file"
                fi
            done
        fi
    done
    
    echo "-----------------------------------------------------------------"
done

# Generate summary tables
echo -e "\n\n=== Performance Summary ==="
for file in "${FILES[@]}"; do
    echo -e "\nResults for $file:"
    printf "%-8s %-6s %-8s %-8s %-12s %-8s\n" "Approach" "Threads" "Time(s)" "Colors" "Correctness" "Speedup"
    printf "%-8s %-6s %-8s %-8s %-12s %-8s\n" "seq" "1" "${TIME_RESULTS["seq-1-$file"]}" "${COLOR_RESULTS["seq-1-$file"]}" "${CORRECT_RESULTS["seq-1-$file"]}" "-"
    
    for approach in "${APPROACHES[@]}"; do
        if [ "$approach" != "seq" ]; then
            for threads in "${THREADS[@]}"; do
                if [ "$threads" -ne 1 ]; then
                    printf "%-8s %-6s %-8s %-8s %-12s %-8s\n" "$approach" "$threads" "${TIME_RESULTS["$approach-$threads-$file"]}" "${COLOR_RESULTS["$approach-$threads-$file"]}" "${CORRECT_RESULTS["$approach-$threads-$file"]}" "${SPEEDUP_RESULTS["$approach-$threads-$file"]}x"
                fi
            done
        fi
    done
done

# Generate time comparison table
echo -e "\n\n=== Time Comparison (seconds) ==="

for file in "${FILES[@]}"; do
    echo -e "\nTime (seconds) for $file:"
    
    # Print a header
    printf "%-8s | " "Threads"
    for approach in "${APPROACHES[@]}"; do
        printf "%-8s | " "$approach"
    done
    echo ""
    
    # Print a separator
    printf "%s\n" "$(printf '=' {1..72})"
    
    # Print the sequential row first (thread count 1)
    printf "%-8s | " "1"
    for approach in "${APPROACHES[@]}"; do
        if [ "$approach" == "seq" ]; then
            printf "%-8s | " "${TIME_RESULTS["seq-1-$file"]}"
        else
            printf "%-8s | " "-"  # Dash for N/A combinations
        fi
    done
    echo ""
    
    # Print time data for each thread count
    for threads in "${THREADS[@]}"; do
        printf "%-8s | " "$threads"
        for approach in "${APPROACHES[@]}"; do
            if [ "$approach" == "seq" ]; then
                printf "%-8s | " "-"  # Sequential doesn't run with >1 threads
            else
                time_s="${TIME_RESULTS["$approach-$threads-$file"]}"
                if [ -z "$time_s" ]; then
                    printf "%-8s | " "N/A"
                else
                    printf "%-8s | " "$time_s"
                fi
            fi
        done
        echo ""
    done
done

# Generate speedup comparison table
echo -e "\n\n=== Speedup Comparison (vs Sequential) ==="

for file in "${FILES[@]}"; do
    echo -e "\nSpeedup for $file:"
    
    # Print a header
    printf "%-8s | " "Threads"
    for approach in "${APPROACHES[@]}"; do
        if [ "$approach" != "seq" ]; then
            printf "%-8s | " "$approach"
        fi
    done
    echo ""
    
    # Print a separator
    printf "%s\n" "$(printf '=' {1..60})"
    
    # Print speedup data for each thread count
    for threads in "${THREADS[@]}"; do
        if [ "$threads" -ne 1 ]; then  # Skip thread count 1
            printf "%-8s | " "$threads"
            for approach in "${APPROACHES[@]}"; do
                if [ "$approach" != "seq" ]; then
                    speedup="${SPEEDUP_RESULTS["$approach-$threads-$file"]}"
                    if [ -z "$speedup" ] || [ "$speedup" == "N/A" ]; then
                        printf "%-8s | " "N/A"
                    else
                        printf "%-8s | " "${speedup}x"
                    fi
                fi
            done
            echo ""
        fi
    done
done

# Generate comparison at 16 threads for a quick overview
echo -e "\n\n=== Performance Comparison at 16 Threads ==="

# Create format string and header
printf "%-20s %-12s" "File" "seq (time)"
for approach in "${APPROACHES[@]}"; do
    if [ "$approach" != "seq" ]; then
        printf " %-12s" "$approach (speed)"
    fi
done
echo ""

for file in "${FILES[@]}"; do
    # Print the file and sequential time
    printf "%-20s %-12s" "$file" "${TIME_RESULTS["seq-1-$file"]}"
    
    # Get speedup for each approach at 16 threads
    for approach in "${APPROACHES[@]}"; do
        if [ "$approach" != "seq" ]; then
            speedup="${SPEEDUP_RESULTS["$approach-16-$file"]}"
            if [ -z "$speedup" ] || [ "$speedup" == "N/A" ]; then
                printf " %-12s" "N/A"
            else
                printf " %-12s" "${speedup}x"
            fi
        fi
    done
    echo ""
done