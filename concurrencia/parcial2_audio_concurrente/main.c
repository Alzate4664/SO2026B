#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "player.h"

#define COMMAND_SIZE 2048

static const char *player_state_name(PlayerState state)
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

static char *skip_spaces(char *text)
{
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    return text;
}

static void remove_newline(char *text)
{
    size_t length = strlen(text);

    while (
        length > 0 &&
        (
            text[length - 1] == '\n' ||
            text[length - 1] == '\r'
        )
    ) {
        text[length - 1] = '\0';
        length--;
    }
}

static void print_help(void)
{
    printf(
        "\n"
        "================ COMANDOS ================\n"
        " help                  Mostrar ayuda\n"
        " playlist              Mostrar playlist\n"
        " add <archivo>          Agregar pista MP3\n"
        " remove <indice>        Eliminar pista\n"
        " move <origen> <dest>   Reordenar pista\n"
        " clear                  Vaciar playlist\n"
        "\n"
        " play                   Reproducir / continuar\n"
        " pause                  Pausar\n"
        " stop                   Detener\n"
        " next                   Siguiente pista\n"
        " prev                   Pista anterior\n"
        " status                 Estado del reproductor\n"
        "\n"
        " quit                   Salir\n"
        "==========================================\n\n"
    );
}

static void print_status(Player *player)
{
    PlayerState state =
        player_get_state(player);

    unsigned long generation =
        player_get_generation(player);

    size_t count =
        playlist_count(&player->playlist);

    Track current;
    size_t current_index = 0;

    printf("\n=== ESTADO ===\n");
    printf(
        "Player     : %s\n",
        player_state_name(state)
    );

    printf(
        "Generation : %lu\n",
        generation
    );

    printf(
        "Pistas     : %zu\n",
        count
    );

    if (
        playlist_get_current(
            &player->playlist,
            &current,
            &current_index
        ) == 0
    ) {
        printf(
            "Actual     : %zu - %s\n",
            current_index + 1,
            current.path
        );
    } else {
        printf(
            "Actual     : ninguna\n"
        );
    }

    printf("==============\n\n");
}

static int parse_index(
    const char *text,
    size_t *index)
{
    if (
        text == NULL ||
        index == NULL
    ) {
        return -1;
    }

    char *end = NULL;

    unsigned long value =
        strtoul(text, &end, 10);

    if (
        end == text ||
        *end != '\0' ||
        value == 0
    ) {
        return -1;
    }

    /*
     * La interfaz muestra posiciones desde 1,
     * pero internamente usamos índices desde 0.
     */
    *index = (size_t)(value - 1);

    return 0;
}

static void handle_add(
    Player *player,
    char *argument)
{
    argument = skip_spaces(argument);

    if (*argument == '\0') {
        printf(
            "[ERROR] Uso: add <archivo>\n"
        );

        return;
    }

    if (
        playlist_add(
            &player->playlist,
            argument
        ) != 0
    ) {
        printf(
            "[ERROR] No se pudo agregar la pista\n"
        );

        return;
    }

    printf(
        "[OK] Pista agregada: %s\n",
        argument
    );
}

static void handle_remove(
    Player *player,
    char *argument)
{
    argument = skip_spaces(argument);

    size_t index = 0;

    if (
        parse_index(
            argument,
            &index
        ) != 0
    ) {
        printf(
            "[ERROR] Uso: remove <indice>\n"
        );

        return;
    }

    if (
        playlist_remove(
            &player->playlist,
            index
        ) != 0
    ) {
        printf(
            "[ERROR] Indice inexistente\n"
        );

        return;
    }

    printf(
        "[OK] Pista %zu eliminada\n",
        index + 1
    );
}

static void handle_move(
    Player *player,
    char *argument)
{
    argument = skip_spaces(argument);

    char *first =
        strtok(argument, " \t");

    char *second =
        strtok(NULL, " \t");

    char *extra =
        strtok(NULL, " \t");

    if (
        first == NULL ||
        second == NULL ||
        extra != NULL
    ) {
        printf(
            "[ERROR] Uso: move <origen> <destino>\n"
        );

        return;
    }

    size_t from = 0;
    size_t to = 0;

    if (
        parse_index(first, &from) != 0 ||
        parse_index(second, &to) != 0
    ) {
        printf(
            "[ERROR] Los indices deben comenzar en 1\n"
        );

        return;
    }

    if (
        playlist_move(
            &player->playlist,
            from,
            to
        ) != 0
    ) {
        printf(
            "[ERROR] No se pudo mover la pista\n"
        );

        return;
    }

    printf(
        "[OK] Pista movida de %zu a %zu\n",
        from + 1,
        to + 1
    );
}

