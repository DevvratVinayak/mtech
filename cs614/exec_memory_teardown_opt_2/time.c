#include <linux/module.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/timekeeping.h>
#include <linux/sysfs.h>

static pid_t target_pid = -1;
static struct kobject *my_kobj;

/*
 * We store the mm pointer of the target process so we can identify
 * the deferred exit_mmap call running in the kworker context.
 */
static struct mm_struct *target_old_mm = NULL;

struct probe_data {
    u64 start_ns;
    pid_t pid;
    char comm[TASK_COMM_LEN];
};

static ssize_t pid_show(struct kobject *kobj,
                        struct kobj_attribute *attr,
                        char *buf)
{
    return sprintf(buf, "%d\n", target_pid);
}

static ssize_t pid_store(struct kobject *kobj,
                         struct kobj_attribute *attr,
                         const char *buf,
                         size_t count)
{
    sscanf(buf, "%d", &target_pid);
    target_old_mm = NULL; /* reset on new PID */
    printk(KERN_INFO "target PID set to %d\n", target_pid);
    return count;
}

static struct kobj_attribute pid_attr =
    __ATTR(target_pid, 0664, pid_show, pid_store);

/* ================= EXECVE PROBE ================= */

static int execve_entry(struct kretprobe_instance *ri,
                        struct pt_regs *regs)
{
    struct probe_data *d = (struct probe_data *)ri->data;

    if (current->pid != target_pid && current->tgid != target_pid) {
        d->start_ns = 0;
        return 0;
    }

    d->start_ns = ktime_get_ns();
    d->pid = current->pid;
    get_task_comm(d->comm, current);

    /*
     * Capture the current mm BEFORE exec replaces it.
     * This will be compared against exit_mmap's argument to identify
     * the deferred teardown.
     */
    target_old_mm = current->mm;

    printk(KERN_INFO "[exec_probe] do_execveat_common ENTRY "
           "pid=%d comm=%s start=%llu ns old_mm=%px\n",
           d->pid, d->comm, d->start_ns, target_old_mm);

    return 0;
}

static int execve_ret(struct kretprobe_instance *ri,
                      struct pt_regs *regs)
{
    struct probe_data *d = (struct probe_data *)ri->data;
    u64 end_ns;
    u64 elapsed;

    if (d->start_ns == 0)
        return 0;

    end_ns = ktime_get_ns();
    elapsed = end_ns - d->start_ns;

    printk(KERN_INFO "[exec_probe] do_execveat_common RETURN "
           "pid=%d comm=%s "
           "start=%llu ns "
           "end=%llu ns "
           "elapsed=%llu ns (%llu us)\n",
           d->pid, d->comm,
           d->start_ns,
           end_ns,
           elapsed,
           elapsed / 1000);

    return 0;
}

static struct kretprobe krp_execve = {
    .kp.symbol_name = "do_execveat_common.isra.0",
    .entry_handler  = execve_entry,
    .handler        = execve_ret,
    .data_size      = sizeof(struct probe_data),
    .maxactive      = 20,
};

/* ================= EXEC_DEFERRED_MMPUT PROBE ================= */
/*
 * This replaces the old mmput probe. Our kernel mod calls
 * exec_deferred_mmput() instead of mmput(). This probe measures
 * the SYNCHRONOUS cost seen by execve: Phase 1 cleanup + queue_work.
 * With async ON, this should be negligible (<100us).
 * With async OFF, this includes the full __mmput (expensive).
 */

static int deferred_mmput_entry(struct kretprobe_instance *ri,
                                struct pt_regs *regs)
{
    struct probe_data *d = (struct probe_data *)ri->data;

    if (current->pid != target_pid && current->tgid != target_pid) {
        d->start_ns = 0;
        return 0;
    }

    d->start_ns = ktime_get_ns();
    d->pid = current->pid;
    get_task_comm(d->comm, current);

    printk(KERN_INFO "[exec_probe] exec_deferred_mmput ENTRY "
           "pid=%d comm=%s start=%llu ns\n",
           d->pid, d->comm, d->start_ns);

    return 0;
}

static int deferred_mmput_ret(struct kretprobe_instance *ri,
                              struct pt_regs *regs)
{
    struct probe_data *d = (struct probe_data *)ri->data;
    u64 end_ns;
    u64 elapsed;

    if (d->start_ns == 0)
        return 0;

    end_ns = ktime_get_ns();
    elapsed = end_ns - d->start_ns;

    printk(KERN_INFO "[exec_probe] exec_deferred_mmput RETURN "
           "pid=%d comm=%s "
           "start=%llu ns "
           "end=%llu ns "
           "elapsed=%llu ns (%llu us) [sync cost seen by execve]\n",
           d->pid, d->comm,
           d->start_ns,
           end_ns,
           elapsed,
           elapsed / 1000);

