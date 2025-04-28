#!/bin/bash

# Test files
FILES=("sparse-50000.txt" "random-5000.txt" "corner-50000.txt" "components-50000.txt")

# Define thread counts to test
THREADS=(1 2 4 8 16 32)

# Store results in memory
declare -A TIME_RESULTS
declare -A COLOR_RESULTS
declare -A CORRECT_RESULTS
declare -A SPEEDUP_RESULTS

# Function to run a test for HTM
run_htm_test() {
    local threads=$1
    local file=$2
    
    echo "Running HTM with $threads threads on $file"
    export OMP_NUM_THREADS=$threads
    
    # Capture the program output
    program_output=$(./coloring_tsx $file $threads 2>&1)
    echo "$program_output"
    
    # Extract time from HTM output
    time_s=$(echo "$program_output" | grep "HTM coloring completed in" | awk '{print $5}')
    
    # Default to 0 if no time found
    if [ -z "$time_s" ]; then 
        time_s=0
        echo "WARNING: Could not detect time output format for $file with $threads threads"
    fi
    
    # Extract colors
    colors=$(echo "$program_output" | grep "Used " | awk '{print $2}')
    if [ -z "$colors" ]; then colors="0"; fi
    
    # Check correctness
    if echo "$program_output" | grep -q "Coloring is valid"; then
        correctness="PASS"
    else
        correctness="FAIL"
    fi
    
    # Store results
    TIME_RESULTS["htm-$threads-$file"]=$time_s
    COLOR_RESULTS["htm-$threads-$file"]=$colors
    CORRECT_RESULTS["htm-$threads-$file"]=$correctness
    
    # Print immediate feedback
    printf "  %-6s %-15s %-8s %-8s %-12s\n" "$threads" "$file" "$time_s" "$colors" "$correctness"
}

# Function to run a test for sequential
run_seq_test() {
    local file=$1
    
    echo "Running sequential algorithm on $file"
    
    # Capture the program output
    program_output=$(./color-release -seq -f $file 2>&1)
    echo "$program_output"
    
    # Extract time
    time_s=$(echo "$program_output" | grep "Time spent:" | awk '{print $3}')
    
    # Default to 0 if no time found
    if [ -z "$time_s" ]; then 
        time_s=0
        echo "WARNING: Could not detect time output format for sequential on $file"
    fi
    
    # Extract colors
    colors=$(echo "$program_output" | grep "Colored with" | awk '{print $3}')
    if [ -z "$colors" ]; then colors="0"; fi
    
    # Check correctness (always assume sequential is correct)
    correctness="PASS"
    
    # Store results
    TIME_RESULTS["seq-1-$file"]=$time_s
    COLOR_RESULTS["seq-1-$file"]=$colors
    CORRECT_RESULTS["seq-1-$file"]=$correctness
    
    # Print immediate feedback
    printf "  %-6s %-15s %-8s %-8s %-12s\n" "seq" "$file" "$time_s" "$colors" "$correctness"
}

# Header for console output
echo "==================== HTM Graph Coloring Benchmark ===================="
printf "%-6s %-15s %-8s %-8s %-12s\n" "Threads" "File" "Time(s)" "Colors" "Correctness"
echo "===================================================================="

# Main test loop
for file in "${FILES[@]}"; do
    echo "Processing file: $file"
    
    # First run sequential as baseline
    run_seq_test "$file"
    
    # Run HTM with different thread counts
    for threads in "${THREADS[@]}"; do
        if [ "$threads" -ne 1 ]; then  # Skip 1 thread for HTM as it's not usually efficient
            run_htm_test "$threads" "$file"
        fi
    done
    
    echo "-----------------------------------------------------------------"
    
    # Calculate speedups after running all tests for this file
    for threads in "${THREADS[@]}"; do
        if [ "$threads" -ne 1 ]; then  # Skip 1 thread
            seq_time="${TIME_RESULTS["seq-1-$file"]}"
            htm_time="${TIME_RESULTS["htm-$threads-$file"]}"
            
            if [[ -n "$seq_time" && "$seq_time" != "0" && -n "$htm_time" && "$htm_time" != "0" ]]; then
                speedup=$(echo "scale=2; $seq_time / $htm_time" | bc)
                SPEEDUP_RESULTS["htm-$threads-$file"]=$speedup
            else
                SPEEDUP_RESULTS["htm-$threads-$file"]="N/A"
            fi
        fi
    done
done

