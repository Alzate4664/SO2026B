#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char LICENSE[] SEC("license") = "GPL";

#define PROT_WRITE 0x2
#define MAP_SHARED 0x01
#define EPERM 1

SEC("lsm/mmap_file")
int BPF_PROG(mmap_guard, struct file *file,
             unsigned long reqprot, unsigned long prot,
             unsigned long flags, int ret)
{
    struct inode *inode;
    unsigned int owner_uid;

    if (ret != 0 || !file)
        return ret;
    if (!(prot & PROT_WRITE) || !(flags & MAP_SHARED))
        return 0;

    inode = BPF_CORE_READ(file, f_inode);
    if (!inode)
        return 0;
    owner_uid = BPF_CORE_READ(inode, i_uid.val);
    if (owner_uid == 0) {
        bpf_printk("mmap_guard: denied writable shared mmap");
        return -EPERM;
    }
    return 0;
}
