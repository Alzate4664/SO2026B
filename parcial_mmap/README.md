# Seguridad de archivos mapeados con `mmap`

Implementación de referencia para el parcial: un módulo LSM que intercepta el
hook `file_mmap` y rechaza mapeos `MAP_SHARED | PROT_WRITE` sobre archivos
regulares cuyo propietario es `root`. La política es deliberadamente pequeña y
auditable; no intenta reemplazar SELinux/AppArmor ni modificar la syscall table.

## Qué reutiliza del repositorio

- `shell/cat_memoria.c` y el comando `m_mmap`: ejemplos de asignación y acceso a
  memoria mapeada desde espacio de usuario.
- `callsys/file_manager.c`: manejo de descriptores, errores y metadatos de
  archivos.
- `concurrencia/HILOS_PROCESOS` y `concurrencia/parcial2_audio_concurrente`:
  patrones de procesos, `fork`, `wait`, locks y pruebas repetibles.

## Arquitectura

`mmap(2) -> security_file_mmap() -> mmap_file_lsm() -> permitir/denegar`

El hook se ejecuta al crear el VMA, antes de que el proceso pueda tocar la
primera página. El módulo no inspecciona cada lectura: eso requeriría instrumentar
fallos de página y tendría un coste mucho mayor. `MAP_PRIVATE` queda permitido
porque sus escrituras son COW y no persisten en el archivo; `MAP_SHARED` writable
se deniega para el caso protegido. Las decisiones se registran con `pr_info`.

## Compilación y carga

```bash
make
sudo insmod mmap_guard.ko
dmesg | tail -n 20
sudo rmmod mmap_guard
make clean
```

Se necesita un kernel con headers y soporte LSM modular. En kernels con LSM
integrity/confidentiality política estricta, úsese una VM de laboratorio.

## Pruebas

```bash
make -C tests
sudo ./tests/mmap_policy_test /etc/hostname
```

El test verifica que `MAP_PRIVATE | PROT_WRITE` no altera el archivo y que el
caso `MAP_SHARED | PROT_WRITE` recibe `EPERM` cuando el archivo cumple la regla.
La prueba no requiere explotar Dirty COW ni desactivar controles del kernel.

## Limitaciones

La política de ejemplo usa propietario UID 0 como marcador de archivo sensible;
en una entrega real debe sustituirse por una etiqueta LSM, un conjunto de inodos
gestionado por política o SELinux. El módulo no cifra memoria, no protege contra
un proceso root y no es una defensa completa contra DMA o lectura física.
