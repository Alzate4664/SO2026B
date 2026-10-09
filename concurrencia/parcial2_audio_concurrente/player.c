#include "player.h"

#include <stdio.h>

int player_init(Player *player)
{
    if (player == NULL) {
        return -1;
    }

    player->state = PLAYER_STOPPED;
    player->pending_command = PLAYER_CMD_NONE;
    player->generation = 0;
    player->threads_started = 0;

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

    return 0;
}

int player_next(Player *player)
{
    if (player == NULL) {
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

    audio_buffer_destroy(&player->buffer);
    playlist_destroy(&player->playlist);

    pthread_cond_destroy(&player->state_changed);
    pthread_mutex_destroy(&player->state_mutex);
}