# Generate summary tables
echo -e "\n\n=== Performance Summary ==="
for file in "${FILES[@]}"; do
    echo -e "\nResults for $file:"
    printf "%-8s %-6s %-8s %-8s %-12s %-8s\n" "Approach" "Threads" "Time(s)" "Colors" "Correctness" "Speedup"
    printf "%-8s %-6s %-8s %-8s %-12s %-8s\n" "seq" "1" "${TIME_RESULTS["seq-1-$file"]}" "${COLOR_RESULTS["seq-1-$file"]}" "${CORRECT_RESULTS["seq-1-$file"]}" "-"
    
    for threads in "${THREADS[@]}"; do
        if [ "$threads" -ne 1 ]; then  # Skip 1 thread for HTM
            printf "%-8s %-6s %-8s %-8s %-12s %-8s\n" "htm" "$threads" "${TIME_RESULTS["htm-$threads-$file"]}" "${COLOR_RESULTS["htm-$threads-$file"]}" "${CORRECT_RESULTS["htm-$threads-$file"]}" "${SPEEDUP_RESULTS["htm-$threads-$file"]}x"
        fi
    done
done

# Generate time comparison table
echo -e "\n\n=== Time Comparison (seconds) ==="

# Print a header
printf "%-15s | %-8s | " "File" "seq"
for threads in "${THREADS[@]}"; do
    if [ "$threads" -ne 1 ]; then  # Skip 1 thread for HTM
        printf "htm-%-5s | " "$threads"
    fi
done
echo ""

# Print a separator
printf "%s\n" "$(printf '=' {1..80})"

# Print time data for each file
for file in "${FILES[@]}"; do
    printf "%-15s | %-8s | " "$file" "${TIME_RESULTS["seq-1-$file"]}"
    for threads in "${THREADS[@]}"; do
        if [ "$threads" -ne 1 ]; then  # Skip 1 thread for HTM
            time_s="${TIME_RESULTS["htm-$threads-$file"]}"
            if [ -z "$time_s" ]; then
                printf "%-8s | " "N/A"
            else
                printf "%-8s | " "$time_s"
            fi
        fi
    done
    echo ""
done

# Generate speedup comparison table
echo -e "\n\n=== Speedup Comparison (vs Sequential) ==="

# Print a header
printf "%-15s | " "File"
for threads in "${THREADS[@]}"; do
    if [ "$threads" -ne 1 ]; then  # Skip 1 thread for HTM
        printf "htm-%-5s | " "$threads"
    fi
done
echo ""

# Print a separator
printf "%s\n" "$(printf '=' {1..80})"

# Print speedup data for each file
for file in "${FILES[@]}"; do
    printf "%-15s | " "$file"
    for threads in "${THREADS[@]}"; do
        if [ "$threads" -ne 1 ]; then  # Skip 1 thread for HTM
            speedup="${SPEEDUP_RESULTS["htm-$threads-$file"]}"
            if [ -z "$speedup" ] || [ "$speedup" == "N/A" ]; then
                printf "%-8s | " "N/A"
            else
                printf "%-8s | " "${speedup}x"
            fi
        fi
    done
    echo ""
done

# Generate color quality comparison
echo -e "\n\n=== Color Quality Comparison ==="

# Print a header
printf "%-15s | %-8s | %-8s | %-15s\n" "File" "Seq Colors" "HTM Colors" "Quality Ratio"
printf "%s\n" "$(printf '=' {1..55})"

# Print color comparison for each file
for file in "${FILES[@]}"; do
    seq_colors="${COLOR_RESULTS["seq-1-$file"]}"
    htm_colors="${COLOR_RESULTS["htm-8-$file"]}"  # Using 8 threads as reference
    
    if [[ -n "$seq_colors" && "$seq_colors" != "0" && -n "$htm_colors" && "$htm_colors" != "0" ]]; then
        # Lower number of colors is better, so ratio is seq/htm
        quality_ratio=$(echo "scale=2; $seq_colors / $htm_colors" | bc)
        printf "%-15s | %-8s | %-8s | %-15s\n" "$file" "$seq_colors" "$htm_colors" "$quality_ratio"
    else
        printf "%-15s | %-8s | %-8s | %-15s\n" "$file" "$seq_colors" "$htm_colors" "N/A"
    fi
done

# Strong scaling efficiency analysis
echo -e "\n=== Strong Scaling Efficiency ==="
printf "%-15s | " "File"
for threads in "${THREADS[@]}"; do
    if [ "$threads" -ne 1 ]; then  # Skip 1 thread for HTM
        printf "%-8s | " "$threads"
    fi
done
echo ""

# Print a separator
printf "%s\n" "$(printf '=' {1..80})"

# Print efficiency data for each file
for file in "${FILES[@]}"; do
    printf "%-15s | " "$file"
    for threads in "${THREADS[@]}"; do
        if [ "$threads" -ne 1 ]; then  # Skip 1 thread for HTM
            speedup="${SPEEDUP_RESULTS["htm-$threads-$file"]}"
            if [ -z "$speedup" ] || [ "$speedup" == "N/A" ]; then
                efficiency="N/A"
            else
                efficiency=$(echo "scale=2; 100 * $speedup / $threads" | bc)
                efficiency="${efficiency}%"
            fi
            printf "%-8s | " "$efficiency"
        fi
    done
    echo ""
done
