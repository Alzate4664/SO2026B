#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BPFTS="/sys/fs/bpf"
PIN="$BPFTS/mmap_guard"
BPFTool="${BPFTool:-/usr/local/bin/bpftool}"

sudo mkdir -p "$BPFTS"
sudo mountpoint -q "$BPFTS" || sudo mount -t bpf bpf "$BPFTS"
sudo rm -rf "$PIN"
sudo prlimit --memlock=unlimited -- "$BPFTool" prog loadall "$ROOT/mmap_guard.bpf.o" "$PIN" type lsm autoattach
sudo "$BPFTool" link show
echo "mmap_guard eBPF-LSM cargado"
