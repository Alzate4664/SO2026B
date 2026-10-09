#include "player.h"
#include <stdio.h>
#include "audio_output.h"
#include "decoder.h"

static int decoder_should_continue(
    void *context,
    unsigned long generation)
{
    Player *player = (Player *)context;

    pthread_mutex_lock(&player->state_mutex);

    int should_continue =
        player->state != PLAYER_SHUTDOWN &&
        player->state != PLAYER_STOPPED &&
        player->generation == generation;

    pthread_mutex_unlock(&player->state_mutex);

    return should_continue;
}

static void *decoder_worker(void *arg)
{
    Player *player = (Player *)arg;

    while (1) {
        pthread_mutex_lock(&player->state_mutex);

        while (
            player->state != PLAYER_PLAYING &&
            player->state != PLAYER_SHUTDOWN
        ) {
            pthread_cond_wait(
                &player->state_changed,
                &player->state_mutex
            );
        }

        if (player->state == PLAYER_SHUTDOWN) {
            pthread_mutex_unlock(&player->state_mutex);
            break;
        }

        unsigned long generation =
            player->generation;

        player->pending_command =
            PLAYER_CMD_NONE;

        pthread_mutex_unlock(&player->state_mutex);

        Track track;
        size_t track_index = 0;

        if (
            playlist_get_current(
                &player->playlist,
                &track,
                &track_index
            ) != 0
        ) {
            pthread_mutex_lock(&player->state_mutex);

            if (player->state != PLAYER_SHUTDOWN) {
                player->state = PLAYER_STOPPED;
            }

            pthread_cond_broadcast(
                &player->state_changed
            );

            pthread_mutex_unlock(&player->state_mutex);

            continue;
        }

        AudioFormat format;

        if (
            decoder_probe_file(
                track.path,
                &format
            ) != 0
        ) {
            fprintf(
                stderr,
                "[decoder] No se pudo abrir: %s\n",
                track.path
            );

            if (
                playlist_next(
                    &player->playlist
                ) != 0
            ) {
                pthread_mutex_lock(
                    &player->state_mutex
                );

                player->state =
                    PLAYER_STOPPED;

                pthread_cond_broadcast(
                    &player->state_changed
                );

                pthread_mutex_unlock(
                    &player->state_mutex
                );

                continue;
            }

            pthread_mutex_lock(
                &player->state_mutex
            );

            player->generation++;

            pthread_cond_broadcast(
                &player->state_changed
            );

            pthread_mutex_unlock(
                &player->state_mutex
            );

            continue;
        }

        pthread_mutex_lock(&player->state_mutex);

        player->current_format = format;

        pthread_cond_broadcast(
            &player->state_changed
        );

        pthread_mutex_unlock(&player->state_mutex);

        printf(
            "\n[decoder] Pista %zu: %s\n",
            track_index + 1,
            track.path
        );

        size_t decoded_bytes = 0;
        size_t decoded_blocks = 0;

        int result =
            decoder_decode_file_tagged(
                track.path,
                &player->buffer,
                &format,
                &decoded_bytes,
                &decoded_blocks,
                generation,
                decoder_should_continue,
                player
            );

        /*
         * Cambio manual: next, previous, stop
         * o shutdown. No es un error.
         */
        if (result == 1) {
            continue;
        }

        if (result < 0) {
            fprintf(
                stderr,
                "[decoder] Error decodificando %s\n",
                track.path
            );

            continue;
        }

        /*
         * El decoder terminó, pero puede quedar PCM sin
         * reproducir dentro del buffer.
         *
         * Escribimos un marcador EOF de tamaño 0.
         */
        unsigned char eof_marker = 0;

        if (
            audio_buffer_write_tagged(
                &player->buffer,
                &eof_marker,
                0,
                generation
            ) != 0
        ) {
            continue;
        }

        /*
         * NO iniciamos de nuevo la misma canción.
         *
         * Esperamos hasta que:
         * - el consumidor procese EOF y cambie generation,
         * - llegue next/previous/stop,
         * - o llegue shutdown.
         */
        pthread_mutex_lock(
            &player->state_mutex
        );

        while (
            player->state == PLAYER_PLAYING &&
            player->generation == generation
        ) {
            pthread_cond_wait(
                &player->state_changed,
                &player->state_mutex
            );
        }

        pthread_mutex_unlock(
            &player->state_mutex
        );
    }

    return NULL;
}

