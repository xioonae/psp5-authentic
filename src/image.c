/* psp5 – made by Mille Weber */
#include <pspkernel.h>
#include <png.h>
#include <stdio.h>
#include <setjmp.h>
#include <jpeglib.h>
#include <malloc.h>
#include <math.h>
#include <string.h>
#include "image.h"

#define NTX_MAGIC 0x3158544E /* "NTX1" */
#define MAX_PNG_PIXELS (512 * 512)
#define MAX_COVER_PIXELS (1024 * 1024) /* omslag är ofta större än ikoner */
/* A progressive JPEG is decoded whole: libjpeg keeps every coefficient of the picture (2 bytes each, 64 to a block, in every component) before it makes the first
 * pixel, which no row-by-row reading can avoid. That is read from the header (the blocks of each component) and what needs more than this is not decoded: the cover is
 * not shown, and the heap (about 13 MB on a PSP-1000) is not run dry for the icons, the lists and the music. About 1300 x 1300 pixels at the most with the colours at
 * half size (the usual), 930 x 930 with all of them. A JPEG that is not progressive is read a row at a time and needs only the picture it makes. */
#define MAX_PROGRESSIVE_BYTES (5 << 20)

typedef struct {
    u32 magic;
    u16 w, h;
    u16 psm, stride;
    u32 reserved;
} NtxHeader;

static int decode_png(const void *data, u32 size, Image *out, u32 max_pixels)
{
    png_image png;
    memset(&png, 0, sizeof(png));
    png.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&png, data, size))
        return 0;
    /* 64 bitar: width * height i 32 bitar kan gå runt (65536 x 65536 blir 0) och släppa igenom en jättebild */
    if ((u64)png.width * png.height > max_pixels) {
        png_image_free(&png);
        return 0;
    }
    png.format = PNG_FORMAT_RGBA; /* R,G,B,A i minnet = ABGR som u32 på little endian */
    out->w = png.width;
    out->h = png.height;
    out->px = malloc(png.width * png.height * 4);
    if (!out->px) {
        png_image_free(&png);
        return 0;
    }
    if (!png_image_finish_read(&png, NULL, out->px, 0, NULL)) {
        img_free(out);
        return 0;
    }
    return 1;
}

int img_decode_png(const void *data, u32 size, Image *out)
{
    return decode_png(data, size, out, MAX_PNG_PIXELS);
}

/* libjpeg avslutar programmet vid fel om man inte tar över felhanteringen */
typedef struct {
    struct jpeg_error_mgr pub;
    jmp_buf jump;
} JpegError;

static void jpeg_fail(j_common_ptr cinfo)
{
    longjmp(((JpegError *)cinfo->err)->jump, 1);
}

static void jpeg_quiet(j_common_ptr cinfo, int level) {}

/* the memory the coefficients of a progressive JPEG take (valid once the header is read) */
static unsigned long long progressive_bytes(const struct jpeg_decompress_struct *c)
{
    unsigned long long blocks = 0;
    for (int i = 0; i < c->num_components; i++)
        blocks += (unsigned long long)c->comp_info[i].width_in_blocks * c->comp_info[i].height_in_blocks;
    return blocks * 64 * 2;
}

