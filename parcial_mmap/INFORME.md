# Informe técnico: seguridad de archivos con `mmap`

## Decisión

Se selecciona un LSM con `file_mmap`, porque intercepta la creación del mapeo
en kernel sin depender de `LD_PRELOAD` ni de una syscall table modificable. La
regla demostrada deniega `PROT_WRITE|MAP_SHARED` en archivos regulares de UID 0.
`MAP_PRIVATE` se mantiene permitido: su escritura dispara COW y no debe volver
al archivo original.

## Respuestas diagnósticas

1. `mmap` valida argumentos, crea un `vm_area_struct` y delega en el sistema de
   archivos; normalmente no carga páginas. En el primer acceso la MMU no
   encuentra una PTE válida, entra al page-fault handler, Linux busca el VMA,
   verifica permisos, obtiene una página de page cache o anónima, instala la
   PTE y reintenta la instrucción.
2. `MAP_SHARED` comparte modificaciones y páginas sucias con el archivo; requiere
   permisos de escritura coherentes. `MAP_PRIVATE` usa una vista privada y COW.
3. COW comparte una página marcada de solo lectura; la primera escritura provoca
   copia, actualización de la PTE y escritura sobre la copia. Errores en
   `VM_MAYWRITE`/protecciones pueden convertir una copia en modificación del
   archivo o permitir escritura no autorizada.
4. Un hook LSM es la interfaz soportada y compone con otras políticas. Un LKM que
   parchea `sys_call_table` es frágil, dependiente de versión y fácil de romper.
5. El cifrado transparente debe ocurrir debajo de la interfaz de page cache (por
   ejemplo fscrypt/filesystem); el usuario autorizado ve datos descifrados a
   través de sus páginas, pero la autorización sigue siendo la del VMA y del
   archivo. La RAM no debe considerarse confidencial frente a root/DMA.
6. `msync`/writeback marca, bloquea y escribe páginas sucias; inspeccionar solo
   `write(2)` no cubre escrituras por un VMA. Por eso esta defensa decide al crear
   el VMA y evita el caso compartido peligroso.
7. eBPF puede observar `mmap` y fallos de página con tracepoints/kprobes/fentry,
   registrando PID, inode, prot y flags; para bloqueo debe usarse LSM-BPF o un
   LSM tradicional, no un kprobe que intente cambiar el resultado.
8. `PROT_READ|PROT_WRITE|MAP_PRIVATE` puede ser válido con un descriptor de solo
   lectura: la escritura es privada y COW. No permite alterar el archivo, pero
   puede crear una copia temporal con datos sensibles en memoria del proceso.
9. El kernel usa locks del mm, referencias de VMA y reglas COW para serializar
   `madvise` y faults; una aplicación no debe asumir que un puntero privado es
   estable durante concurrencia y debe sincronizar sus propios accesos.
10. PGD/P4D/PUD/PMD/PTE traducen virtual a físico. `R/W`, `U/S`, `NX`, bits de
    dirty/accessed y `struct page` limitan escritura, ejecución y acceso de
    usuarios; no sustituyen LSM ni protecciones contra DMA.

## IA y verificación

| Contexto | Prompt estructurado | Verificación |
|---|---|---|
| Exploración | “Actúa como mantenedor Linux 6.x; compara LSM file_mmap, kprobe y syscall table; exige interfaces soportadas.” | Contrastar con headers y documentación del kernel usado. |
| Diseño | “Diseña una política que niegue solo MAP_SHARED writable en archivos root-owned; conserva COW y devuelve errno.” | Revisar flags, permisos y ausencia de escrituras sobre PTE. |
| Código | “Genera módulo C externo con Makefile Kbuild, sin CR0 ni parcheo de syscall table.” | `make`, `modpost`, revisión manual y `dmesg`. |
| Pruebas | “Crea prueba no destructiva para comparar MAP_PRIVATE y MAP_SHARED, reportando EPERM.” | Ejecutar en VM, revisar hash/tamaño y retirar el módulo. |

Se descartan sugerencias de funciones internas no exportadas, hooks inexistentes,
parcheo de `sys_call_table` y deshabilitar WP/CR0. La validación final debe
compilar contra `/lib/modules/$(uname -r)/build`, inspeccionar warnings y probar
en una VM; el montaje actual de WSL desde Windows bloqueó `wsl.exe`, por lo que
esa compilación debe ejecutarse desde una terminal Ubuntu/WSL interactiva.

## Trade-offs y métricas

| Opción | Seguridad | Coste |
|---|---|---|
| LSM `file_mmap` | Bloqueo temprano y auditable | Un chequeo por creación de VMA |
| eBPF observacional | Excelente visibilidad | No bloquea por sí solo en todos los kernels |
| Inspección por page fault | Gran detalle | Coste por fault y complejidad de concurrencia |
| Wrapper de usuario | Fácil de probar | Evasión trivial con syscall directa |

Para la entrega se deben registrar `strace -f -e mmap,msync`, `perf stat -e
page-faults`, tiempo de `make` y el resultado de la prueba con el módulo cargado
y descargado. No se debe probar contra archivos de sistema modificables ni
ejecutar PoCs de Dirty COW fuera de una VM desechable.
