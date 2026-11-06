CLANG ?= clang
BPFTool ?= /usr/local/bin/bpftool
BPF_CFLAGS := -O2 -g -target bpf -D__TARGET_ARCH_x86
KERNEL_BPF_HEADERS ?= /home/alzate46/kernel-src/wsl-kernel/tools/bpf/bpftool/libbpf/include
KERNEL_BPF_GENERATED ?= /home/alzate46/kernel-src/wsl-kernel/tools/bpf/bpftool/libbpf/include

all: mmap_guard.bpf.o tests/mmap_policy_test

vmlinux.h:
	$(BPFTool) btf dump file /sys/kernel/btf/vmlinux format c > $@

mmap_guard.bpf.o: mmap_guard.bpf.c vmlinux.h
	$(CLANG) $(BPF_CFLAGS) -I. -I$(KERNEL_BPF_HEADERS) -I$(KERNEL_BPF_GENERATED) -I/tmp/wsl-kernel/tools/lib -c $< -o $@

tests/mmap_policy_test: tests/mmap_policy_test.c
	$(CC) -O2 -Wall -Wextra -Wpedantic -o $@ $<

clean:
	rm -f mmap_guard.bpf.o vmlinux.h tests/mmap_policy_test

.PHONY: all clean
