#include "../decoder.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    AudioBuffer *buffer;
    const char *path;

    AudioFormat format;

    size_t decoded_bytes;
    size_t decoded_blocks;

    int result;
} ProducerContext;

typedef struct {
    AudioBuffer *buffer;

    size_t consumed_bytes;
    size_t consumed_blocks;
} ConsumerContext;

static void *decoder_thread(void *arg)
{
    ProducerContext *ctx = (ProducerContext *)arg;

    ctx->result = decoder_decode_file(
        ctx->path,
        ctx->buffer,
        &ctx->format,
        &ctx->decoded_bytes,
        &ctx->decoded_blocks
    );

    return NULL;
}

static void *consumer_thread(void *arg)
{
    ConsumerContext *ctx = (ConsumerContext *)arg;

    unsigned char pcm[AUDIO_BLOCK_SIZE];

    while (1) {
        size_t size = 0;

        if (
            audio_buffer_read(
                ctx->buffer,
                pcm,
                &size
            ) != 0
        ) {
            break;
        }

        ctx->consumed_bytes += size;
        ctx->consumed_blocks++;
    }

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

    AudioBuffer buffer;

    if (audio_buffer_init(&buffer) != 0) {
        fprintf(stderr, "No se pudo inicializar AudioBuffer\n");

        decoder_library_cleanup();

        return EXIT_FAILURE;
    }

    ProducerContext producer = {
        .buffer = &buffer,
        .path = argv[1],
        .decoded_bytes = 0,
        .decoded_blocks = 0,
        .result = -1
    };

    ConsumerContext consumer = {
        .buffer = &buffer,
        .consumed_bytes = 0,
        .consumed_blocks = 0
    };

    pthread_t producer_tid;
    pthread_t consumer_tid;

    printf("=== TEST DECODER MP3 -> PCM -> BUFFER ===\n\n");

    /*
     * Arrancamos primero el consumidor.
     * Al no haber datos quedará dormido en not_empty.
     */
    if (
        pthread_create(
            &consumer_tid,
            NULL,
            consumer_thread,
            &consumer
        ) != 0
    ) {
        fprintf(stderr, "Error creando consumidor\n");

        audio_buffer_destroy(&buffer);
        decoder_library_cleanup();

        return EXIT_FAILURE;
    }

    if (
        pthread_create(
            &producer_tid,
            NULL,
            decoder_thread,
            &producer
        ) != 0
    ) {
        fprintf(stderr, "Error creando decoder\n");

        audio_buffer_shutdown(&buffer);
        pthread_join(consumer_tid, NULL);

        audio_buffer_destroy(&buffer);
        decoder_library_cleanup();

        return EXIT_FAILURE;
    }

    /*
     * Esperamos hasta que mpg123 termine el archivo.
     */
    pthread_join(producer_tid, NULL);

    /*
     * Ahora avisamos al consumidor de que no habrá más PCM.
     *
     * audio_buffer_read() seguirá consumiendo cualquier bloque
     * pendiente y terminará cuando el buffer quede vacío.
     */
    audio_buffer_shutdown(&buffer);

    pthread_join(consumer_tid, NULL);

    int failed = 0;

    if (producer.result != 0) {
        fprintf(stderr, "[ERROR] Decoder devolvio error\n");
        failed = 1;
    }

    printf(
        "Formato         : %ld Hz, %d canal(es)\n",
        producer.format.rate,
        producer.format.channels
    );

    printf(
        "PCM producido   : %zu bytes en %zu bloques\n",
        producer.decoded_bytes,
        producer.decoded_blocks
    );

    printf(
        "PCM consumido   : %zu bytes en %zu bloques\n",
        consumer.consumed_bytes,
        consumer.consumed_blocks
    );

    if (
        producer.decoded_bytes == 0 ||
        producer.decoded_blocks == 0
    ) {
        fprintf(stderr, "[ERROR] No se genero PCM\n");
        failed = 1;
    }

    if (
        producer.decoded_bytes != consumer.consumed_bytes
    ) {
        fprintf(
            stderr,
            "[ERROR] Bytes producidos y consumidos no coinciden\n"
        );

        failed = 1;
    }

    if (
        producer.decoded_blocks != consumer.consumed_blocks
    ) {
        fprintf(
            stderr,
            "[ERROR] Bloques producidos y consumidos no coinciden\n"
        );

        failed = 1;
    }

    audio_buffer_destroy(&buffer);
    decoder_library_cleanup();

    if (failed) {
        fprintf(stderr, "\n[ERROR] Test decoder fallido\n");
        return EXIT_FAILURE;
    }

    printf(
        "\n[OK] MP3 decodificado y transferido correctamente "
        "por el buffer concurrente\n"
    );

    return EXIT_SUCCESS;
}