#include "audio_output.h"

#include <ao/ao.h>

#include <stdio.h>
#include <string.h>

void audio_output_library_init(void)
{
    ao_initialize();
}

void audio_output_library_cleanup(void)
{
    ao_shutdown();
}

int audio_output_play(
    AudioBuffer *buffer,
    const AudioFormat *format,
    AudioOutputStats *stats)
{
    if (
        buffer == NULL ||
        format == NULL ||
        stats == NULL
    ) {
        return -1;
    }

    stats->played_bytes = 0;
    stats->played_blocks = 0;

    int driver_id = ao_default_driver_id();

    if (driver_id < 0) {
        fprintf(stderr, "No se encontró dispositivo de audio libao\n");
        return -1;
    }

    ao_sample_format ao_format;

    memset(&ao_format, 0, sizeof(ao_format));

    ao_format.bits = 16;
    ao_format.rate = (int)format->rate;
    ao_format.channels = format->channels;
    ao_format.byte_format = AO_FMT_NATIVE;
    ao_format.matrix = NULL;

    ao_device *device = ao_open_live(
        driver_id,
        &ao_format,
        NULL
    );

    if (device == NULL) {
        fprintf(
            stderr,
            "No se pudo abrir el dispositivo de audio\n"
        );

        return -1;
    }

    unsigned char pcm[AUDIO_BLOCK_SIZE];

    while (1) {
        size_t size = 0;

        if (
            audio_buffer_read(
                buffer,
                pcm,
                &size
            ) != 0
        ) {
            break;
        }

        if (size == 0) {
            continue;
        }

        if (
            ao_play(
                device,
                (char *)pcm,
                (uint_32)size
            ) == 0
        ) {
            fprintf(stderr, "Error reproduciendo bloque PCM\n");

            ao_close(device);

            return -1;
        }

        stats->played_bytes += size;
        stats->played_blocks++;
    }

    ao_close(device);

    return 0;
}