static int audio_formats_equal(
    const AudioFormat *a,
    const AudioFormat *b)
{
    return
        a->rate == b->rate &&
        a->channels == b->channels &&
        a->encoding == b->encoding;
}

static void *audio_worker(void *arg)
{
    Player *player = (Player *)arg;

    unsigned char pcm[AUDIO_BLOCK_SIZE];

    AudioOutputDevice output;

    output.device = NULL;
    output.opened = 0;

    output.format.rate = 0;
    output.format.channels = 0;
    output.format.encoding = 0;

    audio_output_library_init();

    while (1) {
        pthread_mutex_lock(
            &player->state_mutex
        );

        /*
         * STOPPED y PAUSED no consumen PCM.
         */
        while (
            (
                player->state == PLAYER_STOPPED ||
                player->state == PLAYER_PAUSED
            ) &&
            player->state != PLAYER_SHUTDOWN
        ) {
            pthread_cond_wait(
                &player->state_changed,
                &player->state_mutex
            );
        }

        if (
            player->state ==
            PLAYER_SHUTDOWN
        ) {
            pthread_mutex_unlock(
                &player->state_mutex
            );

            break;
        }

        pthread_mutex_unlock(
            &player->state_mutex
        );

        size_t size = 0;
        unsigned long block_generation = 0;

        if (
            audio_buffer_read_tagged(
                &player->buffer,
                pcm,
                &size,
                &block_generation
            ) != 0
        ) {
            if (
                player_get_state(player) ==
                PLAYER_SHUTDOWN
            ) {
                break;
            }

            continue;
        }

        pthread_mutex_lock(
            &player->state_mutex
        );

        /*
         * Si pause ocurrió después de extraer el bloque,
         * conservamos ese bloque y esperamos.
         */
        while (
            player->state == PLAYER_PAUSED
        ) {
            pthread_cond_wait(
                &player->state_changed,
                &player->state_mutex
            );
        }

        PlayerState state =
            player->state;

        unsigned long current_generation =
            player->generation;

        AudioFormat current_format =
            player->current_format;

        pthread_mutex_unlock(
            &player->state_mutex
        );

        /*
         * Bloque viejo perteneciente a una canción
         * cancelada mediante next/previous/stop.
         */
        if (
            state != PLAYER_PLAYING ||
            block_generation !=
                current_generation
        ) {
            continue;
        }

        /*
         * size == 0 es nuestro marcador de fin de pista.
         *
         * Como el buffer es FIFO, si llegamos aquí significa
         * que todos los bloques PCM anteriores ya fueron
         * enviados a libao.
         */
        if (size == 0) {
            if (
                playlist_next(
                    &player->playlist
                ) == 0
            ) {
                pthread_mutex_lock(
                    &player->state_mutex
                );

                /*
                 * Verificamos otra vez para no pisar un
                 * next/stop concurrente de la CLI.
                 */
                if (
                    player->state ==
                        PLAYER_PLAYING &&
                    player->generation ==
                        block_generation
                ) {
                    player->generation++;

                    pthread_cond_broadcast(
                        &player->state_changed
                    );
                }

                pthread_mutex_unlock(
                    &player->state_mutex
                );
            } else {
                pthread_mutex_lock(
                    &player->state_mutex
                );

                if (
                    player->generation ==
                    block_generation
                ) {
                    player->state =
                        PLAYER_STOPPED;

                    pthread_cond_broadcast(
                        &player->state_changed
                    );
                }

                pthread_mutex_unlock(
                    &player->state_mutex
                );
            }

            continue;
        }

        /*
         * Abrimos el dispositivo la primera vez.
         */
        if (!output.opened) {
            if (
                audio_output_open(
                    &output,
                    &current_format
                ) != 0
            ) {
                fprintf(
                    stderr,
                    "[audio] No se pudo abrir salida\n"
                );

                player_stop(player);
                continue;
            }
        }

        /*
         * Si cambia el formato entre canciones,
         * reconfiguramos libao.
         */
        if (
            !audio_formats_equal(
                &output.format,
                &current_format
            )
        ) {
            audio_output_close(
                &output
            );

            if (
                audio_output_open(
                    &output,
                    &current_format
                ) != 0
            ) {
                fprintf(
                    stderr,
                    "[audio] No se pudo cambiar formato\n"
                );

                player_stop(player);
                continue;
            }
        }

        if (
            audio_output_write(
                &output,
                pcm,
                size
            ) != 0
        ) {
            fprintf(
                stderr,
                "[audio] Error reproduciendo PCM\n"
            );

            player_stop(player);
        }
    }

    audio_output_close(&output);

    audio_output_library_cleanup();

    return NULL;
}