static int decode_jpeg(const void *data, u32 size, Image *out, int min_side)
{
    struct jpeg_decompress_struct cinfo;
    JpegError err;
    u8 *volatile row = NULL; /* ändras efter setjmp – måste överleva longjmp */
    out->px = NULL;
    cinfo.err = jpeg_std_error(&err.pub);
    err.pub.error_exit = jpeg_fail;
    err.pub.emit_message = jpeg_quiet;
    if (setjmp(err.jump)) {
        jpeg_destroy_decompress(&cinfo);
        free((void *)row);
        img_free(out);
        return 0;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, (const unsigned char *)data, size);
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK ||
        (cinfo.progressive_mode && progressive_bytes(&cinfo) > MAX_PROGRESSIVE_BYTES)) {
        jpeg_destroy_decompress(&cinfo);
        return 0;
    }
    /* skala ner redan i avkodaren (1/2, 1/4, 1/8) så länge bilden blir minst min_side */
    int side = cinfo.image_width < cinfo.image_height ? cinfo.image_width : cinfo.image_height;
    cinfo.scale_num = 1;
    cinfo.scale_denom = 1;
    while (cinfo.scale_denom < 8 && side / (int)(cinfo.scale_denom * 2) >= min_side)
        cinfo.scale_denom *= 2;
    cinfo.out_color_space = JCS_RGB;
    /* omslagen krymps ändå till några tiotal pixlar: den snabba avkodningen syns inte men sparar tid */
    cinfo.dct_method = JDCT_IFAST;
    cinfo.do_fancy_upsampling = FALSE;
    jpeg_start_decompress(&cinfo);
    int w = cinfo.output_width, h = cinfo.output_height;
    if ((u64)w * h > MAX_COVER_PIXELS || cinfo.output_components != 3)
        longjmp(err.jump, 1);
    out->w = w;
    out->h = h;
    out->px = malloc(w * h * 4);
    row = malloc(w * 3);
    if (!out->px || !row)
        longjmp(err.jump, 1);
    while (cinfo.output_scanline < cinfo.output_height) {
        int y = cinfo.output_scanline;
        JSAMPROW rows[1] = { row };
        jpeg_read_scanlines(&cinfo, rows, 1);
        for (int x = 0; x < w; x++)
            out->px[y * w + x] = RGBA(row[x * 3], row[x * 3 + 1], row[x * 3 + 2], 255);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    free((void *)row);
    return 1;
}

