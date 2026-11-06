// SPDX-License-Identifier: GPL-2.0
/*
 * Educational LSM for mmap policy.
 * Denies writable shared mappings of root-owned regular files.
 */
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/lsm_hooks.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/uidgid.h>

static bool protected_root_files = true;
module_param(protected_root_files, bool, 0644);
MODULE_PARM_DESC(protected_root_files,
                 "protect regular files owned by uid 0 (default: true)");

static int mmap_guard_mmap_file(struct file *file,
                                unsigned long reqprot,
                                unsigned long prot,
                                unsigned long flags)
{
    const struct inode *inode;
    bool writable_shared;

    (void)reqprot;

    if (!protected_root_files || !file)
        return 0;

    inode = file_inode(file);
    writable_shared = (prot & PROT_WRITE) && (flags & MAP_SHARED);
    if (!S_ISREG(inode->i_mode) || !writable_shared ||
        !uid_eq(inode->i_uid, GLOBAL_ROOT_UID))
        return 0;

    pr_warn_ratelimited("mmap_guard: denied writable shared mmap pid=%d comm=%s ino=%lu prot=0x%lx flags=0x%lx\n",
                        current->pid, current->comm, inode->i_ino,
                        prot, flags);
    return -EPERM;
}

static struct security_hook_list mmap_guard_hooks[] __ro_after_init = {
    LSM_HOOK_INIT(mmap_file, mmap_guard_mmap_file),
};

static int __init mmap_guard_init(void)
{
    security_add_hooks(mmap_guard_hooks, ARRAY_SIZE(mmap_guard_hooks),
                       "mmap_guard");
    pr_info("mmap_guard: loaded (root-owned regular files protected=%d)\n",
            protected_root_files);
    return 0;
}

static void __exit mmap_guard_exit(void)
{
    pr_info("mmap_guard: unloaded\n");
}

module_init(mmap_guard_init);
module_exit(mmap_guard_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("SO2026B");
MODULE_DESCRIPTION("Educational LSM policy for writable shared file mappings");
