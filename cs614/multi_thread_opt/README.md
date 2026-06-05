# CS614 Project: Optimizing exec() in Multi-threaded Processes

## Artifact Directory Structure

```
multi_thread_opt/
├── exec_hook.c                        # Kernel module for instrumentation
├── do_test.sh                         # Main automation script (handles build & module loading)
├── test_heavy.c                       # Test: CPU-intensive threads
├── test_nonmain_exec.c                # Test: non-leader calls exec
├── test_double_exec.c                 # Test: consecutive exec calls
├── parse_1.py                         # Data parsing script, used by do_test.sh
├── parse_2.py                         # Data parsing script, used by do_test.sh
├── plot_test.py                       # Individual test plotting, used by do_test.sh
├── plot_comparisons.py                # Comparison plot generator
├── exec_stats.txt                     # Pre-experiment characterization data
├── Makefile                           # Build script
│
├── kernel_modification/               # Kernel modifications
│   ├── fs/
│   ├── kernel/
│   └── exec_opt_multi-thread.patch
│
├── results/                           # Experimental data (current runs)
│   └── <test_name>/
│       ├── original/                  # Original execution results
│       │   ├── logs/
│       │   │   └── final.json         # SIGKILL response time logs
│       │   └── trace/
│       │       └── final.json         # exec syscall trace
│       │
│       │
│       ├── optimized/                 # Optimized execution results
│       │   ├── logs/
│       │   │   └── final.log          # final consolidated runtime logs
│       │   └── trace/
│       │       └── final.json         # processed execution trace summary
│       │
│       │
│       └── *.png                      # single-test plots
│
├── pre_results/                       # Previously executed results (past runs on our machine)
│   └── <test_name>/
│       ├── original/
│       │   ├── logs/
│       │   │   └── final.log
│       │   └── trace/
│       │       └── final.json
│       │
│       │
│       ├── optimized/
│       │   ├── logs/
│       │   │   └── final.log
│       │   └── trace/
│       │       └── final.json
│       │
│       │
│       └── *.png
│
├── plots/                             # Generated comparison plots (current runs)
│   └── *_comparison_*.png
│
├── pre_plots/                         # Previously generated comparison plots (on our machine)
│   └── *_comparison_*.png
│
└── README.md                          # This file
```

---

## System Requirements

| Component      | Requirement  |
| -------------- | ------------ |
| CPU            | 2+ cores     |
| RAM            | 4 GB minimum |
| Kernel Version | Linux 6.1.4  |

---

## Setup Instructions

### 1. Software Dependencies

```bash

# Install Python dependencies for plotting
sudo apt install -y python3 python3-pip
pip3 install matplotlib numpy pandas
```

### 2. Compile Modified Kernel

Navigate to your kernel source directory and apply modifications:

#### Option 1: Apply Patch

```bash
cd /usr/src/linux-6.1.4/    # Your kernel source directory

# Apply the patch
git apply /path/to/multi_thread_opt/kernel_modification/exec_opt_multi-thread.patch

# Build kernel
make -j$(nproc)
```

#### Option 2: Replace Modified Files

```bash
cd /usr/src/linux-6.1.4/    # Your kernel source directory

# Copy modified files from kernel_modification/fs/ to kernel source fs/
# Copy modified files from kernel_modification/kernel/ to kernel source kernel/

# Build kernel
make -j$(nproc)
```

#### If Build Fails Due to "declaration after statement" Error

```bash
# Disable the error and rebuild
make -j$(nproc) KCFLAGS="-Wno-error=declaration-after-statement"
```

### 3. Install and Reboot

```bash
# Install kernel modules and kernel
sudo make modules_install
sudo make install

# Update bootloader
sudo update-grub

# Reboot into modified kernel
sudo reboot
```

---

## Test Cases

| Test                | Description                                            | Purpose                                            |
| ------------------- | ------------------------------------------------------ | -------------------------------------------------- |
| `test_heavy`        | Threads perform floating-point computation before exec | Test under CPU-intensive workload                  |
| `test_nonmain_exec` | Non-leader thread calls exec                           | Validate correctness with non-main thread          |
| `test_double_exec`  | Two consecutive exec calls with thread respawn         | Validate correctness with multiple exec operations |

---

## Getting Started (30 Minutes)

### Quick Test

```bash
cd multi_thread_opt/

# Run test_heavy with 3 threads
# Script automatically builds module, loads it, runs tests
./do_test.sh test_heavy 3

# Expected runtime: ~5 minutes
```

### Check Results

```bash
# View results structure
ls results/test_heavy/3/
# Should show: original/ and optimized/ directories

# Check logs and traces
ls results/test_heavy/3/original/logs/     # final.json and log files
ls results/test_heavy/3/original/trace/    # final.json and trace files (SIGKILL response time)

# View plots
ls plots/test_heavy/3/
# Should contain: comparison plots for this test config
```