int img_decode_cover(const void *data, u32 size, Image *out, int min_side)
{
    const u8 *b = data;
    if (size >= 8 && b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G')
        return decode_png(data, size, out, MAX_COVER_PIXELS);
    if (size >= 4 && b[0] == 0xFF && b[1] == 0xD8)
        return decode_jpeg(data, size, out, min_side);
    return 0;
}

int img_crop_square(Image *img)
{
    int side = img->w < img->h ? img->w : img->h;
    if (img->w == img->h)
        return 1;
    u32 *px = malloc(side * side * 4);
    if (!px)
        return 0;
    int x0 = (img->w - side) / 2, y0 = (img->h - side) / 2;
    for (int y = 0; y < side; y++)
        memcpy(px + y * side, img->px + (y + y0) * img->w + x0, side * 4);
    free(img->px);
    img->px = px;
    img->w = img->h = side;
    return 1;
}

/* ---------------------------------------------------------------------- */
/* pictures for the image viewer */

#define MAX_VIEW_PNG_PIXELS 2200000

void img_fit_down(int w, int h, int mw, int mh, int *ow, int *oh)
{
    if (w <= mw && h <= mh) {
        *ow = w;
        *oh = h;
        return;
    }
    if ((long long)w * mh >= (long long)h * mw) { /* wider than the box: the width is what limits */
        *ow = mw;
        *oh = (int)(((long long)h * mw + w / 2) / w);
    } else {
        *oh = mh;
        *ow = (int)(((long long)w * mh + h / 2) / h);
    }
    if (*ow < 1)
        *ow = 1;
    if (*oh < 1)
        *oh = 1;
}

/* the way a camera says the picture is turned (EXIF, tag 0x0112): 1 = as it is ... 8, 1 when it says nothing; d points at the data of an APP1 marker */
static int exif_orientation(const unsigned char *d, unsigned len)
{
    if (len < 14 || memcmp(d, "Exif\0\0", 6))
        return 1;
    const unsigned char *t = d + 6;
    unsigned tl = len - 6;
    int le = t[0] == 'I' && t[1] == 'I';
    if (!le && !(t[0] == 'M' && t[1] == 'M'))
        return 1;
#define U16(p) (le ? (unsigned)((p)[0] | ((p)[1] << 8)) : (unsigned)(((p)[0] << 8) | (p)[1]))
#define U32(p) (le ? (unsigned)((p)[0] | ((p)[1] << 8) | ((p)[2] << 16) | ((unsigned)(p)[3] << 24)) : (unsigned)(((unsigned)(p)[0] << 24) | ((p)[1] << 16) | ((p)[2] << 8) | (p)[3]))
    unsigned off = U32(t + 4);
    if (off > tl || off + 2 > tl)
        return 1;
    unsigned n = U16(t + off);
    for (unsigned i = 0; i < n; i++) {
        unsigned at = off + 2 + 12 * i;
        if (at + 12 > tl)
            break;
        if (U16(t + at) == 0x0112) {
            unsigned v = U16(t + at + 8);
            return v >= 1 && v <= 8 ? (int)v : 1;
        }
    }
#undef U16
#undef U32
    return 1;
}

/* turns the picture as EXIF orientation o says (2 mirror, 3 half turn, 4 flip, 5 mirror on the diagonal, 6 a quarter turn clockwise, 7 mirror on the other diagonal, 8 a quarter turn the other way) */
static int orient_image(Image *img, int o)
{
    if (o <= 1 || o > 8)
        return 1;
    int w = img->w, h = img->h, ow = o >= 5 ? h : w, oh = o >= 5 ? w : h;
    u32 *px = malloc((size_t)ow * oh * 4);
    if (!px)
        return 0;
    for (int y = 0; y < oh; y++)
        for (int x = 0; x < ow; x++) {
            int sx, sy;
            switch (o) {
            case 2: sx = w - 1 - x; sy = y; break;
            case 3: sx = w - 1 - x; sy = h - 1 - y; break;
            case 4: sx = x; sy = h - 1 - y; break;
            case 5: sx = y; sy = x; break;
            case 6: sx = y; sy = h - 1 - x; break;
            case 7: sx = w - 1 - y; sy = h - 1 - x; break;
            default: sx = w - 1 - y; sy = x; break;
            }
            px[y * ow + x] = img->px[sy * w + sx];
        }
    free(img->px);
    img->px = px;
    img->w = ow;
    img->h = oh;
    return 1;
}

/* the picture made smaller to the size it is shown at (the area average of img_resize), when it is bigger than that */
static int shrink_to_fit(Image *img, int mw, int mh)
{
    int fw, fh;
    img_fit_down(img->w, img->h, mw, mh, &fw, &fh);
    if (fw == img->w && fh == img->h)
        return 1;
    Image small;
    if (!img_resize(img, fw, fh, &small))
        return 0;
    img_free(img);
    *img = small;
    return 1;
}

static int load_jpeg_fit(FILE *fp, int mw, int mh, Image *out)
{
    struct jpeg_decompress_struct cinfo;
    JpegError err;
    u8 *volatile row = NULL;
    out->px = NULL;
    cinfo.err = jpeg_std_error(&err.pub);
    err.pub.error_exit = jpeg_fail;
    err.pub.emit_message = jpeg_quiet;
    if (setjmp(err.jump)) {
        jpeg_destroy_decompress(&cinfo);
        free((void *)row);
        img_free(out);
        return 0;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    jpeg_save_markers(&cinfo, JPEG_APP0 + 1, 0xFFFF); /* the camera's EXIF: which way up the picture is */
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK || (cinfo.progressive_mode && progressive_bytes(&cinfo) > MAX_PROGRESSIVE_BYTES)) {
        jpeg_destroy_decompress(&cinfo);
        return 0;
    }
    int orient = 1;
    for (jpeg_saved_marker_ptr mk = cinfo.marker_list; mk; mk = mk->next)
        if (mk->marker == JPEG_APP0 + 1) {
            orient = exif_orientation(mk->data, mk->data_length);
            break;
        }
    /* the size it is made at, before it is turned: the box the other way round when the camera was held sideways */
    int bw = orient >= 5 ? mh : mw, bh = orient >= 5 ? mw : mh, fw, fh;
    img_fit_down((int)cinfo.image_width, (int)cinfo.image_height, bw, bh, &fw, &fh);
    cinfo.scale_num = 1;
    cinfo.scale_denom = 1;
    while (cinfo.scale_denom < 8 && (int)(cinfo.image_width / (cinfo.scale_denom * 2)) >= fw && (int)(cinfo.image_height / (cinfo.scale_denom * 2)) >= fh)
        cinfo.scale_denom *= 2;
    cinfo.out_color_space = JCS_RGB;
    cinfo.dct_method = JDCT_ISLOW; /* a photo is looked at: the exact one */
    jpeg_start_decompress(&cinfo);
    int w = cinfo.output_width, h = cinfo.output_height;
    if ((u64)w * h > 4000000ull || cinfo.output_components != 3)
        longjmp(err.jump, 1);
    out->w = w;
    out->h = h;
    out->px = malloc((size_t)w * h * 4);
    row = malloc(w * 3);
    if (!out->px || !row)
        longjmp(err.jump, 1);
    while (cinfo.output_scanline < cinfo.output_height) {
        int y = cinfo.output_scanline;
        JSAMPROW rows[1] = { row };
        jpeg_read_scanlines(&cinfo, rows, 1);
        for (int x = 0; x < w; x++)
            out->px[y * w + x] = RGBA(row[x * 3], row[x * 3 + 1], row[x * 3 + 2], 255);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    free((void *)row);
    if (!shrink_to_fit(out, bw, bh) || !orient_image(out, orient)) {
        img_free(out);
        return 0;
    }
    return 1;
}

static int load_png_fit(FILE *fp, int mw, int mh, Image *out)
{
    png_image png;
    memset(&png, 0, sizeof(png));
    png.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_stdio(&png, fp))
        return 0;
    if ((u64)png.width * png.height > MAX_VIEW_PNG_PIXELS) {
        png_image_free(&png);
        return 0;
    }
    png.format = PNG_FORMAT_RGBA;
    out->w = png.width;
    out->h = png.height;
    out->px = malloc((size_t)png.width * png.height * 4);
    if (!out->px) {
        png_image_free(&png);
        return 0;
    }
    if (!png_image_finish_read(&png, NULL, out->px, 0, NULL) || !shrink_to_fit(out, mw, mh)) {
        img_free(out);
        return 0;
    }
    return 1;
}

static unsigned le16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static unsigned le32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24); }

