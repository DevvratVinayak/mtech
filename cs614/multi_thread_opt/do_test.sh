#!/bin/bash
set -euo pipefail
MODULE_NAME="exec_hook"

TESTS=(
    test_heavy test_double_exec test_nonmain_exec
)

MODES=(0 1)   # 0 = ORIGINAL, 1 = OPTIMIZED

# =========================
# PRINT AVAILABLE TESTS
# =========================
print_available_tests() {
    echo ""
    echo "[AVAILABLE TESTS]"
    for t in "${TESTS[@]}"; do
        echo "  - $t"
    done
    echo ""
}

# =========================
# USAGE FUNCTION
# =========================
usage() {
    echo "=================================================="
    echo "             EXEC HOOK TEST SUITE"
    echo "=================================================="
    echo ""
    echo "USAGE:"
    echo "  $0 <test_name|all> [NUM_THREADS]"
    echo ""
    echo "ARGUMENTS:"
    echo "  test_name    -> specific test or 'all'"
    echo "  NUM_THREADS  -> optional (default: 1 to 5)"
    echo ""
    print_available_tests
    echo "EXAMPLES:"
    echo "  $0 all 7"
    echo "  $0 test_heavy 4"
    echo "  $0 all"
    echo ""
    echo "=================================================="
    exit 1
}

# =========================
# ARG CHECK
# =========================
if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "[ERROR] Invalid arguments!"
    usage
fi

TEST_NAME=$1

# =========================
# THREAD HANDLING
# =========================
if [ $# -eq 2 ]; then
    THREAD_LIST=("$2")
else
    THREAD_LIST=(1 2 3 4 5)
fi

# =========================
# VALIDATE TEST NAME
# =========================
if [ "$TEST_NAME" == "all" ]; then
    RUN_TESTS=("${TESTS[@]}")
else
    VALID=0
    for t in "${TESTS[@]}"; do
        if [[ "$t" == "$TEST_NAME" ]]; then
            VALID=1
            break
        fi
    done

    if [ "$VALID" -eq 0 ]; then
        echo "[ERROR] Invalid test name: $TEST_NAME"
        print_available_tests
        exit 1
    fi

    RUN_TESTS=("$TEST_NAME")
fi

# =========================
# MODULE HANDLING
# =========================
echo "[INFO] Checking kernel module..."

if lsmod | grep -q "^${MODULE_NAME}"; then
    echo "[INFO] Removing existing module..."
    sudo rmmod ${MODULE_NAME}
fi

echo "[INFO] Building project..."

make clean
make || {
    echo "[ERROR] Build failed during make"
    exit 1
}

# =========================
# VERIFY BINARIES
# =========================
echo "[INFO] Verifying compiled test binaries..."

for TEST in "${RUN_TESTS[@]}"; do
    if [[ ! -x "./${TEST}" ]]; then
        echo "[ERROR] Missing compiled binary: ${TEST}"
        exit 1
    fi
done

echo "[SUCCESS] All test binaries compiled successfully"

# =========================
# INSERT MODULE
# =========================
echo "[INFO] Inserting kernel module..."
sudo insmod ${MODULE_NAME}.ko || {
    echo "[ERROR] Failed to insert kernel module!"
    exit 1
}

echo "[SUCCESS] Module loaded successfully"
echo ""

# =========================
# RUN TESTS
# =========================
TRACE_NUM=30

for TEST in "${RUN_TESTS[@]}"; do

    echo ""
    echo "=================================================="
    echo " TEST: $TEST"
    echo "=================================================="

    for THREADS in "${THREAD_LIST[@]}"; do

        echo ""
        echo "---------------- THREADS: $THREADS ----------------"

        for MODE in "${MODES[@]}"; do

            if [ "$MODE" -eq 1 ]; then
                MODE_NAME="OPTIMIZED"
            else
                MODE_NAME="ORIGINAL"
            fi

            echo ""
            echo "[MODE] $MODE_NAME"

            RUN_DIR="./results/${TEST}/${THREADS}/${MODE_NAME,,}"
            mkdir -p "${RUN_DIR}/traces"
            mkdir -p "${RUN_DIR}/logs"

            for i in $(seq 1 ${TRACE_NUM}); do
                echo "[INFO] Run #$i"

                sudo dmesg -C

                echo "[INFO] Executing ./${TEST} ${THREADS} ${MODE}"
                sudo ./"${TEST}" ${THREADS} ${MODE}

                echo "[INFO] Capturing kernel trace..."
                sudo cat /proc/cs614hook_log > "${RUN_DIR}/traces/trace_${i}.txt"

                echo "[INFO] Saving dmesg log..."
                sudo dmesg > "${RUN_DIR}/logs/log_${i}.txt"

                sudo dmesg -C

            done

            echo "[INFO] Running analysis scripts..."
            python3 parse_1.py "${RUN_DIR}" ${TRACE_NUM}
            python3 parse_2.py "${RUN_DIR}" ${TRACE_NUM}

            # rm -f "${RUN_DIR}"/traces/trace_*
            # rm -f "${RUN_DIR}"/logs/log_*

            echo "[DONE] $TEST | Threads: $THREADS | $MODE_NAME"
        done

        echo "[INFO] Generating plots..."
        python3 plot_test.py "${TEST}" "${THREADS}"

    done

done

# =========================
# CLEANUP
# =========================
echo "[INFO] Removing kernel module..."
sudo rmmod ${MODULE_NAME}
echo "[SUCCESS] All tests completed successfully"
echo "Results saved in: ./results/<test_name>/<threads>/<org|opt>/"
