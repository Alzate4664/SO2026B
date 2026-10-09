#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../player.h"

int main(void)
{
    Player player;

    printf("=== TEST PLAYER AUDIO CONCURRENTE ===\n\n");

    if (player_init(&player) != 0) {
        fprintf(stderr, "[ERROR] No se pudo inicializar Player\n");
        return EXIT_FAILURE;
    }

    if (
        playlist_add(
            &player.playlist,
            "../concurrencia_ludica/music/canon.mp3"
        ) != 0 ||
        playlist_add(
            &player.playlist,
            "../concurrencia_ludica/music/reggae.mp3"
        ) != 0
    ) {
        fprintf(stderr, "[ERROR] No se pudo llenar la playlist\n");

        player_destroy(&player);
        return EXIT_FAILURE;
    }

    printf("[OK] Playlist creada con 2 pistas\n");

    if (player_start_workers(&player) != 0) {
        fprintf(stderr, "[ERROR] No se pudieron iniciar los workers\n");

        player_destroy(&player);
        return EXIT_FAILURE;
    }

    printf("[OK] Decoder worker iniciado\n");
    printf("[OK] Audio worker iniciado\n\n");

    printf("1. PLAY -> debe sonar CANON durante 5 segundos\n");

    if (player_play(&player) != 0) {
        fprintf(stderr, "[ERROR] Fallo player_play()\n");

        player_shutdown(&player);
        player_join_workers(&player);
        player_destroy(&player);

        return EXIT_FAILURE;
    }

    sleep(5);

    printf("\n2. PAUSE -> debe quedar en silencio durante 3 segundos\n");

    if (player_pause(&player) != 0) {
        fprintf(stderr, "[ERROR] Fallo player_pause()\n");
    }

    sleep(3);

    printf("\n3. PLAY -> CANON debe continuar\n");

    if (player_play(&player) != 0) {
        fprintf(stderr, "[ERROR] Fallo al reanudar\n");
    }

    sleep(5);

    printf("\n4. NEXT -> debe cambiar inmediatamente a REGGAE\n");

    if (player_next(&player) != 0) {
        fprintf(stderr, "[ERROR] Fallo player_next()\n");
    }

    sleep(5);

    printf("\n5. PREVIOUS -> debe volver inmediatamente a CANON\n");

    if (player_previous(&player) != 0) {
        fprintf(stderr, "[ERROR] Fallo player_previous()\n");
    }

    sleep(5);

    printf("\n6. STOP -> el audio debe detenerse\n");

    if (player_stop(&player) != 0) {
        fprintf(stderr, "[ERROR] Fallo player_stop()\n");
    }

    sleep(3);

    printf("\n7. PLAY despues de STOP -> debe iniciar CANON desde el principio\n");

    if (player_play(&player) != 0) {
        fprintf(stderr, "[ERROR] Fallo player_play() despues de stop\n");
    }

    sleep(5);

    printf("\n8. SHUTDOWN\n");

    player_shutdown(&player);
    player_join_workers(&player);
    player_destroy(&player);

    printf("\n[OK] Player concurrente finalizado correctamente\n");

    return EXIT_SUCCESS;
}