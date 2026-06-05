#!/bin/bash
# ============================================================================
# benchmark.sh — Automated Experiment Runner for exec/mmput Timing
# ============================================================================
#
# Usage:  sudo ./benchmark.sh [iterations]
#
#   iterations  : number of times to run each test (default: 10)
#
# Prerequisites:
#   - Kernel module (time.ko) must be loaded: sudo insmod time.ko
#   - Must run as root (needs dmesg access and sysfs writes)
#
# Output:
#   - Console: formatted summary table with averages
#   - File:    results_YYYYMMDD_HHMMSS.csv with per-run raw data
# ============================================================================

set -e

# ---- Configuration ----
ITERATIONS=${1:-10}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
CSV_FILE="${SCRIPT_DIR}/results_${TIMESTAMP}.csv"

# Test files and their BSS sizes (MB) for the summary
declare -a TEST_FILES=("test" "test1" "test2" "test3" "test4" "test5" "test6" "test7" "test8" "test9" "test10")
declare -a BSS_SIZES=("0" "50" "200" "300" "400" "600" "800" "1024" "1600" "2048" "4096")

# ---- Color codes for pretty output ----
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color

# ---- Helper functions ----

print_header() {
    echo ""
    echo -e "${CYAN}╔══════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC}  ${BOLD}Kernel Exec/MMput Timing Benchmark${NC}                         ${CYAN}║${NC}"
    echo -e "${CYAN}╠══════════════════════════════════════════════════════════════╣${NC}"
    echo -e "${CYAN}║${NC}  Iterations per test : ${BOLD}${ITERATIONS}${NC}                                    ${CYAN}║${NC}"
    echo -e "${CYAN}║${NC}  Total test cases    : ${BOLD}${#TEST_FILES[@]}${NC}                                    ${CYAN}║${NC}"
    echo -e "${CYAN}║${NC}  Results file        : ${BOLD}$(basename $CSV_FILE)${NC}     ${CYAN}║${NC}"
    echo -e "${CYAN}╚══════════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

check_root() {
    if [ "$EUID" -ne 0 ]; then
        echo -e "${RED}ERROR: Must run as root (sudo ./benchmark.sh)${NC}"
        exit 1
    fi
}

check_module() {
    if ! lsmod | grep -q "^time "; then
        echo -e "${RED}ERROR: Kernel module 'time' is not loaded.${NC}"
        echo -e "${YELLOW}Load it with: sudo insmod time.ko${NC}"
        exit 1
    fi
    echo -e "${GREEN}✓ Kernel module 'time' is loaded${NC}"
}

check_sysfs() {
    if [ ! -f /sys/kernel/time/target_pid ]; then
        echo -e "${RED}ERROR: /sys/kernel/time/target_pid not found${NC}"
        echo -e "${YELLOW}Is the kernel module loaded correctly?${NC}"
        exit 1
    fi
    echo -e "${GREEN}✓ sysfs interface available${NC}"
}

compile_tests() {
    echo -e "\n${BOLD}Compiling test programs...${NC}"
    local failed=0
    for t in "${TEST_FILES[@]}"; do
        src="${SCRIPT_DIR}/${t}.c"
        bin="${SCRIPT_DIR}/${t}_bin"
        if [ ! -f "$src" ]; then
            echo -e "  ${RED}✗ ${t}.c not found${NC}"
            failed=1
            continue
        fi
        gcc -o "$bin" "$src" 2>/dev/null
        if [ $? -eq 0 ]; then
            echo -e "  ${GREEN}✓ ${t}.c → ${t}_bin${NC}"
        else
            echo -e "  ${RED}✗ ${t}.c failed to compile${NC}"
            failed=1
        fi
    done
    if [ $failed -eq 1 ]; then
        echo -e "${RED}Some tests failed to compile. Continuing with available tests.${NC}"
    fi
}

# Extract elapsed time (ns) from dmesg for a given probe and PID
# Usage: extract_elapsed "do_execveat_common" $pid
extract_elapsed() {
    local probe_name="$1"
    local pid="$2"
    # Look for the RETURN line with the matching PID and probe function
    # Format: [exec_probe] <probe_name> RETURN pid=<pid> ... elapsed=<ns> ns ...
    dmesg | grep "\[exec_probe\] ${probe_name} RETURN" | grep "pid=${pid} " | tail -1 | \
        grep -oP 'elapsed=\K[0-9]+(?= ns)' || echo "0"
}

# ---- Main benchmark loop ----

run_benchmark() {
    # CSV header
    echo "test,bss_mb,iteration,execve_ns,mmput_ns" > "$CSV_FILE"

    # Arrays to accumulate per-test totals for the summary
    declare -a execve_totals
    declare -a mmput_totals
    declare -a execve_counts
    declare -a mmput_counts

    for idx in "${!TEST_FILES[@]}"; do
        local t="${TEST_FILES[$idx]}"
        local bss="${BSS_SIZES[$idx]}"
        local bin="${SCRIPT_DIR}/${t}_bin"

        if [ ! -x "$bin" ]; then
            echo -e "  ${YELLOW}⏭ Skipping ${t} (binary not found)${NC}"
            execve_totals[$idx]=0
            mmput_totals[$idx]=0
            execve_counts[$idx]=0
            mmput_counts[$idx]=0
            continue
        fi

        echo -e "\n${BOLD}Running ${t}${NC} (BSS=${bss} MB) × ${ITERATIONS} iterations..."

        execve_totals[$idx]=0
        mmput_totals[$idx]=0
        execve_counts[$idx]=0
        mmput_counts[$idx]=0

        for i in $(seq 1 $ITERATIONS); do
            # Clear dmesg to isolate this run's output
            dmesg -C

            # Run the test binary (it does execv → /bin/ls)
            # We need to capture the PID. Since the binary writes its own PID to sysfs,
            # we just run it and then extract from dmesg.
            $bin > /dev/null 2>&1 || true

            # Small delay to let kernel log flush
            sleep 0.1

            # Extract the PID from dmesg (the binary prints and also kprobe logs it)
            local pid
            pid=$(dmesg | grep "\[exec_probe\] do_execveat_common RETURN" | tail -1 | \
                  grep -oP 'pid=\K[0-9]+' | head -1 || echo "")

            local execve_ns=0
            local mmput_ns=0

            if [ -n "$pid" ]; then
                execve_ns=$(extract_elapsed "do_execveat_common" "$pid")
                mmput_ns=$(extract_elapsed "exec_deferred_mmput" "$pid")
            fi

            # Accumulate
            execve_totals[$idx]=$(( ${execve_totals[$idx]} + execve_ns ))
            mmput_totals[$idx]=$(( ${mmput_totals[$idx]} + mmput_ns ))

            if [ "$execve_ns" -gt 0 ] 2>/dev/null; then
                execve_counts[$idx]=$(( ${execve_counts[$idx]} + 1 ))
            fi
            if [ "$mmput_ns" -gt 0 ] 2>/dev/null; then
                mmput_counts[$idx]=$(( ${mmput_counts[$idx]} + 1 ))
            fi

            # Write raw data to CSV
            echo "${t},${bss},${i},${execve_ns},${mmput_ns}" >> "$CSV_FILE"

            # Progress indicator
            printf "  [%3d/%3d] execve=%10s ns  deferred=%10s ns\r" "$i" "$ITERATIONS" "$execve_ns" "$mmput_ns"
        done
        echo "" # newline after progress
    done

    # ---- Compute summary with outlier removal (IQR method) and append to CSV ----
    compute_summary

    # ---- Print Console Summary Table ----
    print_console_summary
}

# ============================================================================
# Compute IQR-filtered averages and append summary to CSV
# Uses awk to: sort values, find Q1/Q3/IQR, discard outliers, compute avg
# ============================================================================
compute_summary() {
    # Snapshot the raw data portion before we start appending summary rows
    local raw_data_file="${CSV_FILE}.raw_tmp"
    cp "$CSV_FILE" "$raw_data_file"

    echo "" >> "$CSV_FILE"
    echo "# =============================== SUMMARY ===============================" >> "$CSV_FILE"
    echo "# Outlier removal: IQR method (values outside [Q1 - 1.5*IQR, Q3 + 1.5*IQR] removed)" >> "$CSV_FILE"
    echo "" >> "$CSV_FILE"
    echo "test,bss_mb,total_runs,raw_avg_execve_ns,raw_avg_execve_us,raw_avg_mmput_ns,raw_avg_mmput_us,filtered_runs_execve,filtered_avg_execve_ns,filtered_avg_execve_us,filtered_runs_mmput,filtered_avg_mmput_ns,filtered_avg_mmput_us" >> "$CSV_FILE"

    for idx in "${!TEST_FILES[@]}"; do
        local t="${TEST_FILES[$idx]}"
        local bss="${BSS_SIZES[$idx]}"

        # Use awk to compute raw avg + IQR-filtered avg for both execve and mmput
        # Read from the snapshot so we don't pick up summary rows from previous iterations
        awk -F',' -v test_name="$t" -v bss_mb="$bss" '
        BEGIN {
            e_n = 0; m_n = 0;
        }
        # Skip header and comment lines
        /^#/ || /^test,bss_mb/ { next }
        {
            if ($1 == test_name && $4 + 0 > 0) {
                e_vals[e_n++] = $4 + 0
            }
            if ($1 == test_name && $5 + 0 > 0) {
                m_vals[m_n++] = $5 + 0
            }
        }
        END {
            # ---- Helper: sort an array (insertion sort, fine for small N) ----
            # Sort execve values
            for (i = 1; i < e_n; i++) {
                key = e_vals[i]
                j = i - 1
                while (j >= 0 && e_vals[j] > key) {
                    e_vals[j+1] = e_vals[j]
                    j--
                }
                e_vals[j+1] = key
            }
            # Sort mmput values
            for (i = 1; i < m_n; i++) {
                key = m_vals[i]
                j = i - 1
                while (j >= 0 && m_vals[j] > key) {
                    m_vals[j+1] = m_vals[j]
                    j--
                }
                m_vals[j+1] = key
            }

            # ---- Raw averages ----
            e_raw_sum = 0; m_raw_sum = 0
            for (i = 0; i < e_n; i++) e_raw_sum += e_vals[i]
            for (i = 0; i < m_n; i++) m_raw_sum += m_vals[i]
            e_raw_avg = (e_n > 0) ? e_raw_sum / e_n : 0
            m_raw_avg = (m_n > 0) ? m_raw_sum / m_n : 0

            # ---- IQR filtering for execve ----
            e_filt_n = 0; e_filt_sum = 0
            if (e_n >= 4) {
                e_q1 = e_vals[int(e_n * 0.25)]
                e_q3 = e_vals[int(e_n * 0.75)]
                e_iqr = e_q3 - e_q1
                e_lo = e_q1 - 1.5 * e_iqr
                e_hi = e_q3 + 1.5 * e_iqr
                for (i = 0; i < e_n; i++) {
                    if (e_vals[i] >= e_lo && e_vals[i] <= e_hi) {
                        e_filt_sum += e_vals[i]
                        e_filt_n++
                    }
                }
            } else {
                # Too few data points to filter, use all
                e_filt_sum = e_raw_sum
                e_filt_n = e_n
            }
            e_filt_avg = (e_filt_n > 0) ? e_filt_sum / e_filt_n : 0

            # ---- IQR filtering for mmput ----
            m_filt_n = 0; m_filt_sum = 0
            if (m_n >= 4) {
                m_q1 = m_vals[int(m_n * 0.25)]
                m_q3 = m_vals[int(m_n * 0.75)]
                m_iqr = m_q3 - m_q1
                m_lo = m_q1 - 1.5 * m_iqr
                m_hi = m_q3 + 1.5 * m_iqr
                for (i = 0; i < m_n; i++) {
                    if (m_vals[i] >= m_lo && m_vals[i] <= m_hi) {
                        m_filt_sum += m_vals[i]
                        m_filt_n++
                    }
                }
            } else {
                m_filt_sum = m_raw_sum
                m_filt_n = m_n
            }
            m_filt_avg = (m_filt_n > 0) ? m_filt_sum / m_filt_n : 0

            total_runs = (e_n > m_n) ? e_n : m_n

            printf "%s,%s,%d,%.0f,%.2f,%.0f,%.2f,%d,%.0f,%.2f,%d,%.0f,%.2f\n", \
                test_name, bss_mb, total_runs, \
                e_raw_avg, e_raw_avg / 1000, \
                m_raw_avg, m_raw_avg / 1000, \
                e_filt_n, e_filt_avg, e_filt_avg / 1000, \
                m_filt_n, m_filt_avg, m_filt_avg / 1000
        }
        ' "$raw_data_file" >> "$CSV_FILE"
    done

    # Clean up temp file
    rm -f "$raw_data_file"

    echo "" >> "$CSV_FILE"
    echo "# Generated: $(date)" >> "$CSV_FILE"
}

# ============================================================================
# Print a pretty console summary (uses the CSV summary rows)
# ============================================================================
print_console_summary() {
    echo ""
    echo -e "${CYAN}══════════════════════════════════════════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${BOLD}                                 RAW RESULTS (all iterations)${NC}"
    echo -e "${CYAN}══════════════════════════════════════════════════════════════════════════════════════════════════════════${NC}"
    printf "${BOLD}%-8s  %8s  %5s  %15s  %12s  %15s  %12s${NC}\n" \
           "Test" "BSS(MB)" "Runs" "Avg execve(ns)" "Avg(μs)" "Avg deferred(ns)" "Avg(μs)"
    echo -e "${CYAN}──────────────────────────────────────────────────────────────────────────────────────────────────────────${NC}"

    # Parse the summary rows from CSV (skip comments and headers)
    awk -F',' '
    /^test,bss_mb,total_runs/ { reading=1; next }
    /^#/ || /^$/ { if (reading) exit; next }
    reading {
        printf "%-8s  %8s  %5d  %15s  %12s  %15s  %12s\n", \
            $1, $2, $3, $4, $5, $6, $7
    }
    ' "$CSV_FILE"

    echo ""
    echo -e "${CYAN}══════════════════════════════════════════════════════════════════════════════════════════════════════════${NC}"
    echo -e "${BOLD}                         FILTERED RESULTS (outliers removed via IQR)${NC}"
    echo -e "${CYAN}══════════════════════════════════════════════════════════════════════════════════════════════════════════${NC}"
    printf "${BOLD}%-8s  %8s  %5s  %15s  %12s  %15s  %12s${NC}\n" \
           "Test" "BSS(MB)" "Kept" "Avg execve(ns)" "Avg(μs)" "Avg deferred(ns)" "Avg(μs)"
    echo -e "${CYAN}──────────────────────────────────────────────────────────────────────────────────────────────────────────${NC}"

    awk -F',' '
    /^test,bss_mb,total_runs/ { reading=1; next }
    /^#/ || /^$/ { if (reading) exit; next }
    reading {
        # Filtered columns: $8=filt_runs_execve, $9=filt_avg_execve_ns, $10=filt_avg_execve_us
        #                    $11=filt_runs_mmput, $12=filt_avg_mmput_ns, $13=filt_avg_mmput_us
        kept = ($8 > $11) ? $8 : $11
        printf "%-8s  %8s  %5s  %15s  %12s  %15s  %12s\n", \
            $1, $2, kept"/"$3, $9, $10, $12, $13
    }
    ' "$CSV_FILE"

    echo -e "${CYAN}══════════════════════════════════════════════════════════════════════════════════════════════════════════${NC}"
    echo ""
    echo -e "${GREEN}Full results (raw + summary) saved to: ${BOLD}${CSV_FILE}${NC}"
    echo ""
}

# ---- Entry point ----
main() {
    print_header
    check_root
    check_module
    check_sysfs
    compile_tests
    echo ""
    echo -e "${BOLD}Starting benchmark...${NC}"
    run_benchmark
    echo -e "${GREEN}${BOLD}Benchmark complete!${NC}"
}

main
