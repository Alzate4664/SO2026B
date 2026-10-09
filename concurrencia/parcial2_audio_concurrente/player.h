#ifndef PLAYER_H
#define PLAYER_H

#include <pthread.h>

#include "audio_buffer.h"
#include "playlist.h"

typedef enum {
    PLAYER_STOPPED,
    PLAYER_PLAYING,
    PLAYER_PAUSED,
    PLAYER_SHUTDOWN
} PlayerState;

typedef enum {
    PLAYER_CMD_NONE,
    PLAYER_CMD_NEXT,
    PLAYER_CMD_PREVIOUS
} PlayerCommand;

typedef struct {
    PlayerState state;
    PlayerCommand pending_command;

    /*
     * Cada vez que cambia de canción, stop, next o previous,
     * incrementamos generation.
     *
     * Sirve para detectar audio viejo que ya no pertenece
     * a la reproducción actual.
     */
    unsigned long generation;

    pthread_mutex_t state_mutex;
    pthread_cond_t state_changed;

    Playlist playlist;
    AudioBuffer buffer;

    pthread_t decoder_thread;
    pthread_t audio_thread;

    int threads_started;
} Player;

int player_init(Player *player);

int player_play(Player *player);
int player_pause(Player *player);
int player_stop(Player *player);
int player_next(Player *player);
int player_previous(Player *player);

PlayerState player_get_state(Player *player);
unsigned long player_get_generation(Player *player);

void player_shutdown(Player *player);
void player_destroy(Player *player);

#endif