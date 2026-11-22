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

SRC := src/main.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c src/song.c src/player.c src/midi_null.c src/midi_capture.c src/ui.c src/history.c src/help.c src/device.c src/input.c src/battery.c src/usbinfo.c $(MIDI_SRC)

all: seq4h

seq4h: $(SRC) src/ui_live.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $(MIDI_CFLAGS) -o $@ $(SRC) $(SDL_LIBS) $(MIDI_LIBS) -lpthread

test: tests/test_engine tests/test_mod tests/test_gen tests/test_ambient tests/test_poly tests/test_rhythm tests/test_harmony tests/test_drum tests/test_lock tests/test_input tests/test_battery tests/test_usbinfo tests/test_song tests/test_device tests/test_live
	./tests/test_engine
	./tests/test_mod
	./tests/test_song
	./tests/test_live
	./tests/test_device
	./tests/test_drum
	./tests/test_lock
	./tests/test_gen
	./tests/test_ambient
	./tests/test_poly
	./tests/test_rhythm
	./tests/test_harmony
	./tests/test_input
	./tests/test_battery
	./tests/test_usbinfo
	./tests/test_history

tests/test_engine: tests/test_engine.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_engine.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c

tests/test_mod: tests/test_mod.c src/song.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_mod.c src/song.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c

tests/test_gen: tests/test_gen.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_gen.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c

tests/test_drum: tests/test_drum.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_drum.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c

tests/test_lock: tests/test_lock.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_lock.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c

tests/test_ambient: tests/test_ambient.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_ambient.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c

tests/test_poly: tests/test_poly.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_poly.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c

tests/test_rhythm: tests/test_rhythm.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_rhythm.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c

tests/test_harmony: tests/test_harmony.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_harmony.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c

tests/test_device: tests/test_device.c src/device.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_device.c src/device.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c

tests/test_live: tests/test_live.c src/song.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_live.c src/song.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c

tests/test_song: tests/test_song.c src/song.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_song.c src/song.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c

test: tests/test_history   # the undo history (see test: above for the rest)

tests/test_history: tests/test_history.c src/history.c src/live.c src/engine.c src/gen.c src/drum.c src/mod.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tests/test_history.c src/history.c src/live.c src/engine.c src/gen.c src/drum.c src/mod.c

tests/test_battery: tests/test_battery.c src/battery.c src/battery.h
	$(CC) $(CFLAGS) -o $@ tests/test_battery.c src/battery.c

tests/test_usbinfo: tests/test_usbinfo.c src/usbinfo.c src/usbinfo.h
	$(CC) $(CFLAGS) -o $@ tests/test_usbinfo.c src/usbinfo.c

tests/test_input: tests/test_input.c src/input.c src/input.h
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ tests/test_input.c src/input.c $(SDL_LIBS)

# Offline renderer: the engine without SDL, ALSA or threads (see tools/render.c).
seq4h-render: tools/render.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c src/midi_capture.c $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ tools/render.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c src/midi_capture.c

# Engine tests under AddressSanitizer and UndefinedBehaviorSanitizer.
test-asan: tests/test_live.c tests/test_engine.c tests/test_song.c tests/test_mod.c src/song.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c $(wildcard src/*.h)
	$(CC) -O1 -g -std=gnu11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all \
	    -o tests/test_engine_asan tests/test_engine.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c
	./tests/test_engine_asan
	$(CC) -O1 -g -std=gnu11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all \
	    -o tests/test_live_asan tests/test_live.c src/song.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c
	./tests/test_live_asan
	$(CC) -O1 -g -std=gnu11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all \
	    -o tests/test_drum_asan tests/test_drum.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c
	./tests/test_drum_asan
	$(CC) -O1 -g -std=gnu11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all \
	    -o tests/test_poly_asan tests/test_poly.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c
	./tests/test_poly_asan
	$(CC) -O1 -g -std=gnu11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all \
	    -o tests/test_song_asan tests/test_song.c src/song.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c
	./tests/test_song_asan
	$(CC) -O1 -g -std=gnu11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all \
	    -o tests/test_mod_asan tests/test_mod.c src/song.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c
	./tests/test_mod_asan
	$(CC) -O1 -g -std=gnu11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all \
	    -o tests/test_rhythm_asan tests/test_rhythm.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c
	./tests/test_rhythm_asan
	$(CC) -O1 -g -std=gnu11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all \
	    -o tests/test_harmony_asan tests/test_harmony.c src/engine.c src/gen.c src/drum.c src/mod.c src/live.c
	./tests/test_harmony_asan

# The hardware-free harness: MIDI capture checks, screenshots, optional ALSA
# loopback. Writes everything to out/. See tests/harness.sh.
check: seq4h seq4h-render test
	./tests/harness.sh

screenshots: seq4h
	./tests/harness.sh screenshots

clean:
	rm -f seq4h seq4h-render tests/test_engine tests/test_mod tests/test_mod_asan tests/test_engine_asan tests/test_drum_asan tests/test_song_asan tests/test_poly_asan tests/test_rhythm tests/test_rhythm_asan tests/test_harmony tests/test_harmony_asan tests/test_gen tests/test_ambient tests/test_poly tests/test_drum tests/test_lock tests/test_input tests/test_battery tests/test_usbinfo tests/test_song tests/test_device
	rm -f tests/test_history
	rm -rf out

.PHONY: all test test-asan check screenshots clean
