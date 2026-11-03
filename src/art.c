/* psp5 – made by Mille Weber */
/* Hämtar kort- och bakgrundsbilder. Ordning:
 *   1. covers/<nyckel>.ico|.bg  (egna omslag från PC-skriptet)
 *   2. cache/<hash>.ico|.bg     (tidigare bearbetade)
 *   3. ICON0/PIC1 ur spelet (eller UMD-skivan) → bearbetas → sparas i cache/ */
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "art.h"
#include "image.h"
#include "iso.h"
#include "umd.h"
#include "music.h"

#define MAX_PNG_SIZE (512 * 1024)
/* The cards have the corner radius w / 12 at every size (ui.c), so the picture is cut to that shape when it is
 * loaded - whatever the file has (older caches and PC covers have a smaller radius baked in, which this only
 * cuts further). */
#define ICON_RADIUS_DIV 12.0f
#define COVER_RADIUS 8.0f /* som rutan i musikfliken */
#define MAX_COVER_FILE (2 * 1024 * 1024)

static char s_cache_dir[200];
static char s_cover_dir[200];

void art_init(const char *base_dir)
{
    snprintf(s_cache_dir, sizeof(s_cache_dir), "%s/cache", base_dir);
    snprintf(s_cover_dir, sizeof(s_cover_dir), "%s/covers", base_dir);
    sceIoMkdir(s_cache_dir, 0777);
    sceIoMkdir(s_cover_dir, 0777);

    /* Square icons (PSP minis) used to be stretched to the card's shape, and the cache still holds them that way: remove the
     * cached icons once, they are made again as they are needed. (The marker has the name psp5 1.6.0 gives it, so a card
     * that has been used with that version is not cleared twice.) The covers/ folder is never touched. */
    char mark[220], path[256];
    SceIoStat st;
    snprintf(mark, sizeof(mark), "%s/icons2", s_cache_dir);
    if (sceIoGetstat(mark, &st) >= 0)
        return;
    SceUID d = sceIoDopen(s_cache_dir);
    if (d >= 0) {
        SceIoDirent ent;
        memset(&ent, 0, sizeof(ent));
        while (sceIoDread(d, &ent) > 0) {
            int n = strlen(ent.d_name);
            if (n > 4 && !strcasecmp(ent.d_name + n - 4, ".ico")) {
                snprintf(path, sizeof(path), "%s/%s", s_cache_dir, ent.d_name);
                sceIoRemove(path);
            }
            memset(&ent, 0, sizeof(ent)); /* d_private must be zero */
        }
        sceIoDclose(d);
    }
    SceUID fd = sceIoOpen(mark, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0)
        sceIoClose(fd);
}

static u32 fnv1a(const char *s, u32 h)
{
    while (*s)
        h = (h ^ (u8)*s++) * 16777619u;
    return h;
}

static void cache_path(const Entry *e, const char *ext, char *out, int size)
{
    u32 h = fnv1a(e->path, 2166136261u) ^ e->file_size ^ (e->file_time * 2654435761u) ^ (e->pboot_stamp * 2246822519u); /* (0 for a game with no PBOOT.PBP: its name stays) */
    if (e->type == ENTRY_UMD) /* alla skivor har samma sökväg */
        h = fnv1a(e->title, fnv1a(e->game_id, h));
    snprintf(out, size, "%s/%08X.%s", s_cache_dir, (unsigned)h, ext);
}

void art_forget(const Entry *e)
{
    char path[240];
    cache_path(e, "ico", path, sizeof(path));
    sceIoRemove(path);
    cache_path(e, "bg", path, sizeof(path));
    sceIoRemove(path);
}

static void cover_path(const Entry *e, const char *ext, char *out, int size)
{
    char key[96];
    entry_cover_key(e, key, sizeof(key));
    snprintf(out, size, "%s/%s.%s", s_cover_dir, key, ext);
}