    return 0;
}

static struct kretprobe krp_deferred_mmput = {
    .kp.symbol_name = "exec_deferred_mmput",
    .entry_handler  = deferred_mmput_entry,
    .handler        = deferred_mmput_ret,
    .data_size      = sizeof(struct probe_data),
    .maxactive      = 20,
};

/* ================= EXIT_MMAP PROBE ================= */
/*
 * exit_mmap() does the actual page table teardown (unmap_vmas +
 * free_pgtables). With async teardown, this runs in a kworker thread.
 *
 * We can't match by PID (kworker has a different PID), so instead
 * we match by the mm_struct pointer captured during execve_entry.
 */

struct exit_mmap_data {
    u64 start_ns;
    struct mm_struct *mm;
    int matched;
};

static int exit_mmap_entry(struct kretprobe_instance *ri,
                           struct pt_regs *regs)
{
    struct exit_mmap_data *d = (struct exit_mmap_data *)ri->data;
    struct mm_struct *mm;

    /* First argument to exit_mmap(struct mm_struct *mm) */
    mm = (struct mm_struct *)regs->di;  /* x86_64 calling convention */

    if (target_old_mm != NULL && mm == target_old_mm) {
        d->start_ns = ktime_get_ns();
        d->mm = mm;
        d->matched = 1;

        printk(KERN_INFO "[exec_probe] exit_mmap ENTRY "
               "mm=%px worker=%s "
               "start=%llu ns (ASYNC TEARDOWN)\n",
               mm, current->comm, d->start_ns);
    } else {
        d->matched = 0;
    }

    return 0;
}

static int exit_mmap_ret(struct kretprobe_instance *ri,
                         struct pt_regs *regs)
{
    struct exit_mmap_data *d = (struct exit_mmap_data *)ri->data;
    u64 end_ns;
    u64 elapsed;

    if (!d->matched)
        return 0;

    end_ns = ktime_get_ns();
    elapsed = end_ns - d->start_ns;

    printk(KERN_INFO "[exec_probe] exit_mmap RETURN "
           "mm=%px worker=%s "
           "start=%llu ns "
           "end=%llu ns "
           "elapsed=%llu ns (%llu us) (ASYNC TEARDOWN COMPLETE)\n",
           d->mm, current->comm,
           d->start_ns,
           end_ns,
           elapsed,
           elapsed / 1000);

    /* Clear so we don't match again */
    target_old_mm = NULL;

    return 0;
}

static struct kretprobe krp_exit_mmap = {
    .kp.symbol_name = "exit_mmap",
    .entry_handler  = exit_mmap_entry,
    .handler        = exit_mmap_ret,
    .data_size      = sizeof(struct exit_mmap_data),
    .maxactive      = 20,
};

/* ================= INIT / EXIT ================= */

static int __init exec_probe_init(void)
{
    int ret;

    my_kobj = kobject_create_and_add("time", kernel_kobj);
    if (!my_kobj)
        return -ENOMEM;

    ret = sysfs_create_file(my_kobj, &pid_attr.attr);
    if (ret) {
        kobject_put(my_kobj);
        return ret;
    }

    ret = register_kretprobe(&krp_execve);
    if (ret < 0) {
        printk(KERN_INFO "[exec_probe] register krp_execve failed: %d\n", ret);
        return ret;
    }

    printk(KERN_INFO "[exec_probe] hooked do_execveat_common\n");

    ret = register_kretprobe(&krp_deferred_mmput);
    if (ret < 0) {
        printk(KERN_INFO "[exec_probe] register krp_deferred_mmput failed: %d\n", ret);
        unregister_kretprobe(&krp_execve);
        return ret;
    }

    printk(KERN_INFO "[exec_probe] hooked exec_deferred_mmput\n");

    ret = register_kretprobe(&krp_exit_mmap);
    if (ret < 0) {
        printk(KERN_INFO "[exec_probe] register krp_exit_mmap failed: %d\n", ret);
        unregister_kretprobe(&krp_execve);
        unregister_kretprobe(&krp_deferred_mmput);
        return ret;
    }

    printk(KERN_INFO "[exec_probe] hooked exit_mmap (async teardown tracker)\n");

    return 0;
}

static void __exit exec_probe_exit(void)
{
    unregister_kretprobe(&krp_execve);
    unregister_kretprobe(&krp_deferred_mmput);
    unregister_kretprobe(&krp_exit_mmap);

    sysfs_remove_file(my_kobj, &pid_attr.attr);
    kobject_put(my_kobj);

    printk(KERN_INFO "exec time tracker unloaded\n");
}

module_init(exec_probe_init);
module_exit(exec_probe_exit);

MODULE_LICENSE("GPL");