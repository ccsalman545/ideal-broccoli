#ifndef FRAME_H
#define FRAME_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pixel_format;
    uint32_t stride;

    size_t size;

    uint64_t sequence;
    uint64_t timestamp_us;

    uint32_t buffer_index;

    uint8_t *data;
} Frame;

double frame_average_luminance(const Frame *frame);

#endif
