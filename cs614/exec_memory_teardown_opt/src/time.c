/*
 * time.c — async mm destruction kthread + exec time tracker
 *
 * ARCHITECTURE
 * ============
 *
 * Goal: offload mm teardown work (normally done synchronously inside
 * mmput → exit_mmap) to a dedicated kernel thread, so the process exit
 * path returns faster.
 *
 * INTERCEPTION POINT: mmput()
 * ---------------------------
 *   mmput() calls __mmput → exit_mmap only when mm_users hits 0:
 *
 *       void mmput(struct mm_struct *mm) {
 *           if (atomic_dec_and_test(&mm->mm_users)) {
 *               __mmput(mm);   // exit_mmap + ksm_exit + khugepaged + aio + mmdrop
 *           }
 *       }
 *
 *   Our kprobe pre_handler fires BEFORE the atomic_dec_and_test.
 *   When mm_users == 1 (this is the final put):
 *     a. mmgrab(mm)              — bump mm_count so struct stays alive
 *     b. atomic_inc(&mm->mm_users) — make it 2; mmput's dec gives 1,
 *                                    not 0, so __mmput is NOT called
 *     c. queue mm to our kthread
 *
 *   The kthread runs the full __mmput sequence asynchronously:
 *     exit_mmap, ksm_exit, khugepaged_exit, exit_aio, mmdrop
 *
 * SAFETY MECHANISMS
 * -----------------
 *   S1. enabled toggle: /sys/kernel/time/enabled (default 1)
 *       Runtime disable without unloading.
 *
 *   S2. OOM check: free RAM < oom_threshold_kb → sync fallback.
 *       Reasoning: under memory pressure, deferring page frees worsens
 *       things; release pages immediately instead.
 *
 *   S3. Complete cleanup: kthread calls ksm_exit, khugepaged_exit, and
 *       exit_aio in addition to exit_mmap. Matches __mmput().
 *
 * OPTIMIZATIONS
 * -------------
 *   O1. Adaptive threshold (min_vm_pages, default 128 pages = 512 KB):
 *       For processes whose mm->total_vm is below the threshold, we
 *       skip async interception. The kprobe + kmalloc + kthread-wakeup
 *       overhead exceeds the cost of just running exit_mmap inline for
 *       small workloads. Configurable via sysfs.
 *
 *   O2. Batched kthread processing (batch_size, default 8):
 *       Instead of cond_resched()-ing after every item, the kthread
 *       processes up to batch_size items in a tight loop, then yields
 *       once. Reduces context-switch overhead under burst load.
 *
 *   O3. Hot-path bypass (HOT_PATH_BYPASS_PAGES = 32, hardcoded):
 *       Ultra-fast bypass for trivial mms. Checked BEFORE the atomic
 *       operation on mm_users, so the bypass is essentially a single
 *       READ_ONCE + compare. Saves the atomic_read overhead for short-
 *       lived processes (e.g., shell builtins, /bin/true).
 *
 * SYSFS
 * -----
 *   /sys/kernel/time/target_pid          — pid filter; -1 = all
 *   /sys/kernel/time/enabled             — master toggle
 *   /sys/kernel/time/oom_threshold_kb    — OOM safety threshold
 *   /sys/kernel/time/min_vm_pages        — adaptive deferral threshold
 *   /sys/kernel/time/batch_size          — kthread batch size
 *   /sys/kernel/time/stats               — read-only counters
 *
 * Kernel 6.1 / Ubuntu x86_64.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/kprobes.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/timekeeping.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/atomic.h>
#include <linux/wait.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/swap.h>
#include <asm/cacheflush.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("priyanshu");
MODULE_DESCRIPTION("async mm destruction kthread + exec time tracker");

/* =======================================================
 * Compile-time constants for hot-path bypass (O3).
 * Hardcoded for speed — checked before any atomic ops.
 *   32 pages = 128 KB, suitable for short-lived shell tools
 * ======================================================= */
