#!/bin/bash
# setup.sh — Compile the kernel module and all test programs
# Human-time: ~1 minute | Compute-time: ~30 seconds
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ARTIFACT_DIR="$(dirname "$SCRIPT_DIR")"
SRC_DIR="$ARTIFACT_DIR/src"
BENCH_DIR="$ARTIFACT_DIR/benchmarks"

echo "=========================================="
echo "  Async MM Teardown — Setup"
echo "=========================================="

# Step 1: Build the kernel module
echo ""
echo "[1/2] Building kernel module..."
cd "$SRC_DIR"
make clean 2>/dev/null || true
make
echo "  ✓ time.ko built successfully"

# Step 2: Compile all test programs
echo ""
echo "[2/2] Compiling test programs..."
cd "$BENCH_DIR"
gcc -o test0 test.c    && echo "  ✓ test0 (minimal, ~0 MB)"
gcc -o test1 test1.c   && echo "  ✓ test1 (50 MB)"
gcc -o test2 test2.c   && echo "  ✓ test2 (200 MB)"
gcc -o test3 test3.c   && echo "  ✓ test3 (200 MB)"
gcc -o test4 test4.c   && echo "  ✓ test4 (400 MB)"
gcc -o test5 test5.c   && echo "  ✓ test5 (600 MB)"
gcc -o test6 test6.c   && echo "  ✓ test6 (800 MB)"
gcc -o test7 test7.c   && echo "  ✓ test7 (1024 MB)"
gcc -o test8 test8.c   && echo "  ✓ test8 (1600 MB)"
gcc -o test9 test9.c   && echo "  ✓ test9 (2048 MB)"

echo ""
echo "=========================================="
echo "  Setup complete!"
echo "  Module: $SRC_DIR/time.ko"
echo "  Tests:  $BENCH_DIR/test0 - test9"
echo "=========================================="
