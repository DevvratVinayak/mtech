#!/bin/bash
# run_experiments.sh — Run async (module ON) and sync (module OFF) experiments
#
# Usage:
#   sudo ./run_experiments.sh async    # Run with module loaded
#   sudo ./run_experiments.sh sync     # Run without module
#   sudo ./run_experiments.sh all      # Run async, then unload, then sync
#
# Human-time: ~10 min total | Compute-time: ~10 min total
#
# Note: We do NOT use 'set -e' because each test program calls execv("/bin/ls")
# whose exit code is the exit code of /bin/ls. With output redirected to
# /dev/null, broken-pipe and similar transient errors can yield non-zero exit
# codes that would abort the whole experiment. Each test invocation is wrapped
# with `|| true` to ensure the loop always continues for all 50 runs of all
# 10 tests.

set -u  # error on undefined variables, but NOT on non-zero exit codes

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ARTIFACT_DIR="$(dirname "$SCRIPT_DIR")"
SRC_DIR="$ARTIFACT_DIR/src"
BENCH_DIR="$ARTIFACT_DIR/benchmarks"
RESULTS_DIR="$ARTIFACT_DIR/results"
RUNS=${RUNS:-50}

mkdir -p "$RESULTS_DIR"

TESTS="test0 test1 test2 test3 test4 test5 test6 test7 test8 test9"
MEMORY_SIZES=("~0 MB" "50 MB" "100 MB" "200 MB" "400 MB" "600 MB" "800 MB" "1024 MB" "1600 MB" "2048 MB")

run_async() {
    echo "=========================================="
    echo "  Phase 1: ASYNC Tests (Module ON)"
    echo "  Runs per test: $RUNS"
    echo "=========================================="

    if lsmod | grep -q "^time "; then
        echo "[*] Module already loaded — skipping insmod"
    else
        echo ""
        echo "[*] Loading kernel module..."
        if ! insmod "$SRC_DIR/time.ko"; then
            echo "ERROR: failed to load module. Did setup.sh succeed?"
            exit 1
        fi
        dmesg | tail -5
        echo "  ✓ Module loaded"
    fi

    idx=0
    for test in $TESTS; do
        echo ""
        echo "[*] Running $test (${MEMORY_SIZES[$idx]}) — $RUNS iterations..."
        dmesg -C
        for i in $(seq 1 $RUNS); do
            "$BENCH_DIR/$test" > /dev/null 2>&1 || true
        done
        dmesg | grep "exec.*RETURN.*$test" > "$RESULTS_DIR/async_avg_${test}.txt" || true
        count=$(wc -l < "$RESULTS_DIR/async_avg_${test}.txt")
        echo "  ✓ $test done: $count samples captured"
        idx=$((idx + 1))
    done

    echo ""
    echo "=========================================="
    echo "  Async tests complete!"
    echo "  Results: $RESULTS_DIR/async_avg_*.txt"
    echo "=========================================="
}

unload_module() {
    if lsmod | grep -q "^time "; then
        echo ""
        echo "[*] Unloading kernel module..."
        if rmmod time 2>/dev/null; then
            echo "  ✓ Module unloaded"
        else
            echo "  ✗ rmmod failed — kernel may lack CONFIG_MODULE_UNLOAD."
            echo "    Reboot the VM, then re-run with 'sync' argument."
            exit 1
        fi
    fi
}

run_sync() {
    echo "=========================================="
    echo "  Phase 2: SYNC Tests (Module OFF)"
    echo "  Runs per test: $RUNS"
    echo "=========================================="

    if lsmod | grep -q "^time "; then
        echo "[*] Module is currently loaded. Unloading..."
        if ! rmmod time 2>/dev/null; then
            echo "ERROR: rmmod failed. Reboot the VM, then run: sudo $0 sync"
            exit 1
        fi
        echo "  ✓ Module unloaded"
    fi
    echo "[*] Module not loaded — proceeding with sync tests"

    idx=0
    for test in $TESTS; do
        echo ""
        echo "[*] Running $test (${MEMORY_SIZES[$idx]}) — $RUNS iterations..."
        : > "$RESULTS_DIR/sync_avg_${test}.txt"   # truncate
        for i in $(seq 1 $RUNS); do
            { time "$BENCH_DIR/$test" > /dev/null 2>&1 || true ; } 2>> "$RESULTS_DIR/sync_avg_${test}.txt"
        done
        count=$(grep -c "^real" "$RESULTS_DIR/sync_avg_${test}.txt" || echo 0)
        echo "  ✓ $test done: $count samples captured"
        idx=$((idx + 1))
    done

    echo ""
    echo "=========================================="
    echo "  Sync tests complete!"
    echo "  Results: $RESULTS_DIR/sync_avg_*.txt"
    echo ""
    echo "  Run plots: python3 $SCRIPT_DIR/plot_results.py"
    echo "=========================================="
}

case "${1:-all}" in
    async)
        run_async
        ;;
    sync)
        run_sync
        ;;
    all)
        run_async
        unload_module
        run_sync
        echo ""
        echo "=========================================="
        echo "  All experiments complete!"
        echo "  Generate plots: python3 $SCRIPT_DIR/plot_results.py"
        echo "=========================================="
        ;;
    *)
        echo "Usage: sudo $0 {async|sync|all}"
        echo "  async  — Run with module loaded"
        echo "  sync   — Run without module"
        echo "  all    — Run async, unload module, then sync (recommended)"
        exit 1
        ;;
esac
