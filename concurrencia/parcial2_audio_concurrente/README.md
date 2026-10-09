# Parcial 2 - Reproductor MP3 Concurrente

## Sistemas Operativos - SO2026B

Implementación en C de un reproductor de audio MP3 concurrente utilizando POSIX Threads y mecanismos explícitos de sincronización.

El programa permite reproducir audio mientras el usuario continúa interactuando con una interfaz de línea de comandos para pausar, detener, cambiar de pista y modificar la playlist.

---

## Características

El reproductor soporta:

- Reproducción de archivos MP3.
- Play.
- Pause.
- Stop.
- Next.
- Previous.
- Playlist dinámica.
- Agregar pistas durante la ejecución.
- Eliminar pistas durante la reproducción.
- Reordenar pistas durante la reproducción.
- Vaciar la playlist.
- Consulta del estado actual.
- Cambio automático de pista al finalizar una canción.
- Manejo de archivos inexistentes o inválidos.
- Cierre seguro de los hilos.

---

## Arquitectura

El programa utiliza tres flujos principales de ejecución:

```text
                     HILO PRINCIPAL
                         CLI
                          |
                comandos del usuario
                          |
                          v
                       Player
                state_mutex + cond
                          |
              +-----------+-----------+
              |                       |
              v                       v
       HILO DECODER               HILO AUDIO
          mpg123                     libao
              |                       ^
              v                       |
        +-----------------------------------+
        |       BUFFER CIRCULAR PCM         |
        |                                   |
        | mutex                             |
        | not_empty condition variable      |
        | not_full condition variable       |
        | generation por bloque             |
        +-----------------------------------+
                          ^
                          |
                    Playlist
                 pthread_rwlock
```

---

## Productor - Consumidor

El hilo decoder funciona como productor.

Su función es:

1. Leer el archivo MP3 actual.
2. Decodificarlo a PCM mediante `libmpg123`.
3. Dividir el PCM en bloques.
4. Insertar los bloques en un buffer circular compartido.

El hilo de audio funciona como consumidor.

Su función es:

1. Esperar hasta que exista PCM disponible.
2. Extraer bloques del buffer.
3. Validar que el bloque pertenezca a la reproducción actual.
4. Enviar el PCM a `libao`.

El buffer circular posee 32 posiciones de 4096 bytes.

La sincronización se realiza mediante:

- `pthread_mutex_t`
- `pthread_cond_t not_empty`
- `pthread_cond_t not_full`

Cuando el buffer está lleno, el productor duerme mediante `pthread_cond_wait()`.

Cuando está vacío, el consumidor duerme mediante `pthread_cond_wait()`.

Por lo tanto, no se utiliza espera activa.

---

## Generaciones de reproducción

Cada reproducción posee un número denominado `generation`.

Cada bloque PCM almacenado en el buffer guarda la generación a la cual pertenece.

Los comandos:

- `next`
- `previous`
- `stop`

incrementan la generación.

Si el consumidor encuentra un bloque perteneciente a una generación anterior, lo descarta.

Esto evita reproducir PCM perteneciente a una canción anterior después de un cambio de pista.

---

## Fin de pista

Cuando el decoder termina correctamente una canción, inserta en el buffer un marcador de fin de pista de tamaño cero.

El consumidor recibe este marcador únicamente después de consumir todos los bloques PCM anteriores.

En ese momento se selecciona la siguiente pista.

Esto evita cortar el final de una canción simplemente porque el decoder terminó de leer el archivo antes de que el dispositivo terminara de reproducir el PCM pendiente.

---

## Playlist concurrente

La playlist está implementada como un arreglo dinámico protegido mediante:

```c
pthread_rwlock_t
```

Las operaciones de lectura utilizan:

```c
pthread_rwlock_rdlock()
```

Las operaciones que modifican la playlist utilizan:

```c
pthread_rwlock_wrlock()
```

Esto permite que varios lectores consulten simultáneamente la playlist mientras las operaciones de escritura mantienen exclusión mutua.

Las operaciones soportadas son:

- agregar;
- eliminar;
- mover;
- consultar;
- avanzar;
- retroceder;
- limpiar.

Al eliminar o mover pistas, `current_index` se ajusta para seguir apuntando a la misma pista lógica siempre que sea posible.

---

## Estado del reproductor

Los estados disponibles son:

```text
PLAYER_STOPPED
PLAYER_PLAYING
PLAYER_PAUSED
PLAYER_SHUTDOWN
```

El estado está protegido mediante:

```c
pthread_mutex_t state_mutex
pthread_cond_t state_changed
```

Los workers esperan cambios mediante variables de condición, evitando ciclos de espera activa.

---

## Controles asíncronos

La reproducción ocurre en hilos diferentes al de la interfaz.

Por esta razón la terminal permanece disponible mientras se reproduce una canción.

Por ejemplo:

```text
player> play
[OK] PLAY

[decoder] Pista 1: canon.mp3

player> pause
[OK] PAUSE

player> play
[OK] PLAY

player> next
[OK] NEXT
```

Los comandos no esperan a que termine la canción.

---

## Manejo de pistas inválidas

Si una pista no puede abrirse, el decoder intenta continuar con la siguiente.

