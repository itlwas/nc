.SUFFIXES:
MAKEFLAGS += --warn-undefined-variables --no-builtin-rules
.PHONY: all build release debug clean
CC ?= gcc
AR ?= ar
INSTALL ?= install
RM ?= rm -f
MKDIR_P ?= mkdir -p
PROJECT := yoc
VERSION := 1.0.0
PLAT := $(if $(filter Windows_NT,$(OS)),win,unix)
SRC_COMMON := main edit render status buf file utf8 mem
SRC := $(SRC_COMMON) $(PLAT)
OBJ := $(addprefix obj/,$(SRC:=.o))
DEP := $(OBJ:.o=.d)
OUT := $(PROJECT)$(if $(filter Windows_NT,$(OS)),.exe,)
WARNFLAGS := -Wall -Wextra -pedantic -Wshadow -Wconversion
OPTFLAGS := -Os
LTOFLAGS ?= -flto
CSTD := -std=c17
CPPFLAGS := -Iinclude -D_FILE_OFFSET_BITS=64
CFLAGS += $(CSTD) $(CPPFLAGS) $(WARNFLAGS) $(OPTFLAGS) -pipe -ffunction-sections -fdata-sections
ifeq ($(filter Windows_NT,$(OS)),Windows_NT)
LDFLAGS += -Wl,--gc-sections -s
else ifeq ($(shell uname),Darwin)
LDFLAGS += -Wl,-dead_strip
else
LDFLAGS += -Wl,--gc-sections -s
endif
DEBUG_FLAGS := -O0 -g -DDEBUG
RELEASE_FLAGS := -DNDEBUG
all: release
release: CFLAGS += $(RELEASE_FLAGS) $(LTOFLAGS)
release: LDFLAGS += $(LTOFLAGS)
release: build
debug: CFLAGS += $(DEBUG_FLAGS)
debug: LDFLAGS :=
debug: build
build: $(OUT)
$(OUT): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
obj:
	$(MKDIR_P) $@
-include $(DEP)
clean:
	$(RM) -r obj $(OUT)
