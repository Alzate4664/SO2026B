#include "../audio_buffer.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TOTAL_BLOCKS 1000

typedef struct {
    AudioBuffer *buffer;
    int produced;
    int consumed;
} TestContext;

void *producer_thread(void *arg)
{
    TestContext *ctx = (TestContext *)arg;

    unsigned char data[AUDIO_BLOCK_SIZE];

    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        int written = snprintf(
            (char *)data,
            sizeof(data),
            "BLOCK-%d",
            i
        );

        if (written < 0) {
            fprintf(stderr, "Error creando bloque %d\n", i);
            return NULL;
        }

        if (audio_buffer_write(
                ctx->buffer,
                data,
                (size_t)written + 1
            ) != 0) {
            fprintf(stderr, "Error escribiendo bloque %d\n", i);
            return NULL;
        }

        ctx->produced++;
    }

    return NULL;
}

void *consumer_thread(void *arg)
{
    TestContext *ctx = (TestContext *)arg;

    unsigned char data[AUDIO_BLOCK_SIZE];
    size_t size;

    for (int i = 0; i < TOTAL_BLOCKS; i++) {
        if (audio_buffer_read(
                ctx->buffer,
                data,
                &size
            ) != 0) {
            fprintf(stderr, "Error leyendo bloque %d\n", i);
            return NULL;
        }

        char expected[64];

        snprintf(
            expected,
            sizeof(expected),
            "BLOCK-%d",
            i
        );

        if (strcmp((char *)data, expected) != 0) {
            fprintf(
                stderr,
                "ERROR: esperado '%s' pero recibido '%s'\n",
                expected,
                data
            );

            return NULL;
        }

        ctx->consumed++;

        /*
         * Frenamos levemente al consumidor a propósito.
         * Esto obliga a que el buffer se llene y el productor
         * tenga que esperar usando not_full.
         */
        if (i % 50 == 0) {
            usleep(1000);
        }
    }

    return NULL;
}

int main(void)
{
    AudioBuffer buffer;

    if (audio_buffer_init(&buffer) != 0) {
        fprintf(stderr, "No se pudo inicializar el buffer\n");
        return EXIT_FAILURE;
    }

    TestContext ctx = {
        .buffer = &buffer,
        .produced = 0,
        .consumed = 0
    };

    pthread_t producer;
    pthread_t consumer;

    printf("=== TEST PRODUCTOR-CONSUMIDOR ===\n\n");

    if (pthread_create(
            &producer,
            NULL,
            producer_thread,
            &ctx
        ) != 0) {
        fprintf(stderr, "Error creando productor\n");
        audio_buffer_destroy(&buffer);
        return EXIT_FAILURE;
    }

    if (pthread_create(
            &consumer,
            NULL,
            consumer_thread,
            &ctx
        ) != 0) {
        fprintf(stderr, "Error creando consumidor\n");

        audio_buffer_shutdown(&buffer);
        pthread_join(producer, NULL);

        audio_buffer_destroy(&buffer);
        return EXIT_FAILURE;
    }

    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);

    printf("Producidos : %d\n", ctx.produced);
    printf("Consumidos : %d\n", ctx.consumed);

    int success =
        ctx.produced == TOTAL_BLOCKS &&
        ctx.consumed == TOTAL_BLOCKS;

    audio_buffer_destroy(&buffer);

    if (!success) {
        fprintf(stderr, "\n[ERROR] Test fallido\n");
        return EXIT_FAILURE;
    }

    printf("\n[OK] Buffer productor-consumidor correcto\n");

    return EXIT_SUCCESS;
}