int main(void)
{
    Player player;

    if (player_init(&player) != 0) {
        fprintf(
            stderr,
            "[ERROR] No se pudo inicializar el reproductor\n"
        );

        return EXIT_FAILURE;
    }

    if (
        player_start_workers(
            &player
        ) != 0
    ) {
        fprintf(
            stderr,
            "[ERROR] No se pudieron iniciar los hilos\n"
        );

        player_destroy(&player);

        return EXIT_FAILURE;
    }

    printf(
        "\n"
        "==========================================\n"
        "       REPRODUCTOR MP3 CONCURRENTE\n"
        "==========================================\n"
        "POSIX Threads + Mutex + Cond + RWLock\n"
        "MPG123 + libao\n"
        "==========================================\n"
    );

    print_help();

    char line[COMMAND_SIZE];

    while (1) {
        printf("player> ");
        fflush(stdout);

        if (
            fgets(
                line,
                sizeof(line),
                stdin
            ) == NULL
        ) {
            printf("\n");

            break;
        }

        remove_newline(line);

        char *input =
            skip_spaces(line);

        if (*input == '\0') {
            continue;
        }

        char *space =
            strpbrk(input, " \t");

        char *argument = NULL;

        if (space != NULL) {
            *space = '\0';

            argument =
                skip_spaces(space + 1);
        }

        if (
            strcmp(input, "help") == 0
        ) {
            print_help();
        } else if (
            strcmp(input, "playlist") == 0
        ) {
            playlist_print(
                &player.playlist
            );
        } else if (
            strcmp(input, "add") == 0
        ) {
            if (argument == NULL) {
                printf(
                    "[ERROR] Uso: add <archivo>\n"
                );
            } else {
                handle_add(
                    &player,
                    argument
                );
            }
        } else if (
            strcmp(input, "remove") == 0
        ) {
            if (argument == NULL) {
                printf(
                    "[ERROR] Uso: remove <indice>\n"
                );
            } else {
                handle_remove(
                    &player,
                    argument
                );
            }
        } else if (
            strcmp(input, "move") == 0
        ) {
            if (argument == NULL) {
                printf(
                    "[ERROR] Uso: move <origen> <destino>\n"
                );
            } else {
                handle_move(
                    &player,
                    argument
                );
            }
        } else if (
            strcmp(input, "clear") == 0
        ) {
            player_stop(&player);
            playlist_clear(
                &player.playlist
            );

            printf(
                "[OK] Playlist vaciada\n"
            );
        } else if (
            strcmp(input, "play") == 0
        ) {
            if (
                playlist_count(
                    &player.playlist
                ) == 0
            ) {
                printf(
                    "[ERROR] La playlist esta vacia\n"
                );
            } else if (
                player_play(
                    &player
                ) != 0
            ) {
                printf(
                    "[ERROR] No se pudo iniciar la reproduccion\n"
                );
            } else {
                printf(
                    "[OK] PLAY\n"
                );
            }
        } else if (
            strcmp(input, "pause") == 0
        ) {
            if (
                player_pause(
                    &player
                ) != 0
            ) {
                printf(
                    "[ERROR] Solo se puede pausar mientras reproduce\n"
                );
            } else {
                printf(
                    "[OK] PAUSE\n"
                );
            }
        } else if (
            strcmp(input, "stop") == 0
        ) {
            if (
                player_stop(
                    &player
                ) != 0
            ) {
                printf(
                    "[ERROR] No se pudo detener\n"
                );
            } else {
                printf(
                    "[OK] STOP\n"
                );
            }
        } else if (
            strcmp(input, "next") == 0
        ) {
            if (
                player_next(
                    &player
                ) != 0
            ) {
                printf(
                    "[ERROR] No hay pistas disponibles\n"
                );
            } else {
                printf(
                    "[OK] NEXT\n"
                );
            }
        } else if (
            strcmp(input, "prev") == 0 ||
            strcmp(input, "previous") == 0
        ) {
            if (
                player_previous(
                    &player
                ) != 0
            ) {
                printf(
                    "[ERROR] No hay pistas disponibles\n"
                );
            } else {
                printf(
                    "[OK] PREVIOUS\n"
                );
            }
        } else if (
            strcmp(input, "status") == 0
        ) {
            print_status(
                &player
            );
        } else if (
            strcmp(input, "quit") == 0 ||
            strcmp(input, "exit") == 0
        ) {
            break;
        } else {
            printf(
                "[ERROR] Comando desconocido: %s\n"
                "        Escriba 'help' para ver los comandos.\n",
                input
            );
        }
    }

    printf(
        "\nCerrando reproductor...\n"
    );

    player_shutdown(&player);
    player_join_workers(&player);
    player_destroy(&player);

    printf(
        "[OK] Reproductor finalizado\n"
    );

    return EXIT_SUCCESS;
}