int player_init(Player *player)
{
    if (player == NULL) {
        return -1;
    }

    player->state = PLAYER_STOPPED;
    player->pending_command = PLAYER_CMD_NONE;
    player->generation = 0;
    player->threads_started = 0;
    player->decoder_running = 0;
    player->audio_running = 0;

    player->current_format.rate = 0;
    player->current_format.channels = 0;
    player->current_format.encoding = 0;

    if (pthread_mutex_init(&player->state_mutex, NULL) != 0) {
        return -1;
    }

    if (pthread_cond_init(&player->state_changed, NULL) != 0) {
        pthread_mutex_destroy(&player->state_mutex);
        return -1;
    }

    if (playlist_init(&player->playlist) != 0) {
        pthread_cond_destroy(&player->state_changed);
        pthread_mutex_destroy(&player->state_mutex);
        return -1;
    }

    if (audio_buffer_init(&player->buffer) != 0) {
        playlist_destroy(&player->playlist);
        pthread_cond_destroy(&player->state_changed);
        pthread_mutex_destroy(&player->state_mutex);
        return -1;
    }

    return 0;
}

int player_play(Player *player)
{
    if (player == NULL) {
        return -1;
    }

    pthread_mutex_lock(&player->state_mutex);

    if (player->state == PLAYER_SHUTDOWN) {
        pthread_mutex_unlock(&player->state_mutex);
        return -1;
    }

    player->state = PLAYER_PLAYING;

    pthread_cond_broadcast(&player->state_changed);

    pthread_mutex_unlock(&player->state_mutex);

    return 0;
}

int player_pause(Player *player)
{
    if (player == NULL) {
        return -1;
    }

    pthread_mutex_lock(&player->state_mutex);

    if (player->state != PLAYER_PLAYING) {
        pthread_mutex_unlock(&player->state_mutex);
        return -1;
    }

    player->state = PLAYER_PAUSED;

    pthread_cond_broadcast(&player->state_changed);

    pthread_mutex_unlock(&player->state_mutex);

    return 0;
}

int player_stop(Player *player)
{
    if (player == NULL) {
        return -1;
    }

    pthread_mutex_lock(&player->state_mutex);

    if (player->state == PLAYER_SHUTDOWN) {
        pthread_mutex_unlock(&player->state_mutex);
        return -1;
    }

    player->state = PLAYER_STOPPED;
    player->pending_command = PLAYER_CMD_NONE;
    player->generation++;

    pthread_cond_broadcast(&player->state_changed);

    pthread_mutex_unlock(&player->state_mutex);

    /*
     * Se limpia fuera de state_mutex para evitar mantener
     * dos locks innecesariamente.
     */
    audio_buffer_clear(&player->buffer);

    playlist_reset_current(&player->playlist);

    return 0;
}

