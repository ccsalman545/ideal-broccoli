CC = gcc

CFLAGS = -Iinclude -Ithird_party/mongoose \
	-std=c11 \
	-D_DEFAULT_SOURCE \
	-D_POSIX_C_SOURCE=200809L \
	-Wall -Wextra -Wpedantic \
	-O2

LDFLAGS = -pthread

BUILD_DIR = build

HTTP_SOURCES = \
	src/http_main.c \
	src/http_server.c \
	src/frame_stream.c \
	src/frame_queue.c \
	src/camera_v4l2.c \
	src/camera_worker.c

HTTP_OBJECTS = \
	$(BUILD_DIR)/src/http_main.o \
	$(BUILD_DIR)/src/http_server.o \
	$(BUILD_DIR)/src/frame_stream.o \
	$(BUILD_DIR)/src/frame_queue.o \
	$(BUILD_DIR)/src/camera_v4l2.o \
	$(BUILD_DIR)/src/camera_worker.o \
	$(BUILD_DIR)/third_party/mongoose/mongoose.o


.PHONY: all http clean

all: http


http: $(BUILD_DIR)/http_server


$(BUILD_DIR)/http_server: $(HTTP_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(HTTP_OBJECTS) -o $@ $(LDFLAGS)


$(BUILD_DIR)/src/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@


$(BUILD_DIR)/third_party/mongoose/mongoose.o: third_party/mongoose/mongoose.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -include alloca.h -c $< -o $@


clean:
	rm -rf $(BUILD_DIR)
