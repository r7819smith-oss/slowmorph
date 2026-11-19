CC ?= cc
CFLAGS ?= -O2 -g
CFLAGS += -std=gnu11 -Wall -Wextra
PKG ?= pkg-config

HAVE_ALSA := $(shell $(PKG) --exists alsa && echo 1)
SDL_CFLAGS := $(shell $(PKG) --cflags sdl2)
SDL_LIBS := $(shell $(PKG) --libs sdl2)
ifeq ($(HAVE_ALSA),1)
  MIDI_SRC := src/midi_alsa.c
  MIDI_CFLAGS := -DHAVE_ALSA $(shell $(PKG) --cflags alsa)
  MIDI_LIBS := $(shell $(PKG) --libs alsa)
else
  MIDI_SRC :=
  $(warning ALSA not found: building without MIDI hardware output)
endif

SRC := src/main.c src/engine.c src/gen.c src/player.c src/midi_null.c src/ui.c src/input.c $(MIDI_SRC)

all: seq4h

seq4h: $(SRC) $(wildcard src/*.h)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $(MIDI_CFLAGS) -o $@ $(SRC) $(SDL_LIBS) $(MIDI_LIBS) -lpthread

test: tests/test_engine tests/test_gen tests/test_input
	./tests/test_engine
	./tests/test_gen
	./tests/test_input

tests/test_engine: tests/test_engine.c src/engine.c src/gen.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_engine.c src/engine.c src/gen.c

tests/test_gen: tests/test_gen.c src/engine.c src/gen.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_gen.c src/engine.c src/gen.c

tests/test_input: tests/test_input.c src/input.c src/input.h
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ tests/test_input.c src/input.c $(SDL_LIBS)

clean:
	rm -f seq4h tests/test_engine tests/test_gen tests/test_input

.PHONY: all test clean
