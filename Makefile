.PHONY: build clean
CC ?= gcc
CFLAGS := -Iinclude -D_FILE_OFFSET_BITS=64 -Wall -Wextra -pipe -Os -ffunction-sections -fdata-sections
SRC := yoc editor display buffer file utils
PLAT := $(if $(filter Windows_NT,$(OS)),platform_windows,platform_unix)
OBJ := $(addprefix obj/,$(SRC:=.o) $(PLAT).o)
OUT := yoc$(if $(filter Windows_NT,$(OS)),.exe,)
LDFLAGS += -Wl,--gc-sections -s
build: $(OUT)
$(OUT): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@
obj:
	mkdir -p obj
clean:
	rm -rf $(OUT) obj
