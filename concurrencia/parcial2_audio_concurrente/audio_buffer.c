#include "audio_buffer.h"

#include <errno.h>
#include <string.h>

int audio_buffer_init(AudioBuffer *buffer)
{
    if (buffer == NULL) {
        return -1;
    }

    memset(buffer, 0, sizeof(*buffer));

    if (pthread_mutex_init(&buffer->mutex, NULL) != 0) {
        return -1;
    }

    if (pthread_cond_init(&buffer->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&buffer->mutex);
        return -1;
    }

    if (pthread_cond_init(&buffer->not_full, NULL) != 0) {
        pthread_cond_destroy(&buffer->not_empty);
        pthread_mutex_destroy(&buffer->mutex);
        return -1;
    }

    return 0;
}

int audio_buffer_write(
    AudioBuffer *buffer,
    const unsigned char *data,
    size_t size)
{
    if (buffer == NULL || data == NULL || size > AUDIO_BLOCK_SIZE) {
        return -1;
    }

    pthread_mutex_lock(&buffer->mutex);

    /*
     * Si el buffer está lleno, el productor duerme.
     *
     * Usamos while y no if porque pthread_cond_wait puede
     * despertar incluso si la condición todavía no se cumple.
     */
    while (buffer->count == AUDIO_BUFFER_SLOTS && !buffer->shutdown) {
        pthread_cond_wait(&buffer->not_full, &buffer->mutex);
    }

    if (buffer->shutdown) {
        pthread_mutex_unlock(&buffer->mutex);
        return -1;
    }

    memcpy(buffer->data[buffer->write_pos], data, size);
    buffer->sizes[buffer->write_pos] = size;

    buffer->write_pos =
        (buffer->write_pos + 1) % AUDIO_BUFFER_SLOTS;

    buffer->count++;

    /*
     * Ahora existe al menos un elemento.
     * Despertamos al consumidor.
     */
    pthread_cond_signal(&buffer->not_empty);

    pthread_mutex_unlock(&buffer->mutex);

    return 0;
}

int audio_buffer_read(
    AudioBuffer *buffer,
    unsigned char *data,
    size_t *size)
{
    if (buffer == NULL || data == NULL || size == NULL) {
        return -1;
    }

    pthread_mutex_lock(&buffer->mutex);

    /*
     * Si está vacío, el consumidor duerme.
     * No hacemos busy waiting.
     */
    while (buffer->count == 0 && !buffer->shutdown) {
        pthread_cond_wait(&buffer->not_empty, &buffer->mutex);
    }

    /*
     * Si estamos cerrando y no queda nada por consumir,
     * terminamos.
     */
    if (buffer->shutdown && buffer->count == 0) {
        pthread_mutex_unlock(&buffer->mutex);
        return -1;
    }

    *size = buffer->sizes[buffer->read_pos];

    memcpy(
        data,
        buffer->data[buffer->read_pos],
        *size
    );

    buffer->read_pos =
        (buffer->read_pos + 1) % AUDIO_BUFFER_SLOTS;

    buffer->count--;

    /*
     * Se liberó al menos una posición.
     * Despertamos al productor.
     */
    pthread_cond_signal(&buffer->not_full);

    pthread_mutex_unlock(&buffer->mutex);

    return 0;
}

void audio_buffer_clear(AudioBuffer *buffer)
{
    if (buffer == NULL) {
        return;
    }

    pthread_mutex_lock(&buffer->mutex);

    buffer->read_pos = 0;
    buffer->write_pos = 0;
    buffer->count = 0;

    /*
     * Si había un productor esperando porque estaba lleno,
     * ahora puede continuar.
     */
    pthread_cond_broadcast(&buffer->not_full);

    pthread_mutex_unlock(&buffer->mutex);
}

void audio_buffer_shutdown(AudioBuffer *buffer)
{
    if (buffer == NULL) {
        return;
    }

    pthread_mutex_lock(&buffer->mutex);

    buffer->shutdown = 1;

    /*
     * Puede haber tanto un productor como un consumidor
     * dormidos. Hay que despertar a ambos para permitirles salir.
     */
    pthread_cond_broadcast(&buffer->not_empty);
    pthread_cond_broadcast(&buffer->not_full);

    pthread_mutex_unlock(&buffer->mutex);
}

void audio_buffer_destroy(AudioBuffer *buffer)
{
    if (buffer == NULL) {
        return;
    }

    pthread_cond_destroy(&buffer->not_empty);
    pthread_cond_destroy(&buffer->not_full);
    pthread_mutex_destroy(&buffer->mutex);
}