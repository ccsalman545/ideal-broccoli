#include "frame.h"

double frame_average_luminance(const Frame *frame)
{
    if (!frame || !frame->data || frame->size < 2)
        return 0.0;

    uint64_t sum = 0;
    size_t pixels = 0;

    /*
     * YUYV layout:
     *
     * Y0 U0 Y1 V0 Y2 U1 Y3 V1 ...
     *
     * Every second byte is a luminance value.
     */
    for (size_t i = 0; i < frame->size; i += 2) {
        sum += frame->data[i];
        pixels++;
    }

    if (pixels == 0)
        return 0.0;

    return (double)sum / pixels;
}