#define HOT_PATH_BYPASS_PAGES   32

/* =======================================================
 * Tunables (writable via sysfs)
 * ======================================================= */
static int            enabled          = 1;            /* S1 */
static unsigned long  oom_threshold_kb = 32 * 1024;    /* S2 — 32 MB */
static unsigned long  min_vm_pages     = 128;          /* O1 — 512 KB */
static unsigned int   batch_size       = 8;            /* O2 */

/* =======================================================
 * Stats counters
 * ======================================================= */
static atomic_long_t stat_intercepted     = ATOMIC_LONG_INIT(0);
static atomic_long_t stat_completed       = ATOMIC_LONG_INIT(0);
static atomic_long_t stat_oom_fallback    = ATOMIC_LONG_INIT(0);
static atomic_long_t stat_disabled_pass   = ATOMIC_LONG_INIT(0);
static atomic_long_t stat_hotpath_bypass  = ATOMIC_LONG_INIT(0);  /* O3 */
static atomic_long_t stat_under_threshold = ATOMIC_LONG_INIT(0);  /* O1 */
static atomic_long_t stat_batches         = ATOMIC_LONG_INIT(0);  /* O2 */

/* =======================================================
 * Symbol resolution via kallsyms_lookup_name (kprobe trick).
 * ======================================================= */
typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
static kallsyms_lookup_name_t my_kallsyms_lookup_name;

static int resolve_kallsyms(void)
{
    struct kprobe kp = { .symbol_name = "kallsyms_lookup_name" };
    int ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("async_mm: cannot find kallsyms_lookup_name ret=%d\n", ret);
        return ret;
    }
    my_kallsyms_lookup_name = (kallsyms_lookup_name_t)kp.addr;
    unregister_kprobe(&kp);
    pr_info("async_mm: kallsyms_lookup_name at %px\n", my_kallsyms_lookup_name);
    return 0;
}

typedef void (*exit_mmap_t)(struct mm_struct *mm);
typedef void (*ksm_exit_t)(struct mm_struct *mm);
typedef void (*khugepaged_exit_t)(struct mm_struct *mm);
typedef void (*exit_aio_t)(struct mm_struct *mm);

static exit_mmap_t       fn_exit_mmap;
static ksm_exit_t        fn_ksm_exit;
static khugepaged_exit_t fn_khugepaged_exit;
static exit_aio_t        fn_exit_aio;

static int resolve_symbols(void)
{
    fn_exit_mmap       = (exit_mmap_t)        my_kallsyms_lookup_name("exit_mmap");
    fn_ksm_exit        = (ksm_exit_t)         my_kallsyms_lookup_name("ksm_exit");
    fn_khugepaged_exit = (khugepaged_exit_t)  my_kallsyms_lookup_name("__khugepaged_exit");
    fn_exit_aio        = (exit_aio_t)         my_kallsyms_lookup_name("exit_aio");

    pr_info("async_mm: exit_mmap        = %px\n", fn_exit_mmap);
    pr_info("async_mm: ksm_exit         = %px %s\n",
            fn_ksm_exit, fn_ksm_exit ? "" : "(absent — KSM not configured)");
    pr_info("async_mm: __khugepaged_exit = %px %s\n",
            fn_khugepaged_exit, fn_khugepaged_exit ? "" : "(absent — THP not configured)");
    pr_info("async_mm: exit_aio         = %px %s\n",
            fn_exit_aio, fn_exit_aio ? "" : "(absent — AIO not configured)");

    if (!fn_exit_mmap) {
        pr_err("async_mm: exit_mmap not found — required\n");
        return -ENOENT;
    }
    return 0;
}

/* =======================================================
 * sysfs interface — show/store handlers
 * ======================================================= */
static pid_t target_pid = -1;
static struct kobject *my_kobj;

#define SHOW_INT(name, fmt, var) \
    static ssize_t name##_show(struct kobject *kobj, struct kobj_attribute *a, \
                                char *buf) \
    { return sprintf(buf, fmt "\n", var); }