/* A BMP is read a row at a time, and every block of step x step pixels becomes one pixel of the result, so a big one never needs more memory than the picture it makes */
static int load_bmp_fit(FILE *fp, int mw, int mh, Image *out)
{
    unsigned char hd[14 + 124];
    if (fread(hd, 1, 14 + 12, fp) != 14 + 12 || hd[0] != 'B' || hd[1] != 'M')
        return 0;
    unsigned data_off = le32(hd + 10), dib = le32(hd + 14);
    if (dib < 12 || dib > 124 || fread(hd + 26, 1, dib - 12, fp) != dib - 12)
        return 0;
    int w, h, bpp, top_down = 0;
    unsigned comp = 0, colors = 0;
    if (dib == 12) { /* the old header: 16-bit sizes, 3-byte palette entries */
        w = (int)le16(hd + 18);
        h = (int)le16(hd + 20);
        bpp = (int)le16(hd + 24);
    } else {
        w = (int)le32(hd + 18);
        h = (int)le32(hd + 22);
        bpp = (int)le16(hd + 28);
        comp = le32(hd + 30);
        colors = le32(hd + 46);
    }
    if (h < 0) {
        h = -h;
        top_down = 1;
    }
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384 || (comp != 0 && !(comp == 3 && (bpp == 32 || bpp == 16))))
        return 0;
    if (bpp != 1 && bpp != 4 && bpp != 8 && bpp != 16 && bpp != 24 && bpp != 32)
        return 0;
    unsigned char pal[256][3] = { { 0 } }; /* r, g, b */
    if (bpp <= 8) {
        unsigned n = colors ? colors : 1u << bpp, esz = dib == 12 ? 3 : 4;
        if (n > 256)
            n = 256;
        if (fseek(fp, (long)(14 + dib), SEEK_SET))
            return 0;
        for (unsigned i = 0; i < n; i++) {
            unsigned char e[4];
            if (fread(e, 1, esz, fp) != esz)
                return 0;
            pal[i][0] = e[2];
            pal[i][1] = e[1];
            pal[i][2] = e[0];
        }
    }
    unsigned stride = (((unsigned)w * bpp + 31) / 32) * 4;
    int step = 1;
    while ((w + step - 1) / step > mw || (h + step - 1) / step > mh)
        step++;
    int ow = (w + step - 1) / step, oh = (h + step - 1) / step;
    unsigned char *line = malloc(stride);
    u32 *sum = calloc((size_t)ow * 3, sizeof(u32));
    out->w = ow;
    out->h = oh;
    out->px = malloc((size_t)ow * oh * 4);
    if (!line || !sum || !out->px) {
        free(line);
        free(sum);
        img_free(out);
        return 0;
    }
    int ok = 1;
    for (int oy = 0; oy < oh && ok; oy++) {
        memset(sum, 0, (size_t)ow * 3 * sizeof(u32));
        int rows = 0;
        for (int k = 0; k < step && oy * step + k < h && ok; k++) {
            int sy = oy * step + k, frow = top_down ? sy : h - 1 - sy;
            if (fseek(fp, (long)(data_off + (unsigned)frow * stride), SEEK_SET) || fread(line, 1, stride, fp) != stride) {
                ok = 0;
                break;
            }
            for (int x = 0; x < w; x++) {
                unsigned r, g, b;
                if (bpp == 24) {
                    b = line[x * 3]; g = line[x * 3 + 1]; r = line[x * 3 + 2];
                } else if (bpp == 32) {
                    b = line[x * 4]; g = line[x * 4 + 1]; r = line[x * 4 + 2];
                } else if (bpp == 16) {
                    unsigned v = le16(line + x * 2);
                    r = ((v >> 10) & 31) * 255 / 31; g = ((v >> 5) & 31) * 255 / 31; b = (v & 31) * 255 / 31;
                } else {
                    unsigned idx = bpp == 8 ? line[x] : bpp == 4 ? (line[x / 2] >> (x & 1 ? 0 : 4)) & 15 : (line[x / 8] >> (7 - (x & 7))) & 1;
                    r = pal[idx][0]; g = pal[idx][1]; b = pal[idx][2];
                }
                u32 *s = sum + (x / step) * 3;
                s[0] += r; s[1] += g; s[2] += b;
            }
            rows++;
        }
        for (int ox = 0; ox < ow && ok; ox++) {
            int cols = step;
            if ((ox + 1) * step > w)
                cols = w - ox * step;
            unsigned n = (unsigned)(rows * cols);
            if (!n)
                n = 1;
            out->px[oy * ow + ox] = RGBA((sum[ox * 3] + n / 2) / n, (sum[ox * 3 + 1] + n / 2) / n, (sum[ox * 3 + 2] + n / 2) / n, 255);
        }
    }
    free(line);
    free(sum);
    if (!ok) {
        img_free(out);
        return 0;
    }
    return 1;
}

