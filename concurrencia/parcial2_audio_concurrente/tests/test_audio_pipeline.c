#include "../audio_output.h"
#include "../decoder.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    AudioBuffer *buffer;
    const char *path;

    AudioFormat format;

    size_t produced_bytes;
    size_t produced_blocks;

    int result;
} DecoderContext;

typedef struct {
    AudioBuffer *buffer;
    AudioFormat format;

    AudioOutputStats stats;

    int result;
} OutputContext;

static void *decoder_thread(void *arg)
{
    DecoderContext *ctx = (DecoderContext *)arg;

    AudioFormat decoded_format;

    ctx->result = decoder_decode_file(
        ctx->path,
        ctx->buffer,
        &decoded_format,
        &ctx->produced_bytes,
        &ctx->produced_blocks
    );

    /*
     * El consumidor ya recibió previamente el formato
     * mediante decoder_probe_file().
     */
    return NULL;
}

static void *output_thread(void *arg)
{
    OutputContext *ctx = (OutputContext *)arg;

    ctx->result = audio_output_play(
        ctx->buffer,
        &ctx->format,
        &ctx->stats
    );

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(
            stderr,
            "Uso: %s <archivo.mp3>\n",
            argv[0]
        );

        return EXIT_FAILURE;
    }

    if (decoder_library_init() != 0) {
        return EXIT_FAILURE;
    }

    audio_output_library_init();

    AudioFormat format;

    if (decoder_probe_file(argv[1], &format) != 0) {
        fprintf(stderr, "No se pudo consultar el formato MP3\n");

        audio_output_library_cleanup();
        decoder_library_cleanup();

        return EXIT_FAILURE;
    }

    printf("=== PIPELINE DE AUDIO CONCURRENTE ===\n\n");

    printf(
        "Archivo : %s\n",
        argv[1]
    );

    printf(
        "Formato : %ld Hz, %d canal(es), PCM 16 bits\n\n",
        format.rate,
        format.channels
    );

    AudioBuffer buffer;

    if (audio_buffer_init(&buffer) != 0) {
        fprintf(stderr, "No se pudo crear AudioBuffer\n");

        audio_output_library_cleanup();
        decoder_library_cleanup();

        return EXIT_FAILURE;
    }

    DecoderContext decoder = {
        .buffer = &buffer,
        .path = argv[1],
        .format = format,
        .produced_bytes = 0,
        .produced_blocks = 0,
        .result = -1
    };

    OutputContext output = {
        .buffer = &buffer,
        .format = format,
        .stats = {0},
        .result = -1
    };

    pthread_t decoder_tid;
    pthread_t output_tid;

    /*
     * Iniciamos primero el consumidor.
     * Como el buffer está vacío, dormirá en not_empty.
     */
    if (
        pthread_create(
            &output_tid,
            NULL,
            output_thread,
            &output
        ) != 0
    ) {
        fprintf(stderr, "No se pudo crear hilo de audio\n");

        audio_buffer_destroy(&buffer);
        audio_output_library_cleanup();
        decoder_library_cleanup();

        return EXIT_FAILURE;
    }

    if (
        pthread_create(
            &decoder_tid,
            NULL,
            decoder_thread,
            &decoder
        ) != 0
    ) {
        fprintf(stderr, "No se pudo crear hilo decoder\n");

        audio_buffer_shutdown(&buffer);
        pthread_join(output_tid, NULL);

        audio_buffer_destroy(&buffer);
        audio_output_library_cleanup();
        decoder_library_cleanup();

        return EXIT_FAILURE;
    }

    pthread_join(decoder_tid, NULL);

    /*
     * Ya no se producirán más bloques.
     * El consumidor terminará después de vaciar
     * lo que todavía quede en el buffer.
     */
    audio_buffer_shutdown(&buffer);

    pthread_join(output_tid, NULL);

    printf("\n=== RESULTADO ===\n");

    printf(
        "Producidos : %zu bytes / %zu bloques\n",
        decoder.produced_bytes,
        decoder.produced_blocks
    );

    printf(
        "Reproducidos: %zu bytes / %zu bloques\n",
        output.stats.played_bytes,
        output.stats.played_blocks
    );

    int failed = 0;

    if (decoder.result != 0) {
        fprintf(stderr, "[ERROR] Falló decoder\n");
        failed = 1;
    }

    if (output.result != 0) {
        fprintf(stderr, "[ERROR] Falló salida de audio\n");
        failed = 1;
    }

    if (
        decoder.produced_bytes != output.stats.played_bytes ||
        decoder.produced_blocks != output.stats.played_blocks
    ) {
        fprintf(
            stderr,
            "[ERROR] Productor y consumidor no coinciden\n"
        );

        failed = 1;
    }

    audio_buffer_destroy(&buffer);

    audio_output_library_cleanup();
    decoder_library_cleanup();

    if (failed) {
        fprintf(stderr, "\n[ERROR] Pipeline fallido\n");
        return EXIT_FAILURE;
    }

    printf(
        "\n[OK] Pipeline MP3 -> PCM -> Buffer -> Audio correcto\n"
    );

    return EXIT_SUCCESS;
}