CC = gcc

CFLAGS = -Wall -Wextra -Wpedantic -pthread
PKG_CFLAGS = $(shell pkg-config --cflags libmpg123 ao)
PKG_LIBS = $(shell pkg-config --libs libmpg123 ao)

TARGET = audio_player

SOURCES = \
	main.c \
	player.c \
	playlist.c \
	audio_buffer.c \
	decoder.c \
	audio_output.c

OBJECTS = $(SOURCES:.c=.o)

.PHONY: all run clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(PKG_LIBS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) $(PKG_CFLAGS) -c $< -o $@

tests/test_buffer: tests/test_buffer.c audio_buffer.c audio_buffer.h
	$(CC) $(CFLAGS) tests/test_buffer.c audio_buffer.c -o tests/test_buffer

tests/test_playlist: tests/test_playlist.c playlist.c playlist.h
	$(CC) $(CFLAGS) tests/test_playlist.c playlist.c -o tests/test_playlist

tests/test_player: tests/test_player.c player.c playlist.c audio_buffer.c decoder.c audio_output.c
	$(CC) $(CFLAGS) $(PKG_CFLAGS) \
		tests/test_player.c \
		player.c playlist.c audio_buffer.c decoder.c audio_output.c \
		$(PKG_LIBS) \
		-o tests/test_player

tests/test_decoder: tests/test_decoder.c decoder.c audio_buffer.c
	$(CC) $(CFLAGS) $(PKG_CFLAGS) \
		tests/test_decoder.c decoder.c audio_buffer.c \
		$(shell pkg-config --libs libmpg123) \
		-o tests/test_decoder

tests/test_audio_pipeline: tests/test_audio_pipeline.c decoder.c audio_buffer.c audio_output.c
	$(CC) $(CFLAGS) $(PKG_CFLAGS) \
		tests/test_audio_pipeline.c \
		decoder.c audio_buffer.c audio_output.c \
		$(PKG_LIBS) \
		-o tests/test_audio_pipeline

tests/test_player_audio: tests/test_player_audio.c player.c playlist.c audio_buffer.c decoder.c audio_output.c
	$(CC) $(CFLAGS) $(PKG_CFLAGS) \
		tests/test_player_audio.c \
		player.c playlist.c audio_buffer.c decoder.c audio_output.c \
		$(PKG_LIBS) \
		-o tests/test_player_audio

test: tests/test_buffer tests/test_playlist tests/test_player
	./tests/test_buffer
	./tests/test_playlist
	./tests/test_player

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJECTS)
	rm -f $(TARGET)
	rm -f tests/test_buffer
	rm -f tests/test_playlist
	rm -f tests/test_player
	rm -f tests/test_decoder
	rm -f tests/test_audio_pipeline
	rm -f tests/test_player_audio