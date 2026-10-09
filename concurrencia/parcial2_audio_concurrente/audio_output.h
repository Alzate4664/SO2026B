#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <stddef.h>

#include "audio_buffer.h"
#include "decoder.h"

typedef struct {
    size_t played_bytes;
    size_t played_blocks;
} AudioOutputStats;

/*
 * Inicializa la biblioteca libao.
 */
void audio_output_library_init(void);

/*
 * Cierra la biblioteca libao.
 */
void audio_output_library_cleanup(void);

/*
 * Consume PCM del buffer circular y lo envía
 * al dispositivo de audio.
 */
int audio_output_play(
    AudioBuffer *buffer,
    const AudioFormat *format,
    AudioOutputStats *stats
);

#endif