int player_next(Player *player)
{
    if (player == NULL) {
        return -1;
    }

    if (playlist_next(&player->playlist) != 0) {
        return -1;
    }

    pthread_mutex_lock(&player->state_mutex);

    if (player->state == PLAYER_SHUTDOWN) {
        pthread_mutex_unlock(&player->state_mutex);
        return -1;
    }

    player->pending_command = PLAYER_CMD_NEXT;
    player->generation++;

    pthread_cond_broadcast(&player->state_changed);

    pthread_mutex_unlock(&player->state_mutex);

    audio_buffer_clear(&player->buffer);

    return 0;
}

int player_previous(Player *player)
{
    if (player == NULL) {
        return -1;
    }

    if (playlist_previous(&player->playlist) != 0) {
        return -1;
    }

    pthread_mutex_lock(&player->state_mutex);

    if (player->state == PLAYER_SHUTDOWN) {
        pthread_mutex_unlock(&player->state_mutex);
        return -1;
    }

    player->pending_command = PLAYER_CMD_PREVIOUS;
    player->generation++;

    pthread_cond_broadcast(&player->state_changed);

    pthread_mutex_unlock(&player->state_mutex);

    audio_buffer_clear(&player->buffer);

    return 0;
}

PlayerState player_get_state(Player *player)
{
    if (player == NULL) {
        return PLAYER_SHUTDOWN;
    }

    pthread_mutex_lock(&player->state_mutex);

    PlayerState state = player->state;

    pthread_mutex_unlock(&player->state_mutex);

    return state;
}

unsigned long player_get_generation(Player *player)
{
    if (player == NULL) {
        return 0;
    }

    pthread_mutex_lock(&player->state_mutex);

    unsigned long generation = player->generation;

    pthread_mutex_unlock(&player->state_mutex);

    return generation;
}

int player_start_workers(Player *player)
{
    if (player == NULL) {
        return -1;
    }

    if (player->threads_started) {
        return 0;
    }

    if (decoder_library_init() != 0) {
        return -1;
    }

    if (
        pthread_create(
            &player->decoder_thread,
            NULL,
            decoder_worker,
            player
        ) != 0
    ) {
        decoder_library_cleanup();
        return -1;
    }

    if (
        pthread_create(
            &player->audio_thread,
            NULL,
            audio_worker,
            player
        ) != 0
    ) {
        player_shutdown(player);

        pthread_join(
            player->decoder_thread,
            NULL
        );

        decoder_library_cleanup();

        return -1;
    }

    player->threads_started = 1;

    return 0;
}

void player_join_workers(Player *player)
{
    if (
        player == NULL ||
        !player->threads_started
    ) {
        return;
    }

    pthread_join(
        player->decoder_thread,
        NULL
    );

    pthread_join(
        player->audio_thread,
        NULL
    );

    player->threads_started = 0;

    decoder_library_cleanup();
}

void player_shutdown(Player *player)
{
    if (player == NULL) {
        return;
    }

    pthread_mutex_lock(&player->state_mutex);

    player->state = PLAYER_SHUTDOWN;
    player->generation++;

    pthread_cond_broadcast(&player->state_changed);

    pthread_mutex_unlock(&player->state_mutex);

    /*
     * También despertamos cualquier hilo bloqueado
     * dentro del buffer.
     */
    audio_buffer_shutdown(&player->buffer);
}

void player_destroy(Player *player)
{
    if (player == NULL) {
        return;
    }

    if (player->threads_started) {
        player_shutdown(player);
        player_join_workers(player);
    }

    audio_buffer_destroy(
        &player->buffer
    );

    playlist_destroy(
        &player->playlist
    );

    pthread_cond_destroy(
        &player->state_changed
    );

    pthread_mutex_destroy(
        &player->state_mutex
    );
}