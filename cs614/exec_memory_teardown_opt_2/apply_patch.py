#!/usr/bin/env python3
"""
Apply exec_deferred_mmput patch to Linux 6.1.4 kernel source.
Usage: python3 apply_patch.py [/path/to/linux-6.1.4]
"""

import sys
import os
import shutil

KERN_DIR = sys.argv[1] if len(sys.argv) > 1 else "/home/rohit/linux-6.1.4"

def read_file(path):
    with open(path, 'r') as f:
        return f.readlines()

def write_file(path, lines):
    with open(path, 'w') as f:
        f.writelines(lines)

def find_line(lines, pattern, start=0):
    """Find first line containing pattern, starting from 'start' index."""
    for i in range(start, len(lines)):
        if pattern in lines[i]:
            return i
    return -1

def backup(path):
    bak = path + ".orig"
    if not os.path.exists(bak):
        shutil.copy2(path, bak)
        print(f"  Backed up: {os.path.basename(path)}")

# ============================================================
# Verify source
# ============================================================
fork_path = os.path.join(KERN_DIR, "kernel/fork.c")
mmh_path = os.path.join(KERN_DIR, "include/linux/sched/mm.h")
sysctl_path = os.path.join(KERN_DIR, "kernel/sysctl.c")
exec_path = os.path.join(KERN_DIR, "fs/exec.c")

for p in [fork_path, mmh_path, sysctl_path, exec_path]:
    if not os.path.exists(p):
        print(f"ERROR: {p} not found")
        sys.exit(1)

# Check if already patched
fork_lines = read_file(fork_path)
fork_text = ''.join(fork_lines)
if 'exec_deferred_mmput' in fork_text:
    print("ERROR: fork.c already contains exec_deferred_mmput.")
    print("       Source appears already patched. Restore from .orig files first.")
    sys.exit(1)

print(f"=== Applying patch to {KERN_DIR} ===\n")

# ============================================================
# FILE 1: kernel/fork.c
# ============================================================
print("--- kernel/fork.c ---")
backup(fork_path)
lines = read_file(fork_path)

# --- 1A: Add includes ---
include_idx = find_line(lines, '#include <linux/slab.h>')
if include_idx < 0:
    include_idx = find_line(lines, '#include <linux/mm.h>')
if include_idx < 0:
    print("ERROR: Cannot find include anchor in fork.c")
    sys.exit(1)

# Check if already present
if find_line(lines, '#include <linux/proc_fs.h>') < 0:
    lines.insert(include_idx + 1, '#include <linux/proc_fs.h>\n')
    lines.insert(include_idx + 2, '#include <linux/seq_file.h>\n')
    print("  [1A] Added proc_fs.h and seq_file.h includes")
else:
    print("  [1A] Includes already present, skipping")

# --- 1B: Add declarations BEFORE fork_init ---
fork_init_idx = find_line(lines, 'void __init fork_init')
if fork_init_idx < 0:
    print("ERROR: Cannot find fork_init in fork.c")
    sys.exit(1)