/* Läser en inbäddad fil (ICON0/PIC1) ur ISO eller PBP – eller filen `umd_name` på UMD-skivan. `in_pboot`: the picture lies in the PBOOT.PBP beside the EBOOT.PBP. */
static void *read_embedded(const Entry *e, u32 offset, u32 size, const char *umd_name, int in_pboot)
{
    if (!size || size > MAX_PNG_SIZE)
        return NULL;
    void *buf = malloc(size);
    if (!buf)
        return NULL;

    int got = -1;
    char umd_path[64];
    if (e->type == ENTRY_UMD) {
        snprintf(umd_path, sizeof(umd_path), UMD_ROOT "PSP_GAME/%s", umd_name);
        SceUID fd = sceIoOpen(umd_path, PSP_O_RDONLY, 0);
        if (fd >= 0) {
            got = sceIoRead(fd, buf, size);
            sceIoClose(fd);
        }
    } else if (e->type == ENTRY_ISO) {
        IsoReader r;
        if (iso_open(&r, e->path) >= 0) {
            got = iso_read(&r, buf, size, offset);
            iso_close(&r);
        }
    } else {
        char pboot[256];
        const char *file = e->path;
        if (in_pboot) {
            entry_pboot_path(e, pboot, sizeof(pboot));
            file = pboot;
        }
        SceUID fd = sceIoOpen(file, PSP_O_RDONLY, 0);
        if (fd >= 0) {
            sceIoLseek32(fd, offset, PSP_SEEK_SET);
            got = sceIoRead(fd, buf, size);
            sceIoClose(fd);
        }
    }
    if (got != (int)size) {
        free(buf);
        return NULL;
    }
    return buf;
}

static int decode_embedded(const Entry *e, u32 offset, u32 size, const char *umd_name, int in_pboot, Image *out)
{
    void *png = read_embedded(e, offset, size, umd_name, in_pboot);
    if (!png)
        return 0;
    int ok = img_decode_png(png, size, out);
    free(png);
    return ok;
}

/* Skalar om bilden till exakt w×h om den inte redan har den storleken. */
static int fit(Image *img, int w, int h)
{
    if (img->w == w && img->h == h)
        return 1;
    Image scaled;
    if (!img_resize(img, w, h, &scaled))
        return 0;
    img_free(img);
    *img = scaled;
    return 1;
}

/* The icon in the card's shape. An icon much narrower than 144 x 80 (the square icons of PSP minis) is not stretched: it is
 * centred on a blurred, darker copy of itself that fills the space beside it. Every other icon is scaled to the card. */
static int fit_icon(Image *img)
{
    if (img->h <= 0 || img->w * 10 >= img->h * 15) /* at least 1.5 : 1 wide - an ordinary icon */
        return fit(img, ICON_W, ICON_H);
    int fw = img->w * ICON_H / img->h; /* the width the icon has at the card's height */
    Image bg, fg;
    if (fw < 1 || !img_resize(img, ICON_W, ICON_H, &bg))
        return 0;
    if (!img_resize(img, fw, ICON_H, &fg)) {
        img_free(&bg);
        return 0;
    }
    img_blur(&bg, 8);
    for (int i = 0; i < ICON_W * ICON_H; i++) { /* 55 % as bright, fully opaque */
        u32 c = bg.px[i];
        bg.px[i] = RGBA((c & 0xFF) * 55 / 100, ((c >> 8) & 0xFF) * 55 / 100, ((c >> 16) & 0xFF) * 55 / 100, 255);
    }
    int x0 = (ICON_W - fw) / 2;
    for (int y = 0; y < ICON_H; y++) {
        for (int x = 0; x < fw; x++) {
            u32 f = fg.px[y * fw + x], *b = &bg.px[y * ICON_W + x0 + x];
            u32 a = f >> 24, out = 0xFF000000;
            for (int s = 0; s < 24; s += 8)
                out |= ((((f >> s) & 0xFF) * a + ((*b >> s) & 0xFF) * (255 - a)) / 255) << s;
            *b = out;
        }
    }
    img_free(&fg);
    img_free(img);
    *img = bg;
    return 1;
}

static Texture *load_icon(const Entry *e, int round)
{
    char path[256];
    Texture *t;

    cover_path(e, "ico", path, sizeof(path));
    t = ntx_load(path);
    if (!t) {
        cache_path(e, "ico", path, sizeof(path));
        t = ntx_load(path);
    }
    if (!t) {
        Image img;
        if (!decode_embedded(e, e->icon_offset, e->icon_size, "ICON0.PNG", e->pboot_flags & ENTRY_ICON_IN_PBOOT, &img))
            return NULL;
        if (fit_icon(&img)) {
            t = img_to_texture(&img, GU_PSM_8888);
            if (t)
                ntx_save(path, t);
        }
        img_free(&img);
    }
    if (t && t->psm != GU_PSM_8888) {
        tex_free(t);
        return NULL;
    }
    if (t && round)
        tex_round_corners(t, t->w / ICON_RADIUS_DIV);
    return t;
}

