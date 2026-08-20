CC := gcc

CFLAGS := -std=c11 -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L \
          -Wall -Wextra -Wpedantic -O2

CPPFLAGS := -Iinclude -Ithird_party/mongoose

BUILD_DIR := build

HTTP_OBJECTS := \
	$(BUILD_DIR)/src/http_main.o \
	$(BUILD_DIR)/src/http_server.o \
	$(BUILD_DIR)/src/frame_stream.o \
	$(BUILD_DIR)/third_party/mongoose/mongoose.o

CAMERA_OBJECTS := \
	$(BUILD_DIR)/src/main.o \
	$(BUILD_DIR)/src/frame.o \
	$(BUILD_DIR)/src/camera_v4l2.o \
	$(BUILD_DIR)/src/transport_tcp.o


.PHONY: all clean http camera

all: camera http


# Camera sender
camera: $(BUILD_DIR)/camera_sender

$(BUILD_DIR)/camera_sender: $(CAMERA_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@


# HTTP/WebSocket server
http: $(BUILD_DIR)/http_server

$(BUILD_DIR)/http_server: $(HTTP_OBJECTS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $^ -o $@


# Camera source files
$(BUILD_DIR)/src/main.o: src/main.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/frame.o: src/frame.c include/frame.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/camera_v4l2.o: src/camera_v4l2.c include/camera_v4l2.h include/frame.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/transport_tcp.o: src/transport_tcp.c include/transport_tcp.h include/frame.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@


# HTTP/WebSocket source files
$(BUILD_DIR)/src/http_main.o: src/http_main.c include/http_server.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/http_server.o: src/http_server.c include/http_server.h include/frame_stream.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/src/frame_stream.o: src/frame_stream.c include/frame_stream.h include/frame.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@


# Mongoose
$(BUILD_DIR)/third_party/mongoose/mongoose.o: third_party/mongoose/mongoose.c third_party/mongoose/mongoose.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -include alloca.h -c $< -o $@


# Clean build files
clean:
	rm -rf $(BUILD_DIR)