declarations = r"""
/*
 * ============================================================
 * exec_deferred_mmput: Two-phase deferred mm teardown for execve
 * ============================================================
 */

/* Sysctl toggles */
int sysctl_exec_async_teardown __read_mostly;
int sysctl_exec_async_min_pages __read_mostly = 1024;
int sysctl_exec_async_oom_shift __read_mostly = 4;
int sysctl_exec_async_max_pending __read_mostly = 64;

/* Dedicated workqueue */
static struct workqueue_struct *exec_teardown_wq;

/* Backpressure */
static atomic_t teardown_pending = ATOMIC_INIT(0);

/* Statistics */
static atomic64_t teardown_async_count = ATOMIC64_INIT(0);
static atomic64_t teardown_sync_count = ATOMIC64_INIT(0);

/* Phase 2 latency tracking (microseconds) */
static atomic64_t teardown_total_us = ATOMIC64_INIT(0);
static atomic64_t teardown_max_us = ATOMIC64_INIT(0);

/* Deferred process size tracking */
static atomic64_t teardown_total_pages = ATOMIC64_INIT(0);

/* Latency histogram */
#define EXEC_TEARDOWN_LAT_BUCKETS 5

static atomic64_t latency_bucket[EXEC_TEARDOWN_LAT_BUCKETS] = {
	ATOMIC64_INIT(0), ATOMIC64_INIT(0), ATOMIC64_INIT(0),
	ATOMIC64_INIT(0), ATOMIC64_INIT(0)
};

/* /proc/exec_teardown_stats */
static int teardown_stats_show(struct seq_file *m, void *v)
{
	s64 async;

	seq_printf(m, "enabled:          %d\n", READ_ONCE(sysctl_exec_async_teardown));
	seq_printf(m, "min_pages:        %d\n", READ_ONCE(sysctl_exec_async_min_pages));
	seq_printf(m, "oom_shift:        %d\n", READ_ONCE(sysctl_exec_async_oom_shift));
	seq_printf(m, "max_pending:      %d\n", READ_ONCE(sysctl_exec_async_max_pending));
	seq_printf(m, "---\n");
	seq_printf(m, "async_teardowns:  %lld\n", atomic64_read(&teardown_async_count));
	seq_printf(m, "sync_teardowns:   %lld\n", atomic64_read(&teardown_sync_count));
	seq_printf(m, "pending:          %d\n", atomic_read(&teardown_pending));
	seq_printf(m, "---\n");
	/* Non-atomic snapshot -- approximate averages */
	async = atomic64_read(&teardown_async_count);
	seq_printf(m, "avg_teardown_us:  %lld\n",
		   async ? atomic64_read(&teardown_total_us) / async : 0);
	seq_printf(m, "max_teardown_us:  %lld\n", atomic64_read(&teardown_max_us));
	seq_printf(m, "avg_vm_pages:     %lld\n",
		   async ? atomic64_read(&teardown_total_pages) / async : 0);
	seq_printf(m, "---\n");
	seq_printf(m, "lat_<1ms:         %lld\n", atomic64_read(&latency_bucket[0]));
	seq_printf(m, "lat_1-5ms:        %lld\n", atomic64_read(&latency_bucket[1]));
	seq_printf(m, "lat_5-20ms:       %lld\n", atomic64_read(&latency_bucket[2]));
	seq_printf(m, "lat_20-100ms:     %lld\n", atomic64_read(&latency_bucket[3]));
	seq_printf(m, "lat_>100ms:       %lld\n", atomic64_read(&latency_bucket[4]));
	return 0;
}

static int teardown_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, teardown_stats_show, NULL);
}

/* Write handler: any non-zero write resets all counters */
static ssize_t teardown_stats_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos)
{
	int i;
	if (!count)
		return 0;
	atomic64_set(&teardown_async_count, 0);
	atomic64_set(&teardown_sync_count, 0);
	atomic64_set(&teardown_total_us, 0);
	atomic64_set(&teardown_max_us, 0);
	atomic64_set(&teardown_total_pages, 0);
	for (i = 0; i < EXEC_TEARDOWN_LAT_BUCKETS; i++)
		atomic64_set(&latency_bucket[i], 0);
	return count;
}

static const struct proc_ops teardown_stats_ops = {
	.proc_open    = teardown_stats_open,
	.proc_read    = seq_read,
	.proc_write   = teardown_stats_write,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

"""

# Re-find fork_init after include insertion
fork_init_idx = find_line(lines, 'void __init fork_init')
decl_lines = [l + '\n' for l in declarations.split('\n')]
for i, dl in enumerate(decl_lines):
    lines.insert(fork_init_idx + i, dl)
print(f"  [1B] Declarations inserted before fork_init (line {fork_init_idx+1})")

# --- 1C: alloc_workqueue inside fork_init, before uprobes_init ---
uprobes_idx = find_line(lines, 'uprobes_init()')
if uprobes_idx < 0:
    print("ERROR: Cannot find uprobes_init() in fork.c")
    sys.exit(1)

