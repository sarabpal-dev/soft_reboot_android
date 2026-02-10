// SPDX-License-Identifier: GPL-2.0-only
/*
 * Soft Reboot - Userspace Reboot LKM for Android
 * 
 * Kills all userspace processes (except init) to trigger a clean
 * userspace reboot. Used for KernelSU/Magisk/LSPosed module injection.
 * 
 * Usage: echo 1 > /dev/soft_reboot
 * 
 * Based on kernel's sysrq send_sig_all() - guaranteed kernel-safe.
 */

#define pr_fmt(fmt) "soft_reboot: " fmt

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/signal.h>
#include <linux/pid.h>

/*
 * Kill zygote processes to trigger userspace reboot.
 * 
 * Killing zygote causes init to restart it, which restarts:
 * - system_server
 * - All Android apps
 * - Framework services
 * 
 * This is the safe way to trigger "soft reboot" without kernel panic.
 * Init's watchdog won't trigger because init itself stays alive.
 */
static void soft_reboot_kill_zygote(void)
{
    struct task_struct *p;
    int killed = 0;

    pr_info("Soft reboot initiated - killing zygote processes\n");

    read_lock(&tasklist_lock);
    for_each_process(p) {
        /* Skip kernel threads */
        if (p->flags & PF_KTHREAD)
            continue;
        
        /* Only kill zygote and zygote64 */
        if (strncmp(p->comm, "zygote", 6) == 0 ||
            strncmp(p->comm, "main", 4) == 0) {  /* zygote's initial name is 'main' */
            pr_info("Killing %s (pid %d)\n", p->comm, p->pid);
            send_sig(SIGKILL, p, 1);
            killed++;
        }
    }
    read_unlock(&tasklist_lock);

    pr_info("Sent SIGKILL to %d zygote processes\n", killed);
    pr_info("Init will respawn zygote - soft reboot in progress\n");
}

/* ========================================================================
 * /dev/soft_reboot Character Device
 * ======================================================================== */

static ssize_t soft_reboot_write(struct file *file, const char __user *buf,
                              size_t count, loff_t *ppos)
{
    char kbuf[8];
    int val;

    if (count > sizeof(kbuf) - 1)
        count = sizeof(kbuf) - 1;

    if (copy_from_user(kbuf, buf, count))
        return -EFAULT;

    kbuf[count] = '\0';

    /* Remove trailing newline */
    if (count > 0 && kbuf[count-1] == '\n')
        kbuf[count-1] = '\0';

    if (kstrtoint(kbuf, 10, &val))
        return -EINVAL;

    if (val == 1) {
        soft_reboot_kill_zygote();
    } else {
        pr_info("Write 1 to trigger userspace reboot\n");
        return -EINVAL;
    }

    return count;
}

static ssize_t soft_reboot_read(struct file *file, char __user *buf,
                             size_t count, loff_t *ppos)
{
    static const char help[] = "Write 1 to trigger userspace reboot\n";
    size_t len = sizeof(help) - 1;

    if (*ppos >= len)
        return 0;

    if (count > len - *ppos)
        count = len - *ppos;

    if (copy_to_user(buf, help + *ppos, count))
        return -EFAULT;

    *ppos += count;
    return count;
}

static const struct file_operations soft_reboot_fops = {
    .owner = THIS_MODULE,
    .write = soft_reboot_write,
    .read = soft_reboot_read,
};

static struct miscdevice soft_reboot_dev = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "soft_reboot",
    .fops = &soft_reboot_fops,
    .mode = 0660,  /* root:root rw-rw---- */
};

/* ========================================================================
 * Module Init/Exit
 * ======================================================================== */

static int __init soft_reboot_init(void)
{
    int rc;

    rc = misc_register(&soft_reboot_dev);
    if (rc) {
        pr_err("Failed to register device: %d\n", rc);
        return rc;
    }

    pr_info("Soft Reboot ready - echo 1 > /dev/soft_reboot\n");
    return 0;
}

static void __exit soft_reboot_exit(void)
{
    misc_deregister(&soft_reboot_dev);
    pr_info("Soft Reboot unloaded\n");
}

module_init(soft_reboot_init);
module_exit(soft_reboot_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("sarabpal.maan@gmail.com");
MODULE_DESCRIPTION("Soft Reboot - Userspace reboot for Android");
MODULE_VERSION("1.0");
