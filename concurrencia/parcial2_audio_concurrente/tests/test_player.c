#include "../player.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define STRESS_ITERATIONS 5000

static const char *state_name(PlayerState state)
{
    switch (state) {
        case PLAYER_STOPPED:
            return "STOPPED";

        case PLAYER_PLAYING:
            return "PLAYING";

        case PLAYER_PAUSED:
            return "PAUSED";

        case PLAYER_SHUTDOWN:
            return "SHUTDOWN";

        default:
            return "UNKNOWN";
    }
}

static int expect_state(
    Player *player,
    PlayerState expected,
    const char *step)
{
    PlayerState actual = player_get_state(player);

    if (actual != expected) {
        fprintf(
            stderr,
            "[ERROR] %s: esperado %s, recibido %s\n",
            step,
            state_name(expected),
            state_name(actual)
        );

        return -1;
    }

    printf(
        "[OK] %-12s -> %s\n",
        step,
        state_name(actual)
    );

    return 0;
}

static void *state_reader_thread(void *arg)
{
    Player *player = (Player *)arg;

    for (int i = 0; i < STRESS_ITERATIONS; i++) {
        PlayerState state = player_get_state(player);

        if (
            state != PLAYER_STOPPED &&
            state != PLAYER_PLAYING &&
            state != PLAYER_PAUSED &&
            state != PLAYER_SHUTDOWN
        ) {
            fprintf(
                stderr,
                "[Reader] Estado inválido detectado: %d\n",
                state
            );

            return (void *)1;
        }

        (void)player_get_generation(player);
    }

    return NULL;
}

int main(void)
{
    Player player;

    printf("=== TEST MAQUINA DE ESTADOS ===\n\n");

    if (player_init(&player) != 0) {
        fprintf(stderr, "No se pudo inicializar Player\n");
        return EXIT_FAILURE;
    }

    if (
        playlist_add(&player.playlist, "track_1.mp3") != 0 ||
        playlist_add(&player.playlist, "track_2.mp3") != 0 ||
        playlist_add(&player.playlist, "track_3.mp3") != 0
    ) {
        fprintf(stderr, "No se pudieron agregar pistas de prueba\n");

        player_destroy(&player);
        return EXIT_FAILURE;
    }

    int failed = 0;

    /*
     * Estado inicial
     */
    if (
        expect_state(
            &player,
            PLAYER_STOPPED,
            "Inicial"
        ) != 0
    ) {
        failed = 1;
    }

    unsigned long generation = player_get_generation(&player);

    if (generation != 0) {
        fprintf(
            stderr,
            "[ERROR] Generación inicial esperada 0, recibida %lu\n",
            generation
        );

        failed = 1;
    } else {
        printf("[OK] Generacion inicial -> 0\n");
    }

    /*
     * STOPPED -> PLAYING
     */
    if (player_play(&player) != 0) {
        fprintf(stderr, "[ERROR] play fallo\n");
        failed = 1;
    }

    if (
        expect_state(
            &player,
            PLAYER_PLAYING,
            "Play"
        ) != 0
    ) {
        failed = 1;
    }

    /*
     * PLAYING -> PAUSED
     */
    if (player_pause(&player) != 0) {
        fprintf(stderr, "[ERROR] pause fallo\n");
        failed = 1;
    }

    if (
        expect_state(
            &player,
            PLAYER_PAUSED,
            "Pause"
        ) != 0
    ) {
        failed = 1;
    }

    /*
     * PAUSED -> PLAYING
     */
    if (player_play(&player) != 0) {
        fprintf(stderr, "[ERROR] resume fallo\n");
        failed = 1;
    }

    if (
        expect_state(
            &player,
            PLAYER_PLAYING,
            "Resume"
        ) != 0
    ) {
        failed = 1;
    }

    /*
     * NEXT debe aumentar generation.
     */
    generation = player_get_generation(&player);

    if (player_next(&player) != 0) {
        fprintf(stderr, "[ERROR] next fallo\n");
        failed = 1;
    }

    if (player_get_generation(&player) != generation + 1) {
        fprintf(stderr, "[ERROR] Next no incremento generation\n");
        failed = 1;
    } else {
        printf(
            "[OK] Next         -> generation %lu\n",
            player_get_generation(&player)
        );
    }

    /*
     * PREVIOUS también debe aumentar generation.
     */
    generation = player_get_generation(&player);

    if (player_previous(&player) != 0) {
        fprintf(stderr, "[ERROR] previous fallo\n");
        failed = 1;
    }

    if (player_get_generation(&player) != generation + 1) {
        fprintf(
            stderr,
            "[ERROR] Previous no incremento generation\n"
        );

        failed = 1;
    } else {
        printf(
            "[OK] Previous     -> generation %lu\n",
            player_get_generation(&player)
        );
    }

    /*
     * STOP cambia estado y genera una nueva generación.
     */
    generation = player_get_generation(&player);

    if (player_stop(&player) != 0) {
        fprintf(stderr, "[ERROR] stop fallo\n");
        failed = 1;
    }

    if (
        expect_state(
            &player,
            PLAYER_STOPPED,
            "Stop"
        ) != 0
    ) {
        failed = 1;
    }

    if (player_get_generation(&player) != generation + 1) {
        fprintf(stderr, "[ERROR] Stop no incremento generation\n");
        failed = 1;
    } else {
        printf(
            "[OK] Stop generation -> %lu\n",
            player_get_generation(&player)
        );
    }

    /*
     * Pause estando detenido debe fallar.
     */
    if (player_pause(&player) == 0) {
        fprintf(
            stderr,
            "[ERROR] Pause fue aceptado estando STOPPED\n"
        );

        failed = 1;
    } else {
        printf("[OK] Pause en STOPPED rechazado\n");
    }

    /*
     * Lecturas concurrentes del estado.
     */
    pthread_t reader1;
    pthread_t reader2;

    if (
        pthread_create(
            &reader1,
            NULL,
            state_reader_thread,
            &player
        ) != 0 ||
        pthread_create(
            &reader2,
            NULL,
            state_reader_thread,
            &player
        ) != 0
    ) {
        fprintf(stderr, "[ERROR] No se pudieron crear readers\n");
        failed = 1;
    } else {
        void *result1 = NULL;
        void *result2 = NULL;

        pthread_join(reader1, &result1);
        pthread_join(reader2, &result2);

        if (result1 != NULL || result2 != NULL) {
            fprintf(stderr, "[ERROR] Lectura concurrente fallo\n");
            failed = 1;
        } else {
            printf("[OK] Lecturas concurrentes de estado\n");
        }
    }

    /*
     * Cierre final.
     */
    player_shutdown(&player);

    if (
        expect_state(
            &player,
            PLAYER_SHUTDOWN,
            "Shutdown"
        ) != 0
    ) {
        failed = 1;
    }

    /*
     * Después de shutdown no se permite volver a play.
     */
    if (player_play(&player) == 0) {
        fprintf(
            stderr,
            "[ERROR] Play fue aceptado después de SHUTDOWN\n"
        );

        failed = 1;
    } else {
        printf("[OK] Play despues de SHUTDOWN rechazado\n");
    }

    player_destroy(&player);

    if (failed) {
        fprintf(stderr, "\n[ERROR] Test Player fallido\n");
        return EXIT_FAILURE;
    }

    printf("\n[OK] Maquina de estados correcta\n");

    return EXIT_SUCCESS;
}