#define STORE_INT(name, type, fmt, var, log) \
    static ssize_t name##_store(struct kobject *kobj, struct kobj_attribute *a, \
                                 const char *buf, size_t count) \
    { \
        type tmp; \
        if (sscanf(buf, fmt, &tmp) != 1) return -EINVAL; \
        var = tmp; \
        log; \
        return count; \
    }

SHOW_INT(pid, "%d", target_pid)
STORE_INT(pid, pid_t, "%d", target_pid,
          pr_info("async_mm: target_pid = %d\n", target_pid))

SHOW_INT(enabled, "%d", enabled)
STORE_INT(enabled, int, "%d", enabled,
          { enabled = !!enabled;
            pr_info("async_mm: enabled = %d\n", enabled); })

SHOW_INT(oom_threshold, "%lu", oom_threshold_kb)
STORE_INT(oom_threshold, unsigned long, "%lu", oom_threshold_kb,
          pr_info("async_mm: oom_threshold_kb = %lu\n", oom_threshold_kb))

SHOW_INT(min_vm_pages, "%lu", min_vm_pages)
STORE_INT(min_vm_pages, unsigned long, "%lu", min_vm_pages,
          pr_info("async_mm: min_vm_pages = %lu\n", min_vm_pages))

SHOW_INT(batch_size, "%u", batch_size)
STORE_INT(batch_size, unsigned int, "%u", batch_size,
          { if (batch_size < 1) batch_size = 1;
            pr_info("async_mm: batch_size = %u\n", batch_size); })

static ssize_t stats_show(struct kobject *kobj, struct kobj_attribute *attr,
                           char *buf)
{
    return sprintf(buf,
        "intercepted     : %ld\n"
        "completed       : %ld\n"
        "oom_fallback    : %ld\n"
        "disabled_pass   : %ld\n"
        "hotpath_bypass  : %ld\n"
        "under_threshold : %ld\n"
        "batches         : %ld\n",
        atomic_long_read(&stat_intercepted),
        atomic_long_read(&stat_completed),
        atomic_long_read(&stat_oom_fallback),
        atomic_long_read(&stat_disabled_pass),
        atomic_long_read(&stat_hotpath_bypass),
        atomic_long_read(&stat_under_threshold),
        atomic_long_read(&stat_batches));
}

static struct kobj_attribute pid_attr           =
    __ATTR(target_pid,       0664, pid_show,           pid_store);
static struct kobj_attribute enabled_attr       =
    __ATTR(enabled,          0664, enabled_show,       enabled_store);
static struct kobj_attribute oom_threshold_attr =
    __ATTR(oom_threshold_kb, 0664, oom_threshold_show, oom_threshold_store);
static struct kobj_attribute min_vm_pages_attr  =
    __ATTR(min_vm_pages,     0664, min_vm_pages_show,  min_vm_pages_store);
static struct kobj_attribute batch_size_attr    =
    __ATTR(batch_size,       0664, batch_size_show,    batch_size_store);
static struct kobj_attribute stats_attr         =
    __ATTR(stats,            0444, stats_show,         NULL);

static struct attribute *time_attrs[] = {
    &pid_attr.attr,
    &enabled_attr.attr,
    &oom_threshold_attr.attr,
    &min_vm_pages_attr.attr,
    &batch_size_attr.attr,
    &stats_attr.attr,
    NULL,
};

static struct attribute_group time_attr_group = {
    .attrs = time_attrs,
};

/* =======================================================
 * S2 — OOM check
 * ======================================================= */
static bool low_memory(void)
{
    struct sysinfo i;
    si_meminfo(&i);
    return ((i.freeram * i.mem_unit) >> 10) < oom_threshold_kb;
}

/* =======================================================
 * Async work queue
 * ======================================================= */
struct mm_work {
    struct list_head  node;
    struct mm_struct *mm;
    pid_t             pid;
    unsigned long     vm_pages;       /* logged for analytics */
    char              comm[TASK_COMM_LEN];
    u64               enqueue_ns;
};

