#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <pthread.h>
#include <stddef.h>

#define PLAYLIST_INITIAL_CAPACITY 8
#define TRACK_PATH_MAX 1024

typedef struct {
    char path[TRACK_PATH_MAX];
} Track;

typedef struct {
    Track *tracks;

    size_t count;
    size_t capacity;
    size_t current_index;

    pthread_rwlock_t lock;
} Playlist;

/* Inicializa la playlist. */
int playlist_init(Playlist *playlist);

/* Agrega una nueva pista al final. */
int playlist_add(Playlist *playlist, const char *path);

/* Elimina una pista por índice. */
int playlist_remove(Playlist *playlist, size_t index);

/* Mueve una pista de una posición a otra. */
int playlist_move(
    Playlist *playlist,
    size_t from,
    size_t to
);

/* Obtiene una COPIA segura de una pista. */
int playlist_get(
    Playlist *playlist,
    size_t index,
    Track *destination
);

/* Devuelve la cantidad actual de pistas. */
size_t playlist_count(Playlist *playlist);

/* Muestra la playlist. */
void playlist_print(Playlist *playlist);

/* Elimina todas las pistas. */
void playlist_clear(Playlist *playlist);

/*
 * Obtiene una copia de la pista actualmente seleccionada.
 */
int playlist_get_current(
    Playlist *playlist,
    Track *destination,
    size_t *index
);

/*
 * Avanza a la siguiente pista.
 * Retorna 0 si pudo avanzar.
 * Retorna -1 si está vacía.
 *
 * Si ya está en la última pista vuelve a la primera.
 */
int playlist_next(Playlist *playlist);

/*
 * Retrocede a la pista anterior.
 * Si está en la primera, vuelve a la última.
 */
int playlist_previous(Playlist *playlist);

/*
 * Reinicia la posición a la primera pista.
 */
void playlist_reset_current(Playlist *playlist);

/* Libera recursos. */
void playlist_destroy(Playlist *playlist);

#endif