Si ninguna pista de la playlist puede abrirse, la reproducción cambia a:

```text
STOPPED
```

y no entra en un ciclo infinito intentando abrir continuamente los mismos archivos.

---

## Dependencias

En Ubuntu / WSL:

```bash
sudo apt update
sudo apt install build-essential pkg-config libmpg123-dev mpg123 libao-dev libao4
```

El proyecto utiliza:

- POSIX Threads (`pthread`)
- libmpg123
- libao

---

## Compilación

Desde:

```text
SO2026B/concurrencia/parcial2_audio_concurrente
```

ejecutar:

```bash
make
```

Esto genera:

```text
audio_player
```

Para limpiar:

```bash
make clean
```

---

## Ejecución

La forma recomendada es:

```bash
./run.sh
```

También puede ejecutarse:

```bash
make run
```

o:

```bash
./audio_player
```

---

## Comandos

```text
help
```

Muestra la ayuda.

```text
playlist
```

Muestra las pistas.

```text
add <archivo>
```

Agrega una pista.

Ejemplo:

```text
add ../concurrencia_ludica/music/canon.mp3
```

```text
remove <indice>
```

Elimina una pista.

```text
move <origen> <destino>
```

Reordena una pista.

```text
play
pause
stop
next
prev
```

Controlan la reproducción.

```text
status
```

Muestra el estado actual.

```text
clear
```

Detiene la reproducción y limpia la playlist.

```text
quit
```

Finaliza el programa y los hilos correctamente.

---

## Ejemplo

```text
player> add ../concurrencia_ludica/music/canon.mp3
[OK] Pista agregada

player> add ../concurrencia_ludica/music/reggae.mp3
[OK] Pista agregada

player> play
[OK] PLAY

[decoder] Pista 1: ../concurrencia_ludica/music/canon.mp3

player> next
[OK] NEXT

[decoder] Pista 2: ../concurrencia_ludica/music/reggae.mp3

player> pause
[OK] PAUSE

player> play
[OK] PLAY

player> stop
[OK] STOP
```

---

## Pruebas

Ejecutar:

```bash
make test
```

Incluye pruebas del:

- buffer productor-consumidor;
- acceso concurrente a playlist;
- máquina de estados del reproductor.

También existen pruebas específicas:

```bash
./tests/test_decoder ../concurrencia_ludica/music/canon.mp3
```

```bash
./tests/test_audio_pipeline ../concurrencia_ludica/music/canon.mp3
```

```bash
./tests/test_player_audio
```

La prueba de audio concurrente valida:

```text
PLAY
PAUSE
RESUME
NEXT
PREVIOUS
STOP
PLAY después de STOP
SHUTDOWN
```

---

## Validaciones realizadas

### Buffer concurrente

```text
Producidos : 1000
Consumidos : 1000

[OK] Buffer productor-consumidor correcto
```

### Playlist concurrente

Se realizaron múltiples lectores concurrentes junto con un escritor modificando la playlist.

```text
[OK] Playlist concurrente correcta
```

### Máquina de estados

```text
[OK] Inicial -> STOPPED
[OK] Play -> PLAYING
[OK] Pause -> PAUSED
[OK] Resume -> PLAYING
[OK] Next
[OK] Previous
[OK] Stop
[OK] Shutdown
```

### Pipeline MP3

Con `canon.mp3`:

```text
Formato : 48000 Hz, 2 canales

Producidos   : 19948032 bytes / 4871 bloques
Reproducidos : 19948032 bytes / 4871 bloques

[OK] Pipeline MP3 -> PCM -> Buffer -> Audio correcto
```

### Valgrind

Las pruebas propias fueron verificadas con Valgrind.

No se detectaron errores de acceso a memoria ni bloques definitivamente perdidos en el código del proyecto.

Las asignaciones marcadas como `still reachable` durante reproducción provienen de bibliotecas externas de audio como PulseAudio/libao.

---

## Sincronización utilizada

| Recurso | Mecanismo |
|---|---|
| Buffer circular | `pthread_mutex_t` |
| Buffer vacío | `pthread_cond_t not_empty` |
| Buffer lleno | `pthread_cond_t not_full` |
| Playlist | `pthread_rwlock_t` |
| Estado Player | `pthread_mutex_t` |
| Cambio de estado | `pthread_cond_t state_changed` |
| Hilo decoder | `pthread_create` / `pthread_join` |
| Hilo audio | `pthread_create` / `pthread_join` |

No se utilizan ciclos de espera activa para sincronizar los componentes.

---

## Módulos

```text
main.c
```

Interfaz de usuario y comandos.

```text
player.c / player.h
```

Estado global, coordinación de hilos y controles asíncronos.

```text
playlist.c / playlist.h
```

Playlist compartida protegida mediante Readers-Writers.

```text
audio_buffer.c / audio_buffer.h
```

Buffer circular productor-consumidor.

```text
decoder.c / decoder.h
```

Decodificación MP3 -> PCM utilizando mpg123.

```text
audio_output.c / audio_output.h
```

Salida PCM utilizando libao.

```text
tests/
```

Pruebas unitarias, concurrentes y de integración.

---

## Autor

Miguel Angel Alzate Osorno

Sistemas Operativos - SO2026B