#ifndef DECODER_H
#define DECODER_H

#include <stddef.h>

#include "audio_buffer.h"

typedef struct {
    long rate;
    int channels;
    int encoding;
} AudioFormat;

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