int img_load_fit(const char *path, int max_w, int max_h, Image *out)
{
    out->px = NULL;
    out->w = out->h = 0;
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return 0;
    unsigned char b[8];
    size_t got = fread(b, 1, sizeof(b), fp);
    rewind(fp);
    int ok = 0;
    if (got >= 4 && b[0] == 0xFF && b[1] == 0xD8)
        ok = load_jpeg_fit(fp, max_w, max_h, out);
    else if (got >= 8 && b[0] == 0x89 && b[1] == 'P' && b[2] == 'N' && b[3] == 'G')
        ok = load_png_fit(fp, max_w, max_h, out);
    else if (got >= 2 && b[0] == 'B' && b[1] == 'M')
        ok = load_bmp_fit(fp, max_w, max_h, out);
    fclose(fp);
    return ok;
}

void img_free(Image *img)
{
    free(img->px);
    img->px = NULL;
}

#define CH(c, s) (((c) >> (s)) & 0xFF)

int img_resize(const Image *src, int dw, int dh, Image *out)
{
    out->w = dw;
    out->h = dh;
    out->px = malloc(dw * dh * 4);
    if (!out->px)
        return 0;

    float sx = (float)src->w / dw, sy = (float)src->h / dh;
    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            u32 acc[4] = { 0 };
            u32 r;
            if (sx > 1.0f || sy > 1.0f) {
                /* förminskning: medelvärde av källrutan */
                int x0 = x * sx, x1 = (x + 1) * sx, y0 = y * sy, y1 = (y + 1) * sy;
                if (x1 <= x0) x1 = x0 + 1;
                if (y1 <= y0) y1 = y0 + 1;
                int n = 0;
                for (int yy = y0; yy < y1 && yy < src->h; yy++)
                    for (int xx = x0; xx < x1 && xx < src->w; xx++, n++) {
                        u32 c = src->px[yy * src->w + xx];
                        for (int k = 0; k < 4; k++)
                            acc[k] += CH(c, k * 8);
                    }
                if (!n) n = 1;
                r = 0;
                for (int k = 0; k < 4; k++)
                    r |= (acc[k] / n) << (k * 8);
            } else {
                /* förstoring: bilinjär */
                float fx = (x + 0.5f) * sx - 0.5f, fy = (y + 0.5f) * sy - 0.5f;
                if (fx < 0) fx = 0;
                if (fy < 0) fy = 0;
                int ix = fx, iy = fy;
                int ix1 = ix + 1 < src->w ? ix + 1 : ix, iy1 = iy + 1 < src->h ? iy + 1 : iy;
                float ax = fx - ix, ay = fy - iy;
                u32 c00 = src->px[iy * src->w + ix], c10 = src->px[iy * src->w + ix1];
                u32 c01 = src->px[iy1 * src->w + ix], c11 = src->px[iy1 * src->w + ix1];
                r = 0;
                for (int k = 0; k < 4; k++) {
                    float top = CH(c00, k * 8) * (1 - ax) + CH(c10, k * 8) * ax;
                    float bot = CH(c01, k * 8) * (1 - ax) + CH(c11, k * 8) * ax;
                    r |= ((u32)(top * (1 - ay) + bot * ay + 0.5f)) << (k * 8);
                }
            }
            out->px[y * dw + x] = r;
        }
    }
    return 1;
}

