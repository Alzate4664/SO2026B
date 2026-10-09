#include "decoder.h"

#include <mpg123.h>

#include <stdio.h>
#include <stdlib.h>

int decoder_library_init(void)
{
    if (mpg123_init() != MPG123_OK) {
        fprintf(stderr, "Error inicializando mpg123\n");
        return -1;
    }

    return 0;
}

void decoder_library_cleanup(void)
{
    mpg123_exit();
}

int decoder_probe_file(
    const char *path,
    AudioFormat *format)
{
    if (path == NULL || format == NULL) {
        return -1;
    }

    int error = MPG123_OK;

    mpg123_handle *handle = mpg123_new(NULL, &error);

    if (handle == NULL) {
        fprintf(
            stderr,
            "No se pudo crear decoder mpg123: %s\n",
            mpg123_plain_strerror(error)
        );

        return -1;
    }

    if (mpg123_format_none(handle) != MPG123_OK) {
        mpg123_delete(handle);
        return -1;
    }

    const long *rates = NULL;
    size_t rate_count = 0;

    mpg123_rates(&rates, &rate_count);

    for (size_t i = 0; i < rate_count; i++) {
        if (
            mpg123_format(
                handle,
                rates[i],
                MPG123_MONO | MPG123_STEREO,
                MPG123_ENC_SIGNED_16
            ) != MPG123_OK
        ) {
            mpg123_delete(handle);
            return -1;
        }
    }

    if (mpg123_open(handle, path) != MPG123_OK) {
        fprintf(
            stderr,
            "No se pudo abrir '%s': %s\n",
            path,
            mpg123_strerror(handle)
        );

        mpg123_delete(handle);
        return -1;
    }

    long rate = 0;
    int channels = 0;
    int encoding = 0;

    if (
        mpg123_getformat(
            handle,
            &rate,
            &channels,
            &encoding
        ) != MPG123_OK
    ) {
        fprintf(stderr, "No se pudo consultar formato de '%s'\n", path);

        mpg123_close(handle);
        mpg123_delete(handle);

        return -1;
    }

    format->rate = rate;
    format->channels = channels;
    format->encoding = encoding;

    mpg123_close(handle);
    mpg123_delete(handle);

    return 0;
}

int decoder_decode_file_tagged(
    const char *path,
    AudioBuffer *buffer,
    AudioFormat *format,
    size_t *total_bytes,
    size_t *total_blocks,
    unsigned long generation,
    DecoderContinueCallback should_continue,
    void *callback_context)
{
    if (
        path == NULL ||
        buffer == NULL ||
        format == NULL ||
        total_bytes == NULL ||
        total_blocks == NULL
    ) {
        return -1;
    }

    *total_bytes = 0;
    *total_blocks = 0;

    int error = MPG123_OK;

    mpg123_handle *handle = mpg123_new(NULL, &error);

    if (handle == NULL) {
        fprintf(
            stderr,
            "No se pudo crear decoder mpg123: %s\n",
            mpg123_plain_strerror(error)
        );

        return -1;
    }

    if (mpg123_format_none(handle) != MPG123_OK) {
        mpg123_delete(handle);
        return -1;
    }

    const long *rates = NULL;
    size_t rate_count = 0;

    mpg123_rates(&rates, &rate_count);

    for (size_t i = 0; i < rate_count; i++) {
        if (
            mpg123_format(
                handle,
                rates[i],
                MPG123_MONO | MPG123_STEREO,
                MPG123_ENC_SIGNED_16
            ) != MPG123_OK
        ) {
            mpg123_delete(handle);
            return -1;
        }
    }

    if (mpg123_open(handle, path) != MPG123_OK) {
        fprintf(
            stderr,
            "No se pudo abrir '%s': %s\n",
            path,
            mpg123_strerror(handle)
        );

        mpg123_delete(handle);
        return -1;
    }

    long rate = 0;
    int channels = 0;
    int encoding = 0;

    if (
        mpg123_getformat(
            handle,
            &rate,
            &channels,
            &encoding
        ) != MPG123_OK
    ) {
        fprintf(
            stderr,
            "No se pudo obtener formato de '%s'\n",
            path
        );

        mpg123_close(handle);
        mpg123_delete(handle);

        return -1;
    }

    format->rate = rate;
    format->channels = channels;
    format->encoding = encoding;

    unsigned char pcm[AUDIO_BLOCK_SIZE];

    int result = MPG123_OK;

    while (1) {
        /*
         * Antes de seguir decodificando comprobamos si
         * esta reproducción sigue siendo vigente.
         */
        if (
            should_continue != NULL &&
            !should_continue(
                callback_context,
                generation
            )
        ) {
            mpg123_close(handle);
            mpg123_delete(handle);

            /*
             * No es un error: la reproducción fue
             * cancelada intencionalmente.
             */
            return 1;
        }

        size_t decoded_bytes = 0;

        result = mpg123_read(
            handle,
            pcm,
            sizeof(pcm),
            &decoded_bytes
        );

        if (result == MPG123_NEW_FORMAT) {
            if (
                mpg123_getformat(
                    handle,
                    &rate,
                    &channels,
                    &encoding
                ) != MPG123_OK
            ) {
                mpg123_close(handle);
                mpg123_delete(handle);
                return -1;
            }

            format->rate = rate;
            format->channels = channels;
            format->encoding = encoding;
        }

        if (decoded_bytes > 0) {
            /*
             * Volvemos a comprobar después de mpg123_read()
             * para reducir todavía más la latencia de next/stop.
             */
            if (
                should_continue != NULL &&
                !should_continue(
                    callback_context,
                    generation
                )
            ) {
                mpg123_close(handle);
                mpg123_delete(handle);

                return 1;
            }

            if (
                audio_buffer_write_tagged(
                    buffer,
                    pcm,
                    decoded_bytes,
                    generation
                ) != 0
            ) {
                /*
                 * Si el reproductor cambió de generación, hizo stop
                 * o entró en shutdown mientras estábamos esperando
                 * espacio en el buffer, no es un error de decodificación.
                 */
                if (
                    should_continue != NULL &&
                    !should_continue(
                        callback_context,
                        generation
                    )
                ) {
                    mpg123_close(handle);
                    mpg123_delete(handle);

                    return 1;
                }

                mpg123_close(handle);
                mpg123_delete(handle);

                return -1;
            }

            *total_bytes += decoded_bytes;
            (*total_blocks)++;
        }

        if (result == MPG123_DONE) {
            break;
        }

        if (
            result != MPG123_OK &&
            result != MPG123_NEW_FORMAT
        ) {
            fprintf(
                stderr,
                "Error decodificando '%s': %s\n",
                path,
                mpg123_strerror(handle)
            );

            mpg123_close(handle);
            mpg123_delete(handle);

            return -1;
        }
    }

    mpg123_close(handle);
    mpg123_delete(handle);

    return 0;
}

int decoder_decode_file(
    const char *path,
    AudioBuffer *buffer,
    AudioFormat *format,
    size_t *total_bytes,
    size_t *total_blocks)
{
    return decoder_decode_file_tagged(
        path,
        buffer,
        format,
        total_bytes,
        total_blocks,
        0,
        NULL,
        NULL
    );
}