wq_code = [
    '\t/* Init dedicated workqueue (NO WQ_SYSFS -- not safe this early).\n',
    '\t * Built-in: no destroy_workqueue() needed -- lives for kernel lifetime. */\n',
    '\texec_teardown_wq = alloc_workqueue("exec_teardown",\n',
    '\t\t\tWQ_UNBOUND | WQ_HIGHPRI | WQ_MEM_RECLAIM, 0);\n',
    '\tif (!exec_teardown_wq)\n',
    '\t\tpr_warn("exec_teardown: workqueue alloc failed, async disabled\\n");\n',
    '\n',
]
for i, wl in enumerate(wq_code):
    lines.insert(uprobes_idx + i, wl)
print(f"  [1C] Workqueue init inserted before uprobes_init (line {uprobes_idx+1})")

# --- 1D: Functions after EXPORT_SYMBOL_GPL(mmput_async) ---
export_idx = find_line(lines, 'EXPORT_SYMBOL_GPL(mmput_async)')
if export_idx < 0:
    print("ERROR: Cannot find EXPORT_SYMBOL_GPL(mmput_async) in fork.c")
    sys.exit(1)

functions = r"""
/* Latency bucket update: <1ms, 1-5ms, 5-20ms, 20-100ms, >100ms */
static void update_latency_bucket(s64 us)
{
	int b = (us < 1000) ? 0 :
		(us < 5000) ? 1 :
		(us < 20000) ? 2 :
		(us < 100000) ? 3 : 4;
	atomic64_inc(&latency_bucket[b]);
}

/*
 * Phase 2 work handler: Heavy page table teardown (runs in kworker).
 * Phase 1 (uprobe/aio/ksm/khugepaged) already completed synchronously.
 */
static void exec_deferred_mmput_fn(struct work_struct *work)
{
	struct mm_struct *mm = container_of(work, struct mm_struct,
					    async_put_work);
	/* Capture size BEFORE exit_mmap reduces total_vm during unmap */
	unsigned long vm_pages = mm->total_vm;
	ktime_t t0 = ktime_get();

	/* Walk all page tables, free all physical pages */
	exit_mmap(mm);

	/* Release huge zero page reference */
	mm_put_huge_zero_page(mm);

	/* Release executable file reference */
	set_mm_exe_file(mm, NULL);

	/* Remove from global mm list */
	if (!list_empty(&mm->mmlist)) {
		spin_lock(&mmlist_lock);
		list_del(&mm->mmlist);
		spin_unlock(&mmlist_lock);
	}

	/* Release binary format module */
	if (mm->binfmt)
		module_put(mm->binfmt->module);

	/* Unregister from LRU generation tracking */
	lru_gen_del_mm(mm);

	/* Free the mm_struct and its pgd */
	mmdrop(mm);

	/*
	 * Track latency AFTER mmdrop -- mm is freed, only global atomics.
	 * Measures: exit_mmap + cleanup + mmdrop (full Phase 2 cost).
	 */
	{
		s64 us = ktime_us_delta(ktime_get(), t0);
		s64 old_max;
		atomic64_add(us, &teardown_total_us);
		do {
			old_max = atomic64_read(&teardown_max_us);
		} while (us > old_max &&
			 atomic64_cmpxchg(&teardown_max_us, old_max, us) != old_max);
		update_latency_bucket(us);
	}

	/* Track deferred process size (using pre-teardown snapshot) */
	atomic64_add(vm_pages, &teardown_total_pages);

	/* Decrement pending LAST -- signals work is fully complete */
	atomic_dec(&teardown_pending);
}

/**
 * exec_deferred_mmput - two-phase mm teardown for execve
 * @mm: the old mm_struct being replaced during exec
 *
 * Phase 1 (sync): uprobe, aio, ksm, khugepaged -- O(1)
 * Phase 2 (async): exit_mmap page table walk -- O(n)
 *
 * Conditions: sysctl enabled, workqueue ready, process large enough,
 * queue not full, enough free memory, work_struct not already queued.
 */
void exec_deferred_mmput(struct mm_struct *mm)
{
	if (!atomic_dec_and_test(&mm->mm_users))
		return;

	/* mm_users reached zero -- we own this mm exclusively */

	if (READ_ONCE(sysctl_exec_async_teardown) &&
	    exec_teardown_wq &&
	    mm->total_vm > READ_ONCE(sysctl_exec_async_min_pages) &&
	    atomic_read(&teardown_pending) < READ_ONCE(sysctl_exec_async_max_pending)) {
		/* Defensive clamp: never allow shift 0 (=all RAM) or >31 */
		int shift = clamp(READ_ONCE(sysctl_exec_async_oom_shift), 1, 8);

		if (si_mem_available() > (totalram_pages() >> shift) &&
		    !WARN_ON(work_pending(&mm->async_put_work))) {

			/* Phase 1 (sync): fast subsystem cleanup */
			uprobe_clear_state(mm);
			exit_aio(mm);
			ksm_exit(mm);
			khugepaged_exit(mm);

			/* Increment BEFORE queue_work -- prevents negative count race */
			atomic_inc(&teardown_pending);

			/* Phase 2 (async): queue heavy teardown */
			INIT_WORK(&mm->async_put_work, exec_deferred_mmput_fn);
			queue_work(exec_teardown_wq, &mm->async_put_work);

			atomic64_inc(&teardown_async_count);
			return;
		}
	}

	/* Synchronous fallback (stock kernel behavior) */
	__mmput(mm);
	if (READ_ONCE(sysctl_exec_async_teardown))
		atomic64_inc(&teardown_sync_count);
}
EXPORT_SYMBOL_GPL(exec_deferred_mmput);

"""

