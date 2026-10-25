/* psp5 – made by Mille Weber */
#ifndef LZO_H
#define LZO_H

#include <stddef.h>

/* LZO1X decompression with the same call as the reference library (liblzo2), because libcisoread calls it by this name when it
 * reads a .jso packed with LZO. `*dst_len` is the room in `dst` on the way in and the number of bytes written on the way out
 * (also after an error). `wrk` is not used. Returns 0 when the data was decoded and ended properly, a negative number if not
 * (-1 error, -4 input too short, -5 output too small, -6 a match reaching before the start, -8 data left after the end). */
int lzo1x_decompress(const void *src, unsigned src_len, void *dst, unsigned *dst_len, void *wrk);

#endif