static LIST_HEAD(mm_work_list);
static DEFINE_SPINLOCK(mm_work_lock);
static DECLARE_WAIT_QUEUE_HEAD(mm_work_wq);
static struct task_struct *mm_kthread;

/* =======================================================
 * Kthread worker
 *
 * O2 — batched processing:
 *   process up to batch_size items per drain cycle, then cond_resched
 *   once. Reduces scheduler overhead under burst exits.
 * ======================================================= */
static int mm_destroy_worker(void *unused)
{
    struct mm_work *work;
    u64 start_ns, elapsed_ns;
    unsigned int processed_in_batch;

    while (!kthread_should_stop()) {
        wait_event_interruptible(mm_work_wq,
            !list_empty(&mm_work_list) || kthread_should_stop());

        processed_in_batch = 0;

        while (true) {
            spin_lock(&mm_work_lock);
            if (list_empty(&mm_work_list)) {
                spin_unlock(&mm_work_lock);
                break;
            }
            work = list_first_entry(&mm_work_list, struct mm_work, node);
            list_del(&work->node);
            spin_unlock(&mm_work_lock);

            pr_info("[mm_destroy] kthread: pid=%d comm=%s vm=%lu pages mm=%px — starting\n",
                    work->pid, work->comm, work->vm_pages, work->mm);

            start_ns = ktime_get_ns();

            /* Full __mmput() cleanup sequence (S3) */
            fn_exit_mmap(work->mm);
            if (fn_ksm_exit)        fn_ksm_exit(work->mm);
            if (fn_khugepaged_exit) fn_khugepaged_exit(work->mm);
            if (fn_exit_aio)        fn_exit_aio(work->mm);

            elapsed_ns = ktime_get_ns() - start_ns;

            pr_info("[mm_destroy] kthread: pid=%d comm=%s mm=%px — done in %llu ns (%llu us)\n",
                    work->pid, work->comm, work->mm,
                    elapsed_ns, elapsed_ns / 1000);
            pr_info("[mm_destroy] total latency (mmput→done): %llu us\n",
                    (ktime_get_ns() - work->enqueue_ns) / 1000);

            atomic_dec(&work->mm->mm_users);
            mmdrop(work->mm);

            atomic_long_inc(&stat_completed);
            kfree(work);

            /* O2 — batch boundary: yield once per batch, not per item */
            processed_in_batch++;
            if (processed_in_batch >= batch_size) {
                atomic_long_inc(&stat_batches);
                cond_resched();
                processed_in_batch = 0;
            }
        }

        if (processed_in_batch > 0) {
            atomic_long_inc(&stat_batches);
            cond_resched();
        }
    }

    return 0;
}

/* =======================================================
 * mmput kprobe — pre_handler
 *
 * Order of checks (cheapest first):
 *   1. NULL mm pointer
 *   2. S1: enabled toggle
 *   3. O3: hot-path bypass (READ_ONCE + compare, no atomic ops)
 *   4. final-put check (atomic_read on mm_users)
 *   5. PID filter
 *   6. O1: adaptive threshold (compare against min_vm_pages)
 *   7. S2: OOM safety check
 *   8. kmalloc + queue
 * ======================================================= */