func_lines = [l + '\n' for l in functions.split('\n')]
for i, fl in enumerate(func_lines):
    lines.insert(export_idx + 1 + i, fl)
print(f"  [1D] Functions inserted after EXPORT_SYMBOL_GPL(mmput_async) (line {export_idx+1})")

# --- 1E: late_initcall at the end ---
late_initcall = r"""
/*
 * Register /proc/exec_teardown_stats.
 * Must use late_initcall because procfs is not ready during fork_init().
 */
static int __init exec_teardown_proc_init(void)
{
	struct proc_dir_entry *pde;
	pde = proc_create("exec_teardown_stats", 0644, NULL, &teardown_stats_ops);
	if (!pde) {
		pr_warn("exec_teardown: failed to create /proc stats\n");
		return -ENOMEM;
	}
	return 0;
}
late_initcall(exec_teardown_proc_init);
"""

lines.append('\n')
for l in late_initcall.split('\n'):
    lines.append(l + '\n')
print("  [1E] late_initcall appended at end of file")

write_file(fork_path, lines)
print("  fork.c DONE\n")

# ============================================================
# FILE 2: include/linux/sched/mm.h
# ============================================================
print("--- include/linux/sched/mm.h ---")
backup(mmh_path)
lines = read_file(mmh_path)

mmput_async_idx = find_line(lines, 'mmput_async')
if mmput_async_idx < 0:
    print("ERROR: Cannot find mmput_async in mm.h")
    sys.exit(1)

lines.insert(mmput_async_idx + 1,
    '/* Two-phase deferred mm teardown for exec */\n')
lines.insert(mmput_async_idx + 2,
    'void exec_deferred_mmput(struct mm_struct *);\n')
print(f"  Declaration added after mmput_async (line {mmput_async_idx+1})")

write_file(mmh_path, lines)
print("  mm.h DONE\n")

# ============================================================
# FILE 3: kernel/sysctl.c
# ============================================================
print("--- kernel/sysctl.c ---")
backup(sysctl_path)
lines = read_file(sysctl_path)

# Find kern_table and insert externs + bounds BEFORE it
kern_table_idx = find_line(lines, 'static struct ctl_table kern_table')
if kern_table_idx < 0:
    print("ERROR: Cannot find kern_table in sysctl.c")
    sys.exit(1)

