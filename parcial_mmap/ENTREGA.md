# Entrega final - Parcial de Sistemas Operativos

## Implementación

La solución principal es `mmap_guard.bpf.c`, un programa eBPF-LSM asociado al
hook `mmap_file`. Deniega `PROT_WRITE | MAP_SHARED` cuando el archivo regular
pertenece a UID 0. `MAP_PRIVATE` continúa permitido y conserva COW.

El LKM `mmap_guard.c` se conserva como alternativa documentada, pero no es la
implementación final porque `security_add_hooks` y `security_hook_heads` no son
exportados por el kernel WSL para módulos externos.

## Archivos que deben estar en el repositorio

```text
parcial_mmap/
├── Makefile
├── Makefile.ebpf
├── mmap_guard.bpf.c
├── load_ebpf.sh
├── unload_ebpf.sh
├── README.md
├── INFORME.md
├── evidencia_bpftool.txt
├── evidencia_prueba.txt
├── evidencia_lsm.txt
├── evidencia_strace.txt
├── evidencia_perf.txt
└── tests/mmap_policy_test.c
```

`vmlinux.h` y `mmap_guard.bpf.o` pueden generarse localmente y no son
obligatorios si el Makefile los produce. No incluir binarios ni archivos
temporales en el ZIP final salvo que el docente los solicite.

## Verificación final

```bash
make clean
make
sudo ./load_ebpf.sh
sudo ./tests/mmap_policy_test > evidencia_prueba.txt
sudo bpftool link show > evidencia_bpftool.txt
cat /sys/kernel/security/lsm > evidencia_lsm.txt
strace -f -e mmap,msync ./tests/mmap_policy_test 2> evidencia_strace.txt
sudo perf stat -e page-faults,context-switches \
  ./tests/mmap_policy_test 2> evidencia_perf.txt || true
sudo ./unload_ebpf.sh
```

La evidencia principal debe contener `MAP_SHARED writable: denied` y `private
write is COW`. Si `perf` no está disponible en WSL, documentarlo en el informe.

## Empaquetado

Desde la raíz del repositorio:

```bash
git status --short
git add parcial_mmap
git commit -m "feat: add eBPF LSM mmap security solution"
git archive --format=tar.gz --output=SO2026B-parcial-mmap.tar.gz HEAD parcial_mmap
```

Entregar el enlace del repositorio o el archivo `SO2026B-parcial-mmap.tar.gz`,
junto con el informe PDF/DOCX y las evidencias individuales solicitadas por
Interactiva EAFIT.
