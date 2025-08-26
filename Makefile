.PHONY: all build release debug clean

CC ?= gcc

PLAT := $(if $(filter Windows_NT,$(OS)),win,unix)
SRC := $(filter-out src/win.c src/unix.c,$(wildcard src/*.c)) \
       src/$(PLAT).c
OBJ := $(SRC:src/%.c=obj/%.o)
OUT := yoc$(if $(filter Windows_NT,$(OS)),.exe,)

VERSION ?= 1.0.0
HASH := $(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)
DATE := $(shell date +%Y-%m-%d)

CFLAGS += -std=c17 -Iinclude -D_FILE_OFFSET_BITS=64 \
           -Wall -Wextra -pedantic -Wshadow -Wconversion \
           -Os -pipe -ffunction-sections -fdata-sections \
           -DYOC_VERSION=\"$(VERSION)\" -DYOC_HASH=\"$(HASH)\" \
           -DYOC_DATE=\"$(DATE)\"

ifeq ($(shell uname),Darwin)
LDFLAGS += -Wl,-dead_strip
else
LDFLAGS += -Wl,--gc-sections -s
endif

-include $(OBJ:.o=.d)

all: release

release: CFLAGS += -DNDEBUG -flto
release: LDFLAGS += -flto
release: build

debug: CFLAGS += -O0 -g -DDEBUG
debug: LDFLAGS :=
debug: build

build: $(OUT)

$(OUT): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

obj:
	mkdir -p $@

clean:
	rm -f -r obj $(OUT)
