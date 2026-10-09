#ifndef DECODER_H
#define DECODER_H

#include <stddef.h>

#include "audio_buffer.h"

typedef struct {
    long rate;
    int channels;
    int encoding;
} AudioFormat;

typedef int (*DecoderContinueCallback)(
    void *context,
    unsigned long generation
);

/*
 * Inicializa la biblioteca mpg123.
 * Debe llamarse una vez antes de decodificar.
 */
int decoder_library_init(void);

/*
 * Libera los recursos globales de mpg123.
 */
void decoder_library_cleanup(void);

/*
 * Consulta el formato PCM de un MP3 sin decodificarlo completo.
 */
int decoder_probe_file(
    const char *path,
    AudioFormat *format
);

/*
 * Versión utilizada por el reproductor real.
 *
 * Cada bloque PCM queda marcado con generation.
 * El callback permite interrumpir la decodificación si
 * ocurre next, previous, stop o shutdown.
 */
int decoder_decode_file_tagged(
    const char *path,
    AudioBuffer *buffer,
    AudioFormat *format,
    size_t *total_bytes,
    size_t *total_blocks,
    unsigned long generation,
    DecoderContinueCallback should_continue,
    void *callback_context
);

/*
 * Decodifica un archivo MP3 a PCM y deposita los bloques
 * resultantes dentro del AudioBuffer.
 */
int decoder_decode_file(
    const char *path,
    AudioBuffer *buffer,
    AudioFormat *format,
    size_t *total_bytes,
    size_t *total_blocks
);

#endif