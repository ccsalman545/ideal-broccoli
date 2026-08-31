/*
 * yuv_convert.h
 *
 * Pixel format conversion for the encode path.
 *
 * The camera delivers YUYV 4:2:2 (packed) or YU12 4:2:0
 * (planar). The encode path converts to planar I420:
 *   YUYV -> I420   (packed camera source)
 *   I420  -> I420  (stride normalized copy for planar sources)
 *
 * The hardware encoder backend performs its own I420 to NV12
 * interleaving directly into the driver buffers.
 *
 * All routines process two rows at once so 4:2:2 -> 4:2:0
 * chroma decimation needs only one pass with no intermediate
 * buffers.
 */
#ifndef MEDIA_YUV_CONVERT_H
#define MEDIA_YUV_CONVERT_H

#include <stddef.h>
#include <stdint.h>

/* Source is YUYV with 'src_stride' bytes per row. */
void yuyv_to_i420(const uint8_t *src, uint32_t src_stride,
                  uint8_t *dst_y, uint8_t *dst_u, uint8_t *dst_v,
                  uint32_t width, uint32_t height);

/* Stride normalized planar copy, used when the source is already I420. */
void i420_copy(const uint8_t *src_y, uint32_t src_y_stride,
               const uint8_t *src_u, const uint8_t *src_v,
               uint8_t *dst_y, uint8_t *dst_u, uint8_t *dst_v,
               uint32_t width, uint32_t height);

#endif