/* En boxoskärpa i en riktning (src sammanhängande, dst med steget `step`);
 * tre pass i varje led ≈ gaussisk oskärpa. */
static void box_pass(u32 *dst, const u32 *src, int len, int step, int radius)
{
    int win = radius * 2 + 1;
    /* sum / win without a (slow) division: exact for every sum up to 255 * win, as win is odd and
     * far below 2^32 / (255 * win) */
    const u32 magic = (u32)((1ull << 32) / (u32)win + 1);
    u32 sum[4] = { 0 };
    for (int i = -radius; i <= radius; i++) {
        int j = i < 0 ? 0 : i >= len ? len - 1 : i;
        for (int k = 0; k < 4; k++)
            sum[k] += CH(src[j], k * 8);
    }
    for (int i = 0; i < len; i++) {
        u32 r = 0;
        for (int k = 0; k < 4; k++)
            r |= (u32)(((u64)sum[k] * magic) >> 32) << (k * 8);
        dst[i * step] = r;
        int out = i - radius < 0 ? 0 : i - radius;
        int in = i + radius + 1 >= len ? len - 1 : i + radius + 1;
        for (int k = 0; k < 4; k++)
            sum[k] += CH(src[in], k * 8) - CH(src[out], k * 8);
    }
}

void img_blur(Image *img, int radius)
{
    int n = img->w > img->h ? img->w : img->h;
    u32 *line = malloc(n * 4);
    if (!line || radius < 1) {
        free(line);
        return;
    }
    for (int pass = 0; pass < 3; pass++) {
        for (int y = 0; y < img->h; y++) {
            u32 *row = img->px + y * img->w;
            memcpy(line, row, img->w * 4);
            box_pass(row, line, img->w, 1, radius);
        }
        for (int x = 0; x < img->w; x++) {
            for (int y = 0; y < img->h; y++)
                line[y] = img->px[y * img->w + x];
            box_pass(img->px + x, line, img->h, img->w, radius);
        }
    }
    free(line);
}

void img_round_corners(Image *img, float radius)
{
    for (int y = 0; y < img->h; y++) {
        for (int x = 0; x < img->w; x++) {
            float cx = x + 0.5f < radius ? radius : x + 0.5f > img->w - radius ? img->w - radius : -1;
            float cy = y + 0.5f < radius ? radius : y + 0.5f > img->h - radius ? img->h - radius : -1;
            if (cx < 0 || cy < 0)
                continue;
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float cover = radius - sqrtf(dx * dx + dy * dy) + 0.5f;
            if (cover >= 1.0f)
                continue;
            if (cover < 0)
                cover = 0;
            u32 *p = &img->px[y * img->w + x];
            *p = (*p & 0x00FFFFFF) | ((u32)(CH(*p, 24) * cover) << 24);
        }
    }
}

/* The same cut as img_round_corners, but on a finished texture, and for any radius: the corner pixels get
 * the share of them that lies inside the rounded rectangle (a pixel whose centre is d outside the arc is
 * covered 0.5 - d). It only multiplies the alpha, so it can be applied again to a texture that already has
 * rounded corners (the cut is then the smaller of the two). The extra row below the image is kept a copy
 * of the last row, as everywhere else. */