externs = """extern int sysctl_exec_async_teardown;
extern int sysctl_exec_async_min_pages;
extern int sysctl_exec_async_oom_shift;
extern int sysctl_exec_async_max_pending;

static int oom_shift_min = 1, oom_shift_max = 8;
/* Min: 256 pages (1MB). Max: 2^20 pages (4GB). Bounds the gate threshold. */
static int min_pages_min = 256, min_pages_max = 1 << 20;
static int max_pending_min = 1, max_pending_max = 1024;

"""

extern_lines = [l + '\n' for l in externs.split('\n')]
for i, el in enumerate(extern_lines):
    lines.insert(kern_table_idx + i, el)
print(f"  Externs + bounds inserted before kern_table (line {kern_table_idx+1})")

# Find the "panic" procname entry as anchor (always present, outside #ifdef)
panic_idx = -1
for i in range(len(lines)):
    if '"panic"' in lines[i] and 'procname' in lines[i]:
        panic_idx = i
        break

if panic_idx < 0:
    print("ERROR: Cannot find 'panic' entry in kern_table")
    sys.exit(1)

# Walk backwards to find the opening { of the panic entry
insert_idx = panic_idx
for j in range(panic_idx - 1, max(panic_idx - 5, 0), -1):
    if '{' in lines[j] and lines[j].strip().startswith('{'):
        insert_idx = j
        break

sysctl_entries = """	{
		.procname	= "exec_async_teardown",
		.data		= &sysctl_exec_async_teardown,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "exec_async_teardown_min_pages",
		.data		= &sysctl_exec_async_min_pages,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &min_pages_min,
		.extra2		= &min_pages_max,
	},
	{
		.procname	= "exec_async_teardown_oom_shift",
		.data		= &sysctl_exec_async_oom_shift,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &oom_shift_min,
		.extra2		= &oom_shift_max,
	},
	{
		.procname	= "exec_async_teardown_max_pending",
		.data		= &sysctl_exec_async_max_pending,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= &max_pending_min,
		.extra2		= &max_pending_max,
	},
"""

entry_lines = [l + '\n' for l in sysctl_entries.split('\n')]
for i, el in enumerate(entry_lines):
    lines.insert(insert_idx + i, el)
print(f"  Sysctl entries inserted before 'panic' entry (line {insert_idx+1})")

write_file(sysctl_path, lines)
print("  sysctl.c DONE\n")

# ============================================================
# FILE 4: fs/exec.c
# ============================================================
print("--- fs/exec.c ---")
backup(exec_path)
lines = read_file(exec_path)

# Find mmput(old_mm) and replace
for i in range(len(lines)):
    if 'mmput(old_mm)' in lines[i] and 'exec_deferred' not in lines[i]:
        original = lines[i]
        if lines[i].strip().startswith('//'):
            # Already commented out, skip
            continue
        lines[i] = lines[i].replace('mmput(old_mm)', 'exec_deferred_mmput(old_mm)')
        print(f"  Replaced mmput(old_mm) -> exec_deferred_mmput(old_mm) (line {i+1})")
        break
else:
    # Check if already patched or commented
    commented_idx = find_line(lines, '//mmput(old_mm)')
    if commented_idx < 0:
        commented_idx = find_line(lines, '// mmput(old_mm)')
    if commented_idx >= 0:
        # Already commented, add our call after if not present
        if find_line(lines, 'exec_deferred_mmput(old_mm)') < 0:
            lines.insert(commented_idx + 1, '\t\texec_deferred_mmput(old_mm);\n')
            print(f"  Added exec_deferred_mmput after commented mmput (line {commented_idx+2})")
    else:
        print("WARNING: Could not find mmput(old_mm) in exec.c")

write_file(exec_path, lines)
print("  exec.c DONE\n")

# ============================================================
# VERIFICATION
# ============================================================
print("=" * 60)
print("VERIFICATION")
print("=" * 60)

lines = read_file(fork_path)
fork_text = ''.join(lines)

