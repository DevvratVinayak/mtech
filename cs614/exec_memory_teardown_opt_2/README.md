# CS614 Artifact: Optimizing execve() Latency via Asynchronous Memory Teardown

This artifact implements an asynchronous memory teardown optimization for the `execve()` system call in the Linux kernel (v6.1.4). By offloading the expensive sequential page table teardown (`exit_mmap`) to a background, high-priority workqueue, we collapse the synchronous latency from **O(n)** to **O(1)**.

## 1. Artifact Directory Structure

```text
codes_experiment/
|-- apply_patch.py           # Python utility to seamlessly apply our kernel modifications
|-- time.c & Makefile        # Custom kretprobe module to trace do_execveat_common and our async logic
|-- benchmark.sh             # The core stress testing mechanism (compiles/runs isolated measurements)
|-- run_experiment.sh        # Full one-click overarching pipeline (Builds modules, sets sysfs, evaluates)
|-- plot_results.py          # Extracts .csv latencies into publication-ready graphs
|-- test.c, test1.c ...      # Benchmark binaries representing BSS process sizes from 0MB to 4096MB
|-- results/                 # Auto-generated. Contains .csv datasets, system facts, and the plotted .pngs
```

## 2. Setup Instructions

**Hardware and Environment Requirements**
- **CPU**: $\geq$ 4 cores (Tested on Intel Skylake, 4 vCPUs)
- **Memory**: `>= 4 GiB` recommended (Test 10 specifically attempts 4096MB to trigger our OOM checks)
- **Storage**: ~15 GiB free (Required for full Linux kernel source and compilation blobs)
- **OS**: Ubuntu (or Debian-based system) executing exactly **Linux 6.1.4**
- **Extra Hardware**: None required

**Kernel Compilation Instructions**
This artifact modifies internal kernel MM/fork logic. You must patch and manually reboot the kernel:

1. Copy this artifact folder (`codes_experiment`) into the VM where you have the downloaded `linux-6.1.4` source.
2. Run the patching utility: `python3 apply_patch.py /path/to/linux-6.1.4`
3. Compile and install manually:
```bash
cd /path/to/linux-6.1.4
make -j$(nproc)
sudo make modules_install
sudo make install
sudo reboot
```
(Verify you booted into 6.1.4 via `uname -r` upon reboot)

## 3. Features & Functionalities Supported

| Feature | Test Scenarios | Objective | Expected Outcome |
| :--- | :--- | :--- | :--- |
| **O(1) Teardown** | Tests 0-9 (0-2048 MB) via `benchmark.sh` | Ensure large teardowns are successfully sent to `WQ_HIGHPRI` | Huge latency reduction (up to 4000x for 2GB) when `exec_async_teardown = 1`. |
| **Minimum Page Guard** | `test.c` (0 MB process) via `benchmark.sh` | Ensure tiny processes skip the costly async queue process. | Both configurations (sync/async) remain sub-millisecond; negligible difference. |
| **OOM Fallback (Safety)** | `test10.c` (4096 MB) on a 3.8GB RAM VM | Evaluate system fault tolerance on memory eviction pressure. | **Graceful Degration**: `execve` refuses to defer memory, falls back to synchronous teardown. `si_mem_available` check avoids kernel panics. |

**Important Findings regarding system stability:** 
During the heavy test bounds (specifically the 4096MB experiment), **no kernel panics, lockups, or deadlocks occurred.** By employing a highly localized bounding heuristic using `si_mem_available()`, the kernel successfully rejected the background optimization on high stress boundaries to eagerly reclaim the mapped physical blocks.

## 4. Unsupported Features and Assumptions
- The async execution requires the caller to be the final user of the memory map (`mm_users == 1`). Standard multi-threaded applications are fully optimized because sibling threads are killed during `de_thread` prior to reaching our teardown logic. However, independent processes sharing the same memory context via raw `clone(CLONE_VM)` will bypass the async teardown.
- We do not employ container/cgroup specific tracking on the queued teardown blocks.

## 5. Getting Started & Evaluating

To run the full suite of experiments and verify our results, you only need to run our automation script. The script will automatically compile the test binaries, load the kernel measurement module, run all tests under both baseline and optimized states, and produce graphic plots.

Step-by-step instructions:
1. Boot into the newly patched kernel.
2. Navigate to this directory:
```bash
cd codes_experiment
```
3. Run the complete automated experiment pipeline (this will run 50 iterations of all 11 binary sizes, collect the stats, and execute `plot_results.py`):
```bash
sudo ./run_experiment.sh 50
```

## 6. Detailed Evaluation Summary

| Objective | How to Run | Estimated Time | Expected Result | Result Access |
| :--- | :--- | :--- | :--- | :--- |
| Full Comparative Benchmark | Execute `sudo ./run_experiment.sh 50` | ~5 Minutes | Complete iterations of 11 process sizes evaluated 50 times each. | Extracted inside `/results/baseline.csv` and `/results/optimized.csv` |
| Graph Plotting & Tables | Handled automatically by `run_experiment.sh` | < 10 seconds | 5 high-quality PNGs tracing speedup arrays and chronological timelines. | Located inside the `/results/` directory as `fig*.png` |
