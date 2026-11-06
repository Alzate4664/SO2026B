#!/usr/bin/env bash
set -euo pipefail
sudo rm -rf /sys/fs/bpf/mmap_guard
echo "mmap_guard eBPF-LSM descargado"
