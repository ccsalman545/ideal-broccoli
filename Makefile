CC := gcc

CFLAGS := -std=c11 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L \
          -Wall -Wextra -Wpedantic -O2

CPPFLAGS := -Iinclude -Ithird_party/mongoose

# =================================================
# Camera Sender
# =================================================

CAMERA_TARGET := build/camera_sender

CAMERA_SRC := \
	src/main.c \
	src/frame.c \
	src/camera_v4l2.c \
	src/transport_tcp.c

CAMERA_OBJ := \
	build/src/main.o \
	build/src/frame.o \
	build/src/camera_v4l2.o \
	build/src/transport_tcp.o


# =================================================
# Mongoose HTTP Server
# =================================================

HTTP_TARGET := build/http_server

HTTP_OBJ := \
	build/src/http_main.o \
	build/src/http_server.o \
	build/third_party/mongoose/mongoose.o


# =================================================
# Main Targets
# =================================================

.PHONY: all camera http clean

all: camera http

camera: $(CAMERA_TARGET)

http: $(HTTP_TARGET)


# =================================================
# Link Camera Sender
# =================================================

$(CAMERA_TARGET): $(CAMERA_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CAMERA_OBJ) -o $@


# =================================================
# Link HTTP Server
# =================================================

$(HTTP_TARGET): $(HTTP_OBJ)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(HTTP_OBJ) -o $@


# =================================================
# Compile Project Source Files
# =================================================

build/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@


# =================================================
# Compile Mongoose
# =================================================

build/third_party/mongoose/mongoose.o: third_party/mongoose/mongoose.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -include alloca.h -c $< -o $@


# =================================================
# Clean Build
# =================================================

clean:
	rm -rf build
