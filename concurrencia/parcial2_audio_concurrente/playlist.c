#include "playlist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int playlist_resize(Playlist *playlist, size_t new_capacity)
{
    Track *new_tracks = realloc(
        playlist->tracks,
        new_capacity * sizeof(Track)
    );

    if (new_tracks == NULL) {
        return -1;
    }

    playlist->tracks = new_tracks;
    playlist->capacity = new_capacity;

    return 0;
}

int playlist_init(Playlist *playlist)
{
    if (playlist == NULL) {
        return -1;
    }

    playlist->tracks = malloc(
        PLAYLIST_INITIAL_CAPACITY * sizeof(Track)
    );

    if (playlist->tracks == NULL) {
        return -1;
    }

    playlist->count = 0;
    playlist->capacity = PLAYLIST_INITIAL_CAPACITY;
    playlist->current_index = 0;

    if (pthread_rwlock_init(&playlist->lock, NULL) != 0) {
        free(playlist->tracks);
        playlist->tracks = NULL;
        return -1;
    }

    return 0;
}

int playlist_add(Playlist *playlist, const char *path)
{
    if (playlist == NULL || path == NULL) {
        return -1;
    }

    pthread_rwlock_wrlock(&playlist->lock);

    if (playlist->count == playlist->capacity) {
        size_t new_capacity = playlist->capacity * 2;

        if (playlist_resize(playlist, new_capacity) != 0) {
            pthread_rwlock_unlock(&playlist->lock);
            return -1;
        }
    }

    strncpy(
        playlist->tracks[playlist->count].path,
        path,
        TRACK_PATH_MAX - 1
    );

    playlist->tracks[playlist->count]
        .path[TRACK_PATH_MAX - 1] = '\0';

    playlist->count++;

    pthread_rwlock_unlock(&playlist->lock);

    return 0;
}

int playlist_remove(Playlist *playlist, size_t index)
{
    if (playlist == NULL) {
        return -1;
    }

    pthread_rwlock_wrlock(&playlist->lock);

    if (index >= playlist->count) {
        pthread_rwlock_unlock(&playlist->lock);
        return -1;
    }

    for (size_t i = index; i + 1 < playlist->count; i++) {
        playlist->tracks[i] = playlist->tracks[i + 1];
    }

    playlist->count--;

    if (playlist->count == 0) {
        playlist->current_index = 0;
    } else if (playlist->current_index >= playlist->count) {
        playlist->current_index = playlist->count - 1;
    }

    pthread_rwlock_unlock(&playlist->lock);

    return 0;
}

int playlist_move(
    Playlist *playlist,
    size_t from,
    size_t to)
{
    if (playlist == NULL) {
        return -1;
    }

    pthread_rwlock_wrlock(&playlist->lock);

    if (
        from >= playlist->count ||
        to >= playlist->count
    ) {
        pthread_rwlock_unlock(&playlist->lock);
        return -1;
    }

    if (from == to) {
        pthread_rwlock_unlock(&playlist->lock);
        return 0;
    }

    Track temp = playlist->tracks[from];

    if (from < to) {
        for (size_t i = from; i < to; i++) {
            playlist->tracks[i] = playlist->tracks[i + 1];
        }
    } else {
        for (size_t i = from; i > to; i--) {
            playlist->tracks[i] = playlist->tracks[i - 1];
        }
    }

    playlist->tracks[to] = temp;

    pthread_rwlock_unlock(&playlist->lock);

    return 0;
}

int playlist_get(
    Playlist *playlist,
    size_t index,
    Track *destination)
{
    if (playlist == NULL || destination == NULL) {
        return -1;
    }

    pthread_rwlock_rdlock(&playlist->lock);

    if (index >= playlist->count) {
        pthread_rwlock_unlock(&playlist->lock);
        return -1;
    }

    /*
     * Copiamos la estructura completa.
     *
     * Así el llamador no conserva un puntero a memoria
     * que después pueda cambiar por remove(), move()
     * o realloc().
     */
    *destination = playlist->tracks[index];

    pthread_rwlock_unlock(&playlist->lock);

    return 0;
}

size_t playlist_count(Playlist *playlist)
{
    if (playlist == NULL) {
        return 0;
    }

    pthread_rwlock_rdlock(&playlist->lock);

    size_t count = playlist->count;

    pthread_rwlock_unlock(&playlist->lock);

    return count;
}

void playlist_print(Playlist *playlist)
{
    if (playlist == NULL) {
        return;
    }

    pthread_rwlock_rdlock(&playlist->lock);

    printf("\n=== PLAYLIST ===\n");

    if (playlist->count == 0) {
        printf("(vacía)\n");
    } else {
        for (size_t i = 0; i < playlist->count; i++) {
            printf(
                "%zu. %s\n",
                i + 1,
                playlist->tracks[i].path
            );
        }
    }

    printf("================\n\n");

    pthread_rwlock_unlock(&playlist->lock);
}

void playlist_clear(Playlist *playlist)
{
    if (playlist == NULL) {
        return;
    }

    pthread_rwlock_wrlock(&playlist->lock);

    playlist->count = 0;
    playlist->current_index = 0;

    pthread_rwlock_unlock(&playlist->lock);
}

int playlist_get_current(
    Playlist *playlist,
    Track *destination,
    size_t *index)
{
    if (
        playlist == NULL ||
        destination == NULL
    ) {
        return -1;
    }

    pthread_rwlock_rdlock(&playlist->lock);

    if (playlist->count == 0) {
        pthread_rwlock_unlock(&playlist->lock);
        return -1;
    }

    if (playlist->current_index >= playlist->count) {
        pthread_rwlock_unlock(&playlist->lock);
        return -1;
    }

    *destination =
        playlist->tracks[playlist->current_index];

    if (index != NULL) {
        *index = playlist->current_index;
    }

    pthread_rwlock_unlock(&playlist->lock);

    return 0;
}

int playlist_next(Playlist *playlist)
{
    if (playlist == NULL) {
        return -1;
    }

    pthread_rwlock_wrlock(&playlist->lock);

    if (playlist->count == 0) {
        pthread_rwlock_unlock(&playlist->lock);
        return -1;
    }

    playlist->current_index =
        (playlist->current_index + 1) %
        playlist->count;

    pthread_rwlock_unlock(&playlist->lock);

    return 0;
}

int playlist_previous(Playlist *playlist)
{
    if (playlist == NULL) {
        return -1;
    }

    pthread_rwlock_wrlock(&playlist->lock);

    if (playlist->count == 0) {
        pthread_rwlock_unlock(&playlist->lock);
        return -1;
    }

    if (playlist->current_index == 0) {
        playlist->current_index =
            playlist->count - 1;
    } else {
        playlist->current_index--;
    }

    pthread_rwlock_unlock(&playlist->lock);

    return 0;
}

void playlist_reset_current(Playlist *playlist)
{
    if (playlist == NULL) {
        return;
    }

    pthread_rwlock_wrlock(&playlist->lock);

    playlist->current_index = 0;

    pthread_rwlock_unlock(&playlist->lock);
}

void playlist_destroy(Playlist *playlist)
{
    if (playlist == NULL) {
        return;
    }

    pthread_rwlock_wrlock(&playlist->lock);

    free(playlist->tracks);

    playlist->tracks = NULL;
    playlist->count = 0;
    playlist->capacity = 0;
    playlist->current_index = 0;

    pthread_rwlock_unlock(&playlist->lock);

    pthread_rwlock_destroy(&playlist->lock);
}