void tex_round_corners(Texture *t, float r)
{
    if (!t || t->psm != GU_PSM_8888 || r <= 0.0f || t->swizzled) /* (the pixels of a swizzled texture are not in rows) */
        return;
    int half = (t->w < t->h ? t->w : t->h) / 2;
    if (r > half)
        r = (float)half;
    int k = (int)ceilf(r) + 1; /* the corner pixels that can be cut: the arc ends r from the corner */
    if (k > half)
        k = half;
    u32 *px = t->data;
    for (int y = 0; y < k; y++) {
        for (int x = 0; x < k; x++) {
            float dx = r - (x + 0.5f), dy = r - (y + 0.5f);
            if (dx <= 0 || dy <= 0)
                continue; /* straight edge: fully inside */
            float cover = r - sqrtf(dx * dx + dy * dy) + 0.5f;
            if (cover >= 1.0f)
                continue;
            if (cover < 0)
                cover = 0;
            int xs[2] = { x, t->w - 1 - x }, ys[2] = { y, t->h - 1 - y };
            for (int i = 0; i < 2; i++) {
                for (int j = 0; j < 2; j++) {
                    u32 *p = &px[ys[j] * t->stride + xs[i]];
                    *p = (*p & 0x00FFFFFF) | ((u32)(CH(*p, 24) * cover + 0.5f) << 24);
                }
            }
        }
    }
    memcpy(px + t->h * t->stride, px + (t->h - 1) * t->stride, t->stride * sizeof(u32));
    tex_flush(t);
}

Texture *img_to_texture(const Image *img, int psm)
{
    Texture *t = tex_create(img->w, img->h, psm);
    if (!t)
        return NULL;
    for (int y = 0; y <= img->h; y++) {
        const u32 *src = img->px + (y < img->h ? y : img->h - 1) * img->w; /* extraraden = sista raden */
        if (psm == GU_PSM_8888) {
            memcpy((u32 *)t->data + y * t->stride, src, img->w * 4);
        } else {
            u16 *dst = (u16 *)t->data + y * t->stride;
            for (int x = 0; x < img->w; x++) {
                u32 c = src[x];
                dst[x] = (CH(c, 0) >> 3) | ((CH(c, 8) >> 2) << 5) | ((CH(c, 16) >> 3) << 11);
            }
        }
    }
    tex_flush(t);
    return t;
}

Texture *ntx_load(const char *path)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return NULL;
    NtxHeader h;
    Texture *t = NULL;
    if (sceIoRead(fd, &h, sizeof(h)) == sizeof(h) && h.magic == NTX_MAGIC &&
        (h.psm == GU_PSM_8888 || h.psm == GU_PSM_5650) && h.w && h.h && h.w <= 512 && h.h <= 512) {
        t = tex_create(h.w, h.h, h.psm);
        if (t && t->stride == h.stride) {
            int bpp = h.psm == GU_PSM_8888 ? 4 : 2;
            int bytes = t->stride * h.h * bpp;
            if (sceIoRead(fd, t->data, bytes) == bytes) {
                /* extraraden = kopia av sista raden */
                memcpy((u8 *)t->data + bytes, (u8 *)t->data + bytes - t->stride * bpp, t->stride * bpp);
                tex_flush(t);
            } else {
                tex_free(t);
                t = NULL;
            }
        } else {
            tex_free(t);
            t = NULL;
        }
    }
    sceIoClose(fd);
    return t;
}

int ntx_save(const char *path, const Texture *t)
{
    if (t->swizzled) /* (the file holds the rows one after the other) */
        return 0;
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0)
        return 0;
    NtxHeader h = { NTX_MAGIC, t->w, t->h, t->psm, t->stride, 0 };
    int bytes = t->stride * t->h * (t->psm == GU_PSM_8888 ? 4 : 2);
    int ok = sceIoWrite(fd, &h, sizeof(h)) == sizeof(h) && sceIoWrite(fd, t->data, bytes) == bytes;
    sceIoClose(fd);
    if (!ok)
        sceIoRemove(path);
    return ok;
}