**Expected Outcome:** Plots show reduced waiting time in optimized mode compared to original mode. however, this improvement may not be clearly visible in the mean plots due to scheduling variability. For a more reliable comparison, refer to the median plots. Additionally, review the raw `trace_<i>.txt` files for both the original and optimized versions, where it is evident that thread cleanup in the optimized version occurs after exec system call execution completes. Also run with higher nuber of thread count `./do_test.sh all 15` to see improvement.

---

## Running Experiments

### Syntax

```bash
./do_test.sh <test_name> [thread_count]
```

**Note:** Script automatically handles:

- Building kernel module and test programs
- Loading the kernel module
- Running tests with both original and optimized modes
- Parsing results
- Generating plots

### Examples

```bash
# Run specific test with specific thread count
./do_test.sh test_heavy 4

# Run specific test with default thread counts (1-5)
./do_test.sh test_nonmain_exec

# Run all tests with default thread counts (1-5)
./do_test.sh all

# Run all tests with specific thread count
./do_test.sh all 6
```

### Configuration

Edit `do_test.sh` to modify:

- `TRACE_NUM=30` — Number of iterations per test (default: 30)
- Default thread counts: 1-5

**Runtime Estimates:**

- Single test, single thread count: ~5 minutes
- Single test, all thread counts (1-5): ~20 minutes
- All tests, all thread counts: ~45 minutes

---

## Generating Comparison Plots

After running experiments:

```bash
cd multi_thread_opt/

# Generate comparison plots across thread counts
python3 plot_comparisons.py
```

This creates comparison visualizations in `plots/<test_name>/` showing original vs optimized performance across different thread counts. These plots will only show till thread count 5 for more than count 5 checkout respective result directory.

---

## Result Organization

### Directory Structure

```
results/<test_name>/<thread_count>/
├── original/
│   ├── logs/
│   │   ├── final.json          # Aggregated timing data
│   │   └── log_<i>.txt         # Raw log files
│   └── trace/
│       ├── final.json          # SIGKILL response time data
│       └── trace_<i>.txt       # Raw trace files
└── optimized/
    ├── logs/
    │   ├── final.json
    │   └── log_<i>.txt
    └── trace/
        ├── final.json
        └── trace_<i>.txt
```

### Data Files

- **logs/final.json** - Aggregated timing measurements SIGKILL responsse time
- **logs/\*log\_<i>.txt** - Raw response time logs
- **trace/final.json** - Aggregated exec characterization data
- **trace/\*log\_<i>.txt** - Raw trace data for time analysis

### Plots

```
plots/<test_name>/<thread_count>/
└── comparison_*.png            # Original vs optimized comparison
```

---

## Expected Results

### Performance Improvements

- **Waiting time reduction:** The optimized version reduces waiting time; however, this improvement may not be clearly visible in the mean plots due to scheduling variability. For a more reliable comparison, refer to the median plots. Additionally, review the raw `log_<i>.txt` files for both the original and optimized versions, where it is evident that thread cleanup in the optimized version occurs after exec system call execution completes.

### Correctness

- All tests complete without crashes
- No deadlocks or assertion failures
- Optimization works correctly with non-main thread exec and double exec scenarios

---

## Module Control via sysfs

The kernel module exposes control interfaces at:

```bash
/sys/kernel/cs614hook/tracked_pid       # PID to track
/sys/kernel/cs614hook/optimization_on   # 1=optimized, 0=original
```

**Note:** The `do_test.sh` script handles all sysfs interaction automatically. Manual control is not needed for normal operation.

---

## Troubleshooting

### Python Plotting Errors

```bash
# Install missing dependencies
pip3 install matplotlib numpy pandas

# Verify installation
python3 -c "import matplotlib, numpy, pandas; print('OK')"
```

### Kernel Build Issues

```bash
# If "declaration after statement" error occurs
make -j$(nproc) KCFLAGS="-Wno-error=declaration-after-statement"

# If other warnings cause build failure
make -j$(nproc) KCFLAGS="-Wno-error"
```

---

## Known Limitations

1. **Instrumentation overhead:** Timing includes instrumentation;
2. **Single process focus:** Tests target single-process, multi-threaded scenarios

---

## Quick Reference

### Essential Commands

```bash
# Compile kernel (after applying patch or replacing files)
cd /usr/src/linux-6.1.4/
make -j$(nproc) KCFLAGS="-Wno-error=declaration-after-statement"
sudo make modules_install && sudo make install
sudo update-grub && sudo reboot

# Quick test (after reboot)
cd multi_thread_opt/
./do_test.sh test_heavy 3

# Full evaluation
./do_test.sh all
python3 plot_comparisons.py
```
