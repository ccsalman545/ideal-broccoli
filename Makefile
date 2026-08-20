CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS := -Iinclude

TARGET := build/camera_sender

SRC := \
	src/main.c \
	src/frame.c \
	src/camera_v4l2.c \
	src/transport_tcp.c

OBJ := $(SRC:%.c=build/%.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(OBJ) -o $@

build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build