Texture *art_load_icon(const Entry *e)
{
    return load_icon(e, 1);
}

Texture *art_load_icon_square(const Entry *e)
{
    return load_icon(e, 0);
}

Texture *art_load_bg(const Entry *e)
{
    char path[256];
    Texture *t;

    cover_path(e, "bg", path, sizeof(path));
    if ((t = ntx_load(path)))
        return t;
    cache_path(e, "bg", path, sizeof(path));
    if ((t = ntx_load(path)))
        return t;

    Image img;
    int blur;
    if (decode_embedded(e, e->pic_offset, e->pic_size, "PIC1.PNG", e->pboot_flags & ENTRY_PIC_IN_PBOOT, &img)) {
        blur = 0; /* PIC1 visas skarp, som en stor omslagsbild */
    } else if (decode_embedded(e, e->icon_offset, e->icon_size, "ICON0.PNG", e->pboot_flags & ENTRY_ICON_IN_PBOOT, &img)) {
        blur = 10; /* ikonen förstoras – kraftig oskärpa ger en mjuk färgyta */
    } else {
        return NULL;
    }

    if (fit(&img, BG_W, BG_H)) {
        img_blur(&img, blur);
        t = img_to_texture(&img, GU_PSM_5650);
        if (t)
            ntx_save(path, t);
    }
    img_free(&img);
    return t;
}

/* --- omslag till musik --- */

/* Bild som ligger bredvid låtarna i albummappen */
static void *folder_cover(const char *mp3_path, u32 *size)
{
    static const char *names[] = { "cover.jpg", "folder.jpg", "front.jpg", "album.jpg", "cover.png", "folder.png" };
    const char *slash = strrchr(mp3_path, '/');
    if (!slash)
        return NULL;
    char path[300];
    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        snprintf(path, sizeof(path), "%.*s/%s", (int)(slash - mp3_path), mp3_path, names[i]);
        SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
        if (fd < 0)
            continue;
        int n = sceIoLseek32(fd, 0, PSP_SEEK_END);
        void *buf = n > 0 && n <= MAX_COVER_FILE ? malloc(n) : NULL;
        if (buf) {
            sceIoLseek32(fd, 0, PSP_SEEK_SET);
            if (sceIoRead(fd, buf, n) != n) {
                free(buf);
                buf = NULL;
            }
        }
        sceIoClose(fd);
        if (buf) {
            *size = n;
            return buf;
        }
    }
    return NULL;
}

Texture *art_load_cover(const char *mp3_path, const char *album)
{
    /* ett album delar omslag (samma mapp och albumnamn); utan album gäller det låten */
    char path[256];
    const char *slash = strrchr(mp3_path, '/');
    u32 h = 2166136261u;
    if (album[0] && slash) {
        char folder[256];
        snprintf(folder, sizeof(folder), "%.*s", (int)(slash - mp3_path), mp3_path);
        h = fnv1a(album, fnv1a(folder, h));
    } else {
        h = fnv1a(mp3_path, h);
    }
    snprintf(path, sizeof(path), "%s/%08X.cov", s_cache_dir, (unsigned)h);
    Texture *t = ntx_load(path);
    if (t)
        return t;

    u32 size = 0;
    void *data = music_cover_data(mp3_path, &size);
    if (!data)
        data = folder_cover(mp3_path, &size);
    if (!data)
        return NULL;
    Image img;
    int ok = img_decode_cover(data, size, &img, COVER_SIZE);
    free(data); /* före skalningen – omslagsfilen kan vara stor */
    if (!ok)
        return NULL;
    if (img_crop_square(&img) && fit(&img, COVER_SIZE, COVER_SIZE)) {
        img_round_corners(&img, COVER_RADIUS);
        t = img_to_texture(&img, GU_PSM_8888);
        if (t)
            ntx_save(path, t);
    }
    img_free(&img);
    return t;
}
