#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/ktime.h>
#include <linux/sysfs.h>
#include <linux/sched.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include "exec_hook.h"

static DEFINE_SPINLOCK(my_lock);

#define MAX_DEPTH 100
#define LOG_BUF_SIZE (10*1024*1024)

/* ---------------- Tracked PID ---------------- */
extern pid_t tracked_pid;

/* ---------------- Optimization flag ---------------- */
extern int optimization_on;

/* ---------------- Global Log Buffer ---------------- */
static char log_buffer[LOG_BUF_SIZE];
static int log_offset = 0;

/* ---------------- Stack ---------------- */
struct func_timer {
    const char *fn_name;
    ktime_t start;
    s64 child_time;
};

static struct func_timer stack[MAX_DEPTH];
static int top = -1;

/* ---------------- Indentation Helper ---------------- */
static void make_indent(int depth, char *buf, size_t buf_size)
{
    int i, len = 0;
    for (i = 0; i < depth && len + 2 < buf_size; i++) {
        buf[len++] = ' ';
        buf[len++] = ' ';
    }
    buf[len] = '\0';
}

/* ---------------- Trace Functions ---------------- */
static void my_enter(const char *func)
{
    unsigned long flags;
    char indent[2*MAX_DEPTH + 1];
    int len;

    if (current->tgid != tracked_pid)
        return;

    spin_lock_irqsave(&my_lock, flags);

    if (top + 1 >= MAX_DEPTH) {
        printk(KERN_ERR "exec trace: stack overflow!\n");
        spin_unlock_irqrestore(&my_lock, flags);
        return;
    }

    top++;
    stack[top].fn_name = func;
    stack[top].start = ktime_get();
    stack[top].child_time = 0;

    make_indent(top, indent, sizeof(indent));

    if (log_offset < LOG_BUF_SIZE - 256) {
        len = sprintf(log_buffer + log_offset,
                      "[pid %d]%s--->%s() {\n",
                      current->pid, indent, func);
        log_offset += len;
    }

    spin_unlock_irqrestore(&my_lock, flags);
}

static void my_exit(const char *func)
{
    unsigned long flags;
    char indent[2*MAX_DEPTH + 1];
    ktime_t end;
    s64 inclusive, exclusive;
    int len;

    if (current->tgid != tracked_pid)
        return;

    spin_lock_irqsave(&my_lock, flags);

    if (top < 0) {
        printk(KERN_ERR "exec trace: stack underflow!\n");
        spin_unlock_irqrestore(&my_lock, flags);
        return;
    }

    if (memcmp(stack[top].fn_name, func, strlen(func))) {
        printk(KERN_ERR "exec trace: stack corrupted! exit [%s] expected [%s]\n",
               func, stack[top].fn_name);
        spin_unlock_irqrestore(&my_lock, flags);
        return;
    }

    end = ktime_get();
    inclusive = ktime_to_ns(ktime_sub(end, stack[top].start));
    exclusive = inclusive - stack[top].child_time;

    make_indent(top, indent, sizeof(indent));

    if (log_offset < LOG_BUF_SIZE - 256) {
        len = sprintf(log_buffer + log_offset,
                      "[pid %d]%s<---%s() inc=%6lld ns, exc=%6lld ns }\n",
                      current->pid, indent, stack[top].fn_name,
                      inclusive, exclusive);
        log_offset += len;
    }

    top--;

    if (top >= 0)
        stack[top].child_time += inclusive;

    spin_unlock_irqrestore(&my_lock, flags);
}

/* ---------------- Sysfs: tracked_pid ---------------- */
static ssize_t read_pid(struct kobject *kobj,
                        struct kobj_attribute *attr,
                        char *buf)
{
    return sprintf(buf, "%d\n", tracked_pid);
}

static ssize_t set_pid(struct kobject *kobj,
                       struct kobj_attribute *attr,
                       const char *buf, size_t count)
{
    int newval;

    if (kstrtoint(buf, 10, &newval) || newval < 0)
        return -EINVAL;

    tracked_pid = newval;
    top = -1;
    log_offset = 0;

    printk(KERN_INFO "exec trace: tracking PID = %d\n", tracked_pid);
    return count;
}

static struct kobj_attribute exechook_pid_attribute =
    __ATTR(tracked_pid, 0644, read_pid, set_pid);

/* ---------------- Sysfs: optimization_on ---------------- */
static ssize_t opt_show(struct kobject *kobj,
                         struct kobj_attribute *attr,
                         char *buf)
{
    return sprintf(buf, "%d\n", optimization_on);
}

static ssize_t opt_store(struct kobject *kobj,
                          struct kobj_attribute *attr,
                          const char *buf,
                          size_t count)
{
    int val;

    if (kstrtoint(buf, 10, &val) || (val != 0 && val != 1))
    return -EINVAL;

    optimization_on = val;
    return count;
}

static struct kobj_attribute exechook_opt_attribute =
    __ATTR(optimization_on, 0644, opt_show, opt_store);

/* ---------------- /proc log ---------------- */
static int log_show(struct seq_file *m, void *v)
{
    seq_write(m, log_buffer, log_offset);
    return 0;
}

static int log_open(struct inode *inode, struct file *file)
{
    return single_open(file, log_show, NULL);
}

static const struct proc_ops log_proc_fops = {
    .proc_open    = log_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

/* ---------------- Sysfs group ---------------- */
static struct attribute *exechook_attrs[] = {
    &exechook_pid_attribute.attr,
    &exechook_opt_attribute.attr,
    NULL,
};

static struct attribute_group exechook_attr_group = {
    .attrs = exechook_attrs,
    .name = "cs614hook",
};

/* ---------------- Init / Exit ---------------- */
static int __init exechook_init(void)
{
    int ret;

    ret = sysfs_create_group(kernel_kobj, &exechook_attr_group);
    if (ret) {
        printk(KERN_ERR "exec trace: sysfs create failed %d\n", ret);
        return ret;
    }

    proc_create("cs614hook_log", 0444, NULL, &log_proc_fops);

    exec_trace_enter_hook = my_enter;
    exec_trace_exit_hook  = my_exit;

    printk(KERN_INFO "exec trace: module loaded\n");
    return 0;
}

static void __exit exechook_exit(void)
{
    exec_trace_enter_hook = NULL;
    exec_trace_exit_hook  = NULL;

    /* stack dump */
    if (top >= 0) {
        printk(KERN_INFO "stack dump start\n");
        while (top >= 0) {
            struct func_timer *fnt = &stack[top];
            printk(KERN_INFO "fn=%s start=%lld child=%lld\n",
                   fnt->fn_name, fnt->start, fnt->child_time);
            top--;
        }
        printk(KERN_INFO "stack dump end\n");
    }

    sysfs_remove_group(kernel_kobj, &exechook_attr_group);
    remove_proc_entry("cs614hook_log", NULL);

    printk(KERN_INFO "exec trace: module unloaded\n");
}

module_init(exechook_init);
module_exit(exechook_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("harshkumar24@cse.iitk.ac.in");
MODULE_DESCRIPTION("Exec timing module with sysfs controls and call graph tracing");