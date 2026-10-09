#include "../playlist.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define READER_THREADS 4
#define READER_ITERATIONS 5000
#define WRITER_ITERATIONS 1000

typedef struct {
    Playlist *playlist;
    int id;
} ReaderArgs;

typedef struct {
    Playlist *playlist;
} WriterArgs;

void *reader_thread(void *arg)
{
    ReaderArgs *args = (ReaderArgs *)arg;

    for (int i = 0; i < READER_ITERATIONS; i++) {
        size_t count = playlist_count(args->playlist);

        if (count > 0) {
            size_t index = (size_t)(i % (int)count);

            Track track;

            if (playlist_get(args->playlist, index, &track) == 0) {
                if (track.path[0] == '\0') {
                    fprintf(
                        stderr,
                        "[Reader %d] ERROR: pista vacía en índice %zu\n",
                        args->id,
                        index
                    );

                    return (void *)1;
                }
            }
        }

        if (i % 100 == 0) {
            usleep(100);
        }
    }

    return NULL;
}

void *writer_thread(void *arg)
{
    WriterArgs *args = (WriterArgs *)arg;

    char path[128];

    for (int i = 0; i < WRITER_ITERATIONS; i++) {
        snprintf(
            path,
            sizeof(path),
            "track_%d.mp3",
            i
        );

        if (playlist_add(args->playlist, path) != 0) {
            fprintf(stderr, "[Writer] ERROR agregando pista\n");
            return (void *)1;
        }

        size_t count = playlist_count(args->playlist);

        if (count > 3 && i % 3 == 0) {
            playlist_remove(args->playlist, 1);
        }

        count = playlist_count(args->playlist);

        if (count > 2 && i % 5 == 0) {
            playlist_move(
                args->playlist,
                0,
                count - 1
            );
        }

        if (i % 100 == 0) {
            usleep(200);
        }
    }

    return NULL;
}

int main(void)
{
    Playlist playlist;

    if (playlist_init(&playlist) != 0) {
        fprintf(stderr, "No se pudo inicializar la playlist\n");
        return EXIT_FAILURE;
    }

    /*
     * Algunas pistas iniciales para que los lectores tengan
     * datos desde el comienzo.
     */
    playlist_add(&playlist, "canon.mp3");
    playlist_add(&playlist, "reggae.mp3");
    playlist_add(&playlist, "toro.mp3");

    pthread_t readers[READER_THREADS];
    pthread_t writer;

    ReaderArgs reader_args[READER_THREADS];

    WriterArgs writer_args = {
        .playlist = &playlist
    };

    printf("=== TEST PLAYLIST CONCURRENTE ===\n\n");

    for (int i = 0; i < READER_THREADS; i++) {
        reader_args[i].playlist = &playlist;
        reader_args[i].id = i;

        if (pthread_create(
                &readers[i],
                NULL,
                reader_thread,
                &reader_args[i]
            ) != 0) {
            fprintf(stderr, "Error creando reader %d\n", i);

            playlist_destroy(&playlist);
            return EXIT_FAILURE;
        }
    }

    if (pthread_create(
            &writer,
            NULL,
            writer_thread,
            &writer_args
        ) != 0) {
        fprintf(stderr, "Error creando writer\n");

        for (int i = 0; i < READER_THREADS; i++) {
            pthread_join(readers[i], NULL);
        }

        playlist_destroy(&playlist);
        return EXIT_FAILURE;
    }

    int failed = 0;

    for (int i = 0; i < READER_THREADS; i++) {
        void *result = NULL;

        pthread_join(readers[i], &result);

        if (result != NULL) {
            failed = 1;
        }
    }

    void *writer_result = NULL;

    pthread_join(writer, &writer_result);

    if (writer_result != NULL) {
        failed = 1;
    }

    size_t final_count = playlist_count(&playlist);

    printf("Lectores       : %d\n", READER_THREADS);
    printf("Iteraciones R  : %d por hilo\n", READER_ITERATIONS);
    printf("Iteraciones W  : %d\n", WRITER_ITERATIONS);
    printf("Pistas finales : %zu\n", final_count);

    playlist_destroy(&playlist);

    if (failed) {
        fprintf(stderr, "\n[ERROR] Test concurrente fallido\n");
        return EXIT_FAILURE;
    }

    printf("\n[OK] Playlist concurrente correcta\n");

    return EXIT_SUCCESS;
}