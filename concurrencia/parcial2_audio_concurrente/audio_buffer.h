#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <pthread.h>
#include <stddef.h>

#define AUDIO_BUFFER_SLOTS 32
#define AUDIO_BLOCK_SIZE 4096

typedef struct {
    unsigned char data[AUDIO_BUFFER_SLOTS][AUDIO_BLOCK_SIZE];
    size_t sizes[AUDIO_BUFFER_SLOTS];

    int read_pos;
    int write_pos;
    int count;

    int shutdown;

    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} AudioBuffer;

/* Inicializa el buffer y sus mecanismos de sincronización. */
int audio_buffer_init(AudioBuffer *buffer);

/* Escribe un bloque. Se bloquea si el buffer está lleno. */
int audio_buffer_write(
    AudioBuffer *buffer,
    const unsigned char *data,
    size_t size
);

/* Lee un bloque. Se bloquea si el buffer está vacío. */
int audio_buffer_read(
    AudioBuffer *buffer,
    unsigned char *data,
    size_t *size
);

/* Vacía el contenido pendiente del buffer. */
void audio_buffer_clear(AudioBuffer *buffer);

/* Despierta los hilos bloqueados y marca el cierre. */
void audio_buffer_shutdown(AudioBuffer *buffer);

/* Libera mutex y variables de condición. */
void audio_buffer_destroy(AudioBuffer *buffer);

#endif