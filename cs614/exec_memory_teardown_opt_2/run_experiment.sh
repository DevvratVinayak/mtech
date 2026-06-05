#!/bin/bash
#
# run_experiment.sh — Fully automated benchmark pipeline
# Runs baseline + optimized benchmarks, collects stats, generates plots
#
# Usage: sudo ./run_experiment.sh [iterations]
#

set -euo pipefail

ITERATIONS=${1:-50}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RESULTS_DIR="$SCRIPT_DIR/results_${TIMESTAMP}"

RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

if [ "$EUID" -ne 0 ]; then
    echo -e "${RED}ERROR: Please run as root: sudo ./run_experiment.sh [iterations]${NC}"
    exit 1
fi

mkdir -p "$RESULTS_DIR"

echo -e "${BOLD}${CYAN}"
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║     exec_deferred_mmput Benchmark Pipeline                  ║"
echo "║     Iterations: $ITERATIONS per test, Results: $RESULTS_DIR ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo -e "${NC}"

# ── Verify kernel support ────────────────────────────────────────
echo -e "${BOLD}[1/5] Verifying kernel support...${NC}"

if [ ! -f /proc/sys/kernel/exec_async_teardown ]; then
    echo -e "${RED}ERROR: /proc/sys/kernel/exec_async_teardown not found!"
    echo "The kernel does not have the async teardown patch applied."
    echo "Apply the patch and reboot first.${NC}"
    exit 1
fi

if [ ! -f /proc/exec_teardown_stats ]; then
    echo -e "${RED}ERROR: /proc/exec_teardown_stats not found!${NC}"
    exit 1
fi

echo -e "  ${GREEN}✓ Kernel support verified${NC}"

# ── Load kprobe module ──────────────────────────────────────────
echo -e "\n${BOLD}[2/5] Loading measurement module...${NC}"

cd "$SCRIPT_DIR"

# Remove if already loaded
rmmod time 2>/dev/null || true

# Build and load
if [ -f Makefile ]; then
    make clean 2>/dev/null || true
    make 2>&1 | tail -1
fi

if [ -f time.ko ]; then
    insmod time.ko
    echo -e "  ${GREEN}✓ time.ko loaded${NC}"
else
    echo -e "  ${RED}✗ time.ko not found — build it first${NC}"
    exit 1
fi

# ── Run baseline (async OFF) ────────────────────────────────────

echo -e "\n${BOLD}[3/5] Running BASELINE benchmark (async OFF)...${NC}"

echo 0 > /proc/sys/kernel/exec_async_teardown
echo 0 > /proc/exec_teardown_stats 2>/dev/null || true
dmesg -C  # Clear dmesg

sleep 1

./benchmark.sh "$ITERATIONS"

# Find the most recent CSV
BASELINE_CSV=$(ls -t "$SCRIPT_DIR"/results_*.csv 2>/dev/null | head -1)
if [ -z "$BASELINE_CSV" ]; then
    echo -e "${RED}ERROR: No baseline CSV found${NC}"
    exit 1
fi
cp "$BASELINE_CSV" "$RESULTS_DIR/baseline.csv"

# Save baseline stats
cat /proc/exec_teardown_stats > "$RESULTS_DIR/baseline_stats.txt"
echo -e "  ${GREEN}✓ Baseline saved: $RESULTS_DIR/baseline.csv${NC}"
echo ""
cat "$RESULTS_DIR/baseline_stats.txt"

# ── Run optimized (async ON) ────────────────────────────────────
echo -e "\n${BOLD}[4/5] Running OPTIMIZED benchmark (async ON)...${NC}"

echo 1 > /proc/sys/kernel/exec_async_teardown
echo 0 > /proc/exec_teardown_stats 2>/dev/null || true
dmesg -C  # Clear dmesg

sleep 1

./benchmark.sh "$ITERATIONS"

# Find the most recent CSV
OPTIMIZED_CSV=$(ls -t "$SCRIPT_DIR"/results_*.csv 2>/dev/null | head -1)
if [ -z "$OPTIMIZED_CSV" ]; then
    echo -e "${RED}ERROR: No optimized CSV found${NC}"
    exit 1
fi
cp "$OPTIMIZED_CSV" "$RESULTS_DIR/optimized.csv"

# Save optimized stats
cat /proc/exec_teardown_stats > "$RESULTS_DIR/optimized_stats.txt"
echo -e "  ${GREEN}✓ Optimized saved: $RESULTS_DIR/optimized.csv${NC}"
echo ""
cat "$RESULTS_DIR/optimized_stats.txt"

# ── Save kernel dmesg ────────────────────────────────────────────
echo -e "\n${BOLD}[5/5] Saving kernel logs...${NC}"

dmesg > "$RESULTS_DIR/dmesg_optimized.txt"
uname -r > "$RESULTS_DIR/kernel_version.txt"
cat /proc/cpuinfo | head -20 > "$RESULTS_DIR/cpu_info.txt"
free -h > "$RESULTS_DIR/memory_info.txt"

echo -e "  ${GREEN}✓ Logs saved${NC}"

# ── Generate plots ───────────────────────────────────────────────
echo -e "\n${BOLD} Generating plots...${NC}"

if command -v python3 &>/dev/null; then
    if python3 -c "import matplotlib" 2>/dev/null; then
        python3 "$SCRIPT_DIR/plot_results.py" \
            "$RESULTS_DIR/baseline.csv" \
            "$RESULTS_DIR/optimized.csv" \
            "$RESULTS_DIR"
        echo -e "  ${GREEN}✓ Plots generated${NC}"
    else
        echo -e "  ${CYAN}matplotlib not available — copy CSVs to your laptop and run:${NC}"
        echo "    pip install matplotlib"
        echo "    python3 plot_results.py baseline.csv optimized.csv results/"
    fi
else
    echo -e "  ${CYAN}python3 not available — copy CSVs to laptop for plotting${NC}"
fi

# ── Summary ──────────────────────────────────────────────────────
echo ""
echo -e "${BOLD}${CYAN}"
echo "╔══════════════════════════════════════════════════════════════╗"
echo "║                    EXPERIMENT COMPLETE                       ║"
echo "╚══════════════════════════════════════════════════════════════╝"
echo -e "${NC}"
echo -e "Results directory: ${BOLD}$RESULTS_DIR/${NC}"
echo ""
ls -la "$RESULTS_DIR/"
echo ""
echo -e "${BOLD}To generate plots on your laptop:${NC}"
echo "  scp -r cs614@<VM_IP>:$RESULTS_DIR/ ./"
echo "  pip install matplotlib"
echo "  python3 plot_results.py results/baseline.csv results/optimized.csv results/"