static int mmput_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
    struct mm_struct *mm = (struct mm_struct *)regs->di;
    struct mm_work   *work;
    pid_t             cur_pid;
    char              cur_comm[TASK_COMM_LEN];
    unsigned long     vm_pages;

    if (!mm)
        return 0;

    /* S1 — master enable toggle */
    if (!enabled) {
        atomic_long_inc(&stat_disabled_pass);
        return 0;
    }

    /* O3 — HOT-PATH BYPASS
     * Single READ_ONCE + compare, no atomic operations.
     * Skips ALL further work for tiny mms (shell builtins, /bin/true). */
    vm_pages = READ_ONCE(mm->total_vm);
    if (vm_pages < HOT_PATH_BYPASS_PAGES) {
        atomic_long_inc(&stat_hotpath_bypass);
        return 0;
    }

    /* only intercept the final put */
    if (atomic_read(&mm->mm_users) != 1)
        return 0;

    /* PID filter */
    cur_pid = current->pid;
    if (target_pid != -1 &&
        current->pid  != target_pid &&
        current->tgid != target_pid)
        return 0;

    /* O1 — ADAPTIVE THRESHOLD
     * For mms below the configurable threshold, deferral overhead
     * (kmalloc + kthread wakeup) exceeds the cost of inline exit_mmap.
     * Skip interception → sync teardown is faster. */
    if (vm_pages < min_vm_pages) {
        atomic_long_inc(&stat_under_threshold);
        return 0;
    }

    /* S2 — OOM safety check */
    if (low_memory()) {
        atomic_long_inc(&stat_oom_fallback);
        pr_warn("async_mm: low memory — sync fallback for pid=%d\n", cur_pid);
        return 0;
    }

    get_task_comm(cur_comm, current);

    work = kmalloc(sizeof(*work), GFP_ATOMIC);
    if (!work) {
        pr_warn("async_mm: kmalloc failed for pid=%d — sync fallback\n", cur_pid);
        return 0;
    }

    /* The interception trick: bump mm_users 1 → 2.
     * mmput's atomic_dec_and_test gives 1, not 0 → __mmput is skipped. */
    mmgrab(mm);
    atomic_inc(&mm->mm_users);

    work->mm         = mm;
    work->pid        = cur_pid;
    work->vm_pages   = vm_pages;
    work->enqueue_ns = ktime_get_ns();
    memcpy(work->comm, cur_comm, TASK_COMM_LEN);

    spin_lock(&mm_work_lock);
    list_add_tail(&work->node, &mm_work_list);
    spin_unlock(&mm_work_lock);

    wake_up(&mm_work_wq);

    pr_info("[mm_destroy] intercepted mmput pid=%d comm=%s vm=%lu pages mm=%px — queued\n",
            cur_pid, cur_comm, vm_pages, mm);

    atomic_long_inc(&stat_intercepted);
    return 0;
}

static struct kprobe mmput_kp = {
    .symbol_name = "mmput",
    .pre_handler = mmput_pre_handler,
};

/* =======================================================
 * exec time tracker — kretprobe on do_execveat_common
 * ======================================================= */
struct exec_probe_data {
    u64   start_ns;
    pid_t pid;
    char  comm[TASK_COMM_LEN];
};

static int execve_entry(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct exec_probe_data *d = (struct exec_probe_data *)ri->data;

    if (target_pid != -1 &&
        current->pid  != target_pid &&
        current->tgid != target_pid) {
        d->start_ns = 0;
        return 0;
    }

    d->start_ns = ktime_get_ns();
    d->pid      = current->pid;
    get_task_comm(d->comm, current);

    pr_info("[exec] ENTRY  pid=%d comm=%s  start=%llu ns\n",
            d->pid, d->comm, d->start_ns);
    return 0;
}

static int execve_ret(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    struct exec_probe_data *d = (struct exec_probe_data *)ri->data;
    u64 elapsed;

    if (d->start_ns == 0)
        return 0;

    elapsed = ktime_get_ns() - d->start_ns;
    pr_info("[exec] RETURN pid=%d comm=%s  elapsed=%llu ns  (%llu us)\n",
            d->pid, d->comm, elapsed, elapsed / 1000);
    return 0;
}

static struct kretprobe krp_execve = {
    .kp.symbol_name = "do_execveat_common.isra.0",
    .entry_handler  = execve_entry,
    .handler        = execve_ret,
    .data_size      = sizeof(struct exec_probe_data),
    .maxactive      = 20,
};

/* =======================================================
 * module init / exit
 * ======================================================= */
