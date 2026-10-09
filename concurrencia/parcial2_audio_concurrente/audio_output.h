#ifndef AUDIO_OUTPUT_H
#define AUDIO_OUTPUT_H

#include <stddef.h>
#include <ao/ao.h>

#include "audio_buffer.h"
#include "decoder.h"

/*
 * Estadísticas usadas por la prueba del pipeline completo.
 */
typedef struct {
    size_t played_bytes;
    size_t played_blocks;
} AudioOutputStats;

/*
 * Dispositivo persistente de salida.
 *
 * El reproductor real lo mantiene abierto mientras reproduce
 * bloques PCM, evitando abrir/cerrar libao para cada bloque.
 */
typedef struct {
    ao_device *device;
    AudioFormat format;
    int opened;
} AudioOutputDevice;

/*
 * Inicializa la biblioteca libao.
 * Se llama una vez antes de utilizar audio.
 */
void audio_output_library_init(void);

/*
 * Cierra la biblioteca libao.
 */
void audio_output_library_cleanup(void);

/*
 * Abre el dispositivo de audio usando el formato PCM indicado.
 */
int audio_output_open(
    AudioOutputDevice *output,
    const AudioFormat *format
);

/*
 * Reproduce un único bloque PCM usando un dispositivo
 * previamente abierto.
 */
int audio_output_write(
    AudioOutputDevice *output,
    const unsigned char *data,
    size_t size
);

/*
 * Cierra un dispositivo de audio abierto.
 */
void audio_output_close(
    AudioOutputDevice *output
);

/*
 * Función de alto nivel utilizada por nuestras pruebas anteriores.
 *
 * Consume todo el AudioBuffer y lo envía al dispositivo.
 * La conservamos para no romper test_audio_pipeline.
 */
int audio_output_play(
    AudioBuffer *buffer,
    const AudioFormat *format,
    AudioOutputStats *stats
);

#endif