checks = [
    ("exec_deferred_mmput_fn",     "Phase 2 work handler"),
    ("exec_deferred_mmput",        "Main function"),
    ("EXPORT_SYMBOL_GPL(exec_deferred_mmput)", "Symbol export"),
    ("sysctl_exec_async_teardown", "Sysctl variable"),
    ("exec_teardown_wq",           "Workqueue variable"),
    ("teardown_pending",           "Backpressure counter"),
    ("teardown_stats_ops",         "Proc ops struct"),
    ("late_initcall",              "Late initcall"),
    ("alloc_workqueue",            "Workqueue init"),
    ("EXEC_TEARDOWN_LAT_BUCKETS",  "Histogram macro"),
    ("update_latency_bucket",      "Histogram function"),
    ("READ_ONCE(sysctl_exec_async_teardown)", "READ_ONCE usage"),
]

all_ok = True
for pattern, desc in checks:
    if pattern in fork_text:
        print(f"  ✓ fork.c: {desc}")
    else:
        print(f"  ✗ fork.c: {desc} MISSING!")
        all_ok = False

# Check ordering
fork_init_line = 0
decl_line = 0
for i, l in enumerate(lines):
    if 'void __init fork_init' in l:
        fork_init_line = i
    if 'sysctl_exec_async_teardown __read_mostly' in l and decl_line == 0:
        decl_line = i

if decl_line > 0 and fork_init_line > 0:
    if decl_line < fork_init_line:
        print(f"  ✓ Declarations (line {decl_line+1}) ABOVE fork_init (line {fork_init_line+1})")
    else:
        print(f"  ✗ Declarations (line {decl_line+1}) BELOW fork_init (line {fork_init_line+1})!")
        all_ok = False

# Check no proc_create in fork_init (but allow it in late_initcall functions)
in_fork_init = False
fork_init_brace_depth = 0
for i, l in enumerate(lines):
    if 'void __init fork_init' in l:
        in_fork_init = True
        fork_init_brace_depth = 0
    if in_fork_init:
        fork_init_brace_depth += l.count('{') - l.count('}')
        if 'proc_create' in l:
            print(f"  ✗ proc_create found in fork_init at line {i+1}!")
            all_ok = False
        if fork_init_brace_depth <= 0 and '{' in ''.join(lines[max(0,i-5):i+1]):
            in_fork_init = False  # exited fork_init

# Verify proc_create IS in late_initcall (not fork_init)
proc_create_line = find_line(lines, 'proc_create("exec_teardown_stats"')
late_initcall_line = find_line(lines, 'late_initcall(exec_teardown_proc_init)')
if proc_create_line > 0 and late_initcall_line > 0 and proc_create_line < late_initcall_line:
    print("  ✓ proc_create is in late_initcall (not fork_init)")
else:
    print("  ✗ proc_create / late_initcall placement issue!")
    all_ok = False

# Check other files
mmh_text = ''.join(read_file(mmh_path))
if 'exec_deferred_mmput' in mmh_text:
    print("  ✓ mm.h: Declaration present")
else:
    print("  ✗ mm.h: Declaration MISSING!")
    all_ok = False

sysctl_text = ''.join(read_file(sysctl_path))
if 'exec_async_teardown' in sysctl_text:
    print("  ✓ sysctl.c: Entries present")
else:
    print("  ✗ sysctl.c: Entries MISSING!")
    all_ok = False

exec_text = ''.join(read_file(exec_path))
if 'exec_deferred_mmput(old_mm)' in exec_text:
    print("  ✓ exec.c: Call site updated")
else:
    print("  ✗ exec.c: Call site NOT updated!")
    all_ok = False

print()
if all_ok:
    print("ALL CHECKS PASSED ✓")
    print()
    print("Next steps:")
    print(f"  cd {KERN_DIR}")
    print("  sudo rm -f kernel/fork.o kernel/sysctl.o fs/exec.o")
    print("  sudo make -j$(nproc)")
    print("  sudo make modules_install && sudo make install")
    print("  sudo reboot")
else:
    print("SOME CHECKS FAILED ✗ — review the output above")
    print("Restore originals with:")
    for p in [fork_path, mmh_path, sysctl_path, exec_path]:
        print(f"  cp {p}.orig {p}")