static int __init async_mm_init(void)
{
    int ret;

    ret = resolve_kallsyms();
    if (ret) return ret;

    ret = resolve_symbols();
    if (ret) return ret;

    my_kobj = kobject_create_and_add("time", kernel_kobj);
    if (!my_kobj) return -ENOMEM;

    ret = sysfs_create_group(my_kobj, &time_attr_group);
    if (ret) {
        kobject_put(my_kobj);
        return ret;
    }

    mm_kthread = kthread_run(mm_destroy_worker, NULL, "mm_destroyer");
    if (IS_ERR(mm_kthread)) {
        ret = PTR_ERR(mm_kthread);
        sysfs_remove_group(my_kobj, &time_attr_group);
        kobject_put(my_kobj);
        return ret;
    }
    pr_info("async_mm: mm_destroyer kthread started\n");

    ret = register_kprobe(&mmput_kp);
    if (ret) {
        kthread_stop(mm_kthread);
        sysfs_remove_group(my_kobj, &time_attr_group);
        kobject_put(my_kobj);
        return ret;
    }
    pr_info("async_mm: kprobe on mmput registered\n");

    ret = register_kretprobe(&krp_execve);
    if (ret < 0) {
        unregister_kprobe(&mmput_kp);
        kthread_stop(mm_kthread);
        sysfs_remove_group(my_kobj, &time_attr_group);
        kobject_put(my_kobj);
        return ret;
    }
    pr_info("async_mm: kretprobe on do_execveat_common registered\n");

    pr_info("async_mm: loaded — controls in /sys/kernel/time/\n");
    pr_info("async_mm:   target_pid=%d enabled=%d\n", target_pid, enabled);
    pr_info("async_mm:   oom_threshold_kb=%lu min_vm_pages=%lu batch_size=%u\n",
            oom_threshold_kb, min_vm_pages, batch_size);
    pr_info("async_mm:   hot_path_bypass=%d pages (compile-time)\n",
            HOT_PATH_BYPASS_PAGES);
    return 0;
}

static void __exit async_mm_exit(void)
{
    struct mm_work *work, *tmp;

    unregister_kprobe(&mmput_kp);
    unregister_kretprobe(&krp_execve);

    wake_up(&mm_work_wq);
    kthread_stop(mm_kthread);

    spin_lock(&mm_work_lock);
    list_for_each_entry_safe(work, tmp, &mm_work_list, node) {
        list_del(&work->node);
        spin_unlock(&mm_work_lock);

        pr_warn("async_mm: cleanup: sync exit_mmap for pid=%d\n", work->pid);
        fn_exit_mmap(work->mm);
        if (fn_ksm_exit)        fn_ksm_exit(work->mm);
        if (fn_khugepaged_exit) fn_khugepaged_exit(work->mm);
        if (fn_exit_aio)        fn_exit_aio(work->mm);
        atomic_dec(&work->mm->mm_users);
        mmdrop(work->mm);
        kfree(work);

        spin_lock(&mm_work_lock);
    }
    spin_unlock(&mm_work_lock);

    sysfs_remove_group(my_kobj, &time_attr_group);
    kobject_put(my_kobj);

    pr_info("async_mm: final stats — intercepted=%ld completed=%ld\n",
            atomic_long_read(&stat_intercepted),
            atomic_long_read(&stat_completed));
    pr_info("async_mm:   bypassed: hotpath=%ld under_threshold=%ld oom=%ld disabled=%ld\n",
            atomic_long_read(&stat_hotpath_bypass),
            atomic_long_read(&stat_under_threshold),
            atomic_long_read(&stat_oom_fallback),
            atomic_long_read(&stat_disabled_pass));
    pr_info("async_mm:   batches=%ld (avg %ld items/batch)\n",
            atomic_long_read(&stat_batches),
            atomic_long_read(&stat_batches) > 0 ?
                atomic_long_read(&stat_completed) / atomic_long_read(&stat_batches) : 0);
    pr_info("async_mm: unloaded\n");
}

module_init(async_mm_init);
module_exit(async_mm_exit);
