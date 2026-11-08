/* psp5 authentic – the video engine (see video.h) */
#include <pspkernel.h>
#include <psputility.h>
#include <pspaudio.h>
#include <pspmpeg.h>
#include <pspaudiocodec.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "video.h"
#include "mp4.h"
#include "mpegps.h"
#include "music.h"
#include "system.h"

#ifdef VIDEO_DEBUG
#include <stdarg.h>
void vlog(const char *fmt, ...)
{
    char line[200];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    SceUID fd = sceIoOpen("ms0:/PSP5/video.log", PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, line, n > 0 ? (n < (int)sizeof(line) ? n : (int)sizeof(line) - 1) : 0);
        sceIoClose(fd);
    }
}
#define VLOG(...) vlog(__VA_ARGS__)
#else
#define VLOG(...) ((void)0)
#endif

/* A video that crashes the PSP leaves nothing behind but what was written before it: so the stage the player has reached is written to a small file (video-crash.txt in the PSP5 folder) before each
 * step that could hang the PSP, and the file is removed when the video is closed the right way. When the next video is opened and the file is there, it is the last one's stage: it is kept as
 * video-crash-last.txt (to be looked at, or sent to the developer) and a new one is started. No file names or other personal things are written into it. */
static void crumb_path(char *out, size_t n, const char *name) { snprintf(out, n, "%s/%s", PSP5_DATA_DIR, name); }

static void crumb(const char *what, int a, int b)
{
    char path[96], line[120];
    crumb_path(path, sizeof(path), "video-crash.txt");
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd < 0)
        return;
    int n = snprintf(line, sizeof(line), "%s %d %d\n", what, a, b);
    sceIoWrite(fd, line, n > 0 && n < (int)sizeof(line) ? n : 0);
    sceIoClose(fd);
}

static void crumb4(const char *what, int a, int b, int c, int d)
{
    char path[96], line[140];
    crumb_path(path, sizeof(path), "video-crash.txt");
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd < 0)
        return;
    int n = snprintf(line, sizeof(line), "%s %d %d %d %d\n", what, a, b, c, d);
    sceIoWrite(fd, line, n > 0 && n < (int)sizeof(line) ? n : 0);
    sceIoClose(fd);
}

static void crumbh(const char *what, unsigned a, unsigned b, unsigned c, unsigned d)
{
    char path[96], line[140];
    crumb_path(path, sizeof(path), "video-crash.txt");
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
    if (fd < 0)
        return;
    int n = snprintf(line, sizeof(line), "%s %08x %08x %08x %08x\n", what, a, b, c, d);
    sceIoWrite(fd, line, n > 0 && n < (int)sizeof(line) ? n : 0);
    sceIoClose(fd);
}

/* the first 16 bytes at an address, as four words (only an address in the user's memory is read: it comes from the PSP's library and nothing says it is one) */
static void crumb_mem(const char *what, unsigned addr)
{
    unsigned q = addr & 0x1FFFFFFF;
    if (q < 0x08800000u || q + 16 > 0x0C000000u || (addr & 3)) {
        crumbh(what, addr, 0, 0, 0);
        return;
    }
    const unsigned char *b = (const unsigned char *)(uintptr_t)addr;
    crumbh(what, (unsigned)b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3], (unsigned)b[4] << 24 | b[5] << 16 | b[6] << 8 | b[7], (unsigned)b[8] << 24 | b[9] << 16 | b[10] << 8 | b[11],
           (unsigned)b[12] << 24 | b[13] << 16 | b[14] << 8 | b[15]);
}

/* The steps of the closing that crashed a PSP are never made again on it: when the crash file that a video left behind ends in the line of one of them, that step is written into video-skip.txt, and
 * the closing leaves it out from then on (the memory it would have freed is left to the PSP until psp5 ends). */
enum { TD_STOP, TD_FLUSH, TD_FREE_ES, TD_UNREG, TD_DELETE, TD_RING, TD_FINISH, TD_N };
static const char *const TD_NAME[TD_N] = { "td-stop", "td-flush", "td-free-es", "td-unregist", "td-delete", "td-ring", "td-finish" };
static unsigned s_skip;

static void skip_load(void)
{
    char path[96], buf[256];
    crumb_path(path, sizeof(path), "video-skip.txt");
    s_skip = 0;
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return;
    int n = sceIoRead(fd, buf, sizeof(buf) - 1);
    sceIoClose(fd);
    if (n <= 0)
        return;
    buf[n] = 0;
    for (int i = 0; i < TD_N; i++)
        if (strstr(buf, TD_NAME[i]))
            s_skip |= 1u << i;
}

static void skip_save(void)
{
    char path[96], buf[256];
    int n = 0;
    crumb_path(path, sizeof(path), "video-skip.txt");
    for (int i = 0; i < TD_N; i++)
        if (s_skip & (1u << i))
            n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s\n", TD_NAME[i]);
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, buf, n);
        sceIoClose(fd);
    }
}

static void crumb_start(void)
{
    char path[96], last[96], buf[512];
    crumb_path(path, sizeof(path), "video-crash.txt");
    crumb_path(last, sizeof(last), "video-crash-last.txt");
    skip_load();
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd >= 0) {
        int n = sceIoRead(fd, buf, sizeof(buf) - 1);
        sceIoClose(fd);
        if (n > 0) { /* the last line of the file of a video that did not end the right way: a step of the closing, when that is where it was */
            buf[n] = 0;
            int e = n;
            while (e > 0 && (buf[e - 1] == '\n' || buf[e - 1] == ' '))
                buf[--e] = 0;
            const char *ln = strrchr(buf, '\n');
            ln = ln ? ln + 1 : buf;
            for (int i = 0; i < TD_N; i++)
                if (!strncmp(ln, TD_NAME[i], strlen(TD_NAME[i])) && (ln[strlen(TD_NAME[i])] == ' ' || ln[strlen(TD_NAME[i])] == 0)) {
                    s_skip |= 1u << i;
                    skip_save();
                }
        }
        SceUID o = n > 0 ? sceIoOpen(last, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777) : -1;
        if (o >= 0) {
            sceIoWrite(o, buf, (int)strlen(buf));
            sceIoWrite(o, "\n", 1);
            sceIoClose(o);
        }
        sceIoRemove(path);
    }
}

static void crumb_end(void)
{
    char path[96];
    crumb_path(path, sizeof(path), "video-crash.txt");
    sceIoRemove(path);
}

/* A video that did not play (it could not be opened, the decoder gave errors, it was given up) is closed the right way, so nothing would be left of its crash file: it is kept as video-failed.txt, to
 * be sent to the developer. */
static void crumb_keep(void)
{
    char path[96], kept[96];
    crumb_path(path, sizeof(path), "video-crash.txt");
    crumb_path(kept, sizeof(kept), "video-failed.txt");
    sceIoRemove(kept);
    if (sceIoRename(path, kept) < 0)
        sceIoRemove(path);
}

/* The PSP's own AAC decoder (sceAudiocodec, codec 0x1003) crashed a real PSP on its first frame: the crash file of the first test on one ended with "first-sound", written just before that call, and the
 * picture decoder had not been asked for anything yet. PPSSPP accepts the call, and what a PSP wants in the parameter block cannot be found out without crashing one, so an MP4 is played without
 * sound until the sound is made another way. A build with -DVIDEO_HW_AAC (PPSSPP tests only) still uses the decoder. */
#ifdef VIDEO_HW_AAC
#define HW_AAC 1
#else
#define HW_AAC 0
#endif

#define PKT 2048
#define RING_PACKETS 0x100            /* 512 KB of program stream in the ring buffer */
#define FRAME_W 512                   /* the width of the buffer the pictures are decoded into (a video is at most 512 wide) */
#define NO_DATA 0x80618001u           /* sceMpegGetAvcAu: nothing to give yet */
#define PTS0 90000                    /* the first time stamp of a PSMF (what the PSP's library expects of it) */
#define PCM_FRAMES 32768              /* the queue of sound: 0.74 seconds at 44.1 kHz (a power of two) */
#define BLOCK 1024                    /* sound frames to the sound output at a time */
#define AAC_FRAME_BYTES 4096          /* 1024 stereo frames of 16 bits */
#define MAX_AU (320 * 1024)           /* the biggest video sample that is read (the PSP's own limit for a picture is far below that) */

enum { VK_PMF, VK_MP4 };

#define RING(v) ((SceMpegRingbuffer *)(v)->ring_mem)

struct Video {
    int kind;
    int w, h;
    float duration;
    int has_sound;
    /* a PMF */
    FILE *fp;
    unsigned stream_off, stream_size;
    unsigned char header[2048];
    struct { unsigned pts, off; } *ep; /* the entry points: time stamp and the sector they are at */
    int n_ep;
    unsigned first_ts;
    int pmf_atrac;
    /* an MP4 */
    Mp4 *mp4;
    PsWriter ps;
    unsigned char *sample, *au[2]; /* the units the program stream writer holds (the one it writes and the one after it) */
    int au_cap, au_sel;
    int inband; /* the parameter sets are in the samples of the video (the first one has them) */
    unsigned char params[512];
    int params_len;
    int vnext, anext;
    int src_eof;
    /* sceMpeg */
    int mpeg_up, lib_up;
    SceMpeg mpeg;
    unsigned char ring_mem[96] __attribute__((aligned(16))); /* the ring buffer's structure: the library writes more into it than the SDK's declaration says (the PSP's own samples use 52 bytes), so it has room */
    void *ring_data, *mpeg_mem;
    int ring_size, mpeg_size;
    SceMpegStream *s_avc, *s_atrac;
    void *es_avc, *es_atrac;
    SceMpegAu au_avc, au_atrac;
    int atrac_out;
    unsigned char *pcm_tmp;
    int have_au, atrac_first;
    double au_ts, last_ts, frame_dur; /* the time of the unit that was given (its decoding time, or what follows the one before when it has none) */
    int have_ts;
    void *frame;
    Texture pic;
    int have_pic;
    /* the sound */
    int audio_ok;
    int rate;
    SceUID athread;
    volatile int quit, paused, reset, started;
    short *pcm;
    volatile int head, tail; /* frames, counted up and masked */
    volatile unsigned played; /* frames the sound output has taken since the sound started */
    int src_reserved;
    unsigned long *codec;
    int codec_up; /* the AAC decoder has its memory of the PSP's (to be given back) */
    unsigned char *aac_in;
    short *aac_out;
    double a_base;
    int a_has_base;
    /* time */
    double clock, skip_until;
    int clock_audio;
    float idle;
    int ended, failed, no_data, dec_errors, audio_dry;
    int stream_error;
    int decoded;                      /* the picture decoder has been asked for a picture in this session (it is only stopped then) */
    int n_put, n_au, n_dec;           /* packets fed, units given, pictures asked for (for the crash file) */
    int err_total;                    /* errors of the library in all (feeding, units, decoding): the crash file is kept when there were any */
    unsigned seen[6];                 /* the first different answers of sceMpegGetAvcAu (for the crash file) */
    int n_seen, hb_lines;
    float hb, since;                  /* the time to the next sign of life in the crash file, and the time this session has gone without a picture */
    int stage; /* what this session has done already, for the crash file (bits): 1 = fed, 2 = first picture asked for, 4 = first sound asked for, 8 = first put answered, 16 = first unit seen */
};

/* the PSP's decoders are loaded once, the first time a video is opened */
static int s_modules;

static int load_modules(void)
{
    if (s_modules)
        return s_modules > 0;
    int ok = 1;
    int r = sceUtilityLoadModule(PSP_MODULE_AV_AVCODEC);
    if (r < 0 && (unsigned)r != 0x80111102u)
        ok = 0;
    r = sceUtilityLoadModule(PSP_MODULE_AV_MPEGBASE);
    if (r < 0 && (unsigned)r != 0x80111102u)
        ok = 0;
    sceUtilityLoadModule(PSP_MODULE_AV_ATRAC3PLUS); /* (the sound of a PMF; without it a PMF plays without sound) */
    s_modules = ok ? 1 : -1;
    return ok;
}

/* the PSP's AAC decoder is a module of its own (the music codecs of music.c are others); an MP4's sound is without it when it cannot be loaded */
static int load_aac_module(void)
{
    static int state; /* 0 = not tried, 1 = loaded, -1 = not */
    if (state)
        return state > 0;
    int r = sceUtilityLoadModule(PSP_MODULE_AV_AAC);
    state = r >= 0 || (unsigned)r == 0x80111102u ? 1 : -1;
    return state > 0;
}

static unsigned be32(const unsigned char *p) { return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3]; }
static unsigned long long be48(const unsigned char *p) { return ((unsigned long long)be32(p) << 16) | ((unsigned)p[4] << 8) | p[5]; }
static unsigned long long au_pts(const SceMpegAu *a) { return ((unsigned long long)a->iPtsMSB << 32) | a->iPts; }
static double au_sec(const SceMpegAu *a) { return ((double)au_pts(a) - PTS0) / 90000.0; }
/* a time stamp the PSP's library gives for a unit that has one: -1 (all ones) is none */
static int ts_valid(unsigned msb, unsigned lo) { return msb == 0 && lo != 0xFFFFFFFFu; }
static int pow2(int n) { int p = 1; while (p < n) p <<= 1; return p; }

/* ---------------------------------------------------------------------- */
/* what the ring buffer is fed with */

/* video sample i of the MP4 made into what the program stream carries, in buf: its length, or -1 when the sample is left out (it cannot be read or understood) */
static int mp4_convert(Video *v, int i, int *key, unsigned char *buf)
{
    int size = mp4_video_size(v->mp4, i);
    float dts, pts;
    *key = 0;
    if (size <= 0 || size > v->au_cap)
        return -1;
    mp4_video_times(v->mp4, i, &dts, &pts, key);
    if (mp4_video_read(v->mp4, i, v->sample, v->au_cap) != size)
        return -1;
    /* a stream that carries its parameter sets in its samples is left as it is (see mpegps_sample_has_params); the others get the ones of the MP4's header on every key frame */
    int l = mpegps_annexb(v->sample, size, mp4_nal_size(v->mp4), v->inband ? NULL : v->params, v->inband ? 0 : v->params_len, *key, buf, v->au_cap + 4096);
    if (l > 0)
        *key = mpegps_unit_has_sps(buf, l); /* (what starts a group is a unit with a sequence header in it) */
    return l;
}

/* the size of unit j for the planning of a group (it is made into a buffer that nothing holds then) */
static int mp4_size(void *arg, int j, int *key)
{
    Video *v = arg;
    return mp4_convert(v, j, key, v->au[0]);
}

/* the next unit for the program stream writer: it takes turns with two buffers */
static int mp4_pull(void *arg, const uint8_t **data, int *len, int64_t *pts, int64_t *dts)
{
    Video *v = arg;
    while (v->vnext < mp4_video_count(v->mp4)) {
        int i = v->vnext++, key;
        unsigned char *buf = v->au[v->au_sel];
        int l = mp4_convert(v, i, &key, buf);
        if (l < 0)
            continue;
        float d, p;
        mp4_video_times(v->mp4, i, &d, &p, &key);
        *data = buf;
        *len = l;
        *pts = (int64_t)((double)p * 90000.0) + PTS0;
        *dts = (int64_t)((double)d * 90000.0) + PTS0;
        v->au_sel ^= 1;
        return 1;
    }
    return 0;
}

static SceInt32 ring_cb(ScePVoid data, SceInt32 packets, ScePVoid param)
{
    Video *v = param;
    if (v->kind == VK_PMF) {
        size_t got = fread(data, 1, (size_t)packets * PKT, v->fp);
        sceKernelDcacheWritebackInvalidateRange(data, (unsigned)((got / PKT) * PKT));
        return (SceInt32)(got / PKT);
    }
    int n = 0;
    while (n < packets && ps_sector(&v->ps, (unsigned char *)data + (size_t)n * PKT))
        n++;
    /* the sectors were written by the CPU, into its cache: the decoder of the PSP (which is another processor) reads the memory, so they are written out to it */
    sceKernelDcacheWritebackInvalidateRange(data, (unsigned)n * PKT);
    return n;
}

/* ---------------------------------------------------------------------- */
/* the sound */

static int rate_ok(int rate)
{
    static const int R[] = { 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000 };
    for (unsigned i = 0; i < sizeof(R) / sizeof(R[0]); i++)
        if (R[i] == rate)
            return 1;
    return 0;
}

static int pcm_count(const Video *v) { return (v->head - v->tail) & 0x7FFFFFFF; }

static void pcm_push(Video *v, const short *frames, int n)
{
    for (int i = 0; i < n; i++) {
        int at = (v->head + i) & (PCM_FRAMES - 1);
        v->pcm[at * 2] = frames[i * 2];
        v->pcm[at * 2 + 1] = frames[i * 2 + 1];
    }
    __sync_synchronize();
    v->head += n;
}

static int audio_thread(SceSize args, void *argp)
{
    Video *v = *(Video **)argp;
    short *block = memalign(64, BLOCK * 4);
    while (!v->quit && block) {
        if (v->reset) {
            v->tail = v->head;
            v->played = 0;
            __sync_synchronize();
            v->reset = 0;
            continue;
        }
        if (v->paused || !v->started || pcm_count(v) < BLOCK) {
            sceKernelDelayThread(4000);
            continue;
        }
        for (int i = 0; i < BLOCK; i++) {
            int at = (v->tail + i) & (PCM_FRAMES - 1);
            block[i * 2] = v->pcm[at * 2];
            block[i * 2 + 1] = v->pcm[at * 2 + 1];
        }
        v->tail += BLOCK;
        sceAudioSRCOutputBlocking(PSP_AUDIO_VOLUME_MAX, block);
        v->played += BLOCK;
    }
    free(block);
    return 0;
}

static int audio_start(Video *v, int rate)
{
    if (!rate_ok(rate))
        return 0;
    for (int i = 0; i < 100 && music_state() != MUSIC_STOPPED; i++)
        sceKernelDelayThread(10000); /* the music thread gives its sound channel back when it has stopped */
    int r = -1;
    for (int i = 0; i < 40; i++) {
        r = sceAudioSRCChReserve(BLOCK, rate, 2);
        if (r >= 0)
            break;
        sceKernelDelayThread(25000);
    }
    if (r < 0)
        return 0;
    v->src_reserved = 1;
    v->rate = rate;
    v->pcm = memalign(64, (size_t)PCM_FRAMES * 4);
    if (!v->pcm)
        return 0;
    v->athread = sceKernelCreateThread("psp5_vsound", audio_thread, 0x1E, 0x4000, 0, NULL);
    if (v->athread < 0)
        return 0;
    Video *self = v;
    sceKernelStartThread(v->athread, sizeof(self), &self);
    return 1;
}

static void audio_stop(Video *v)
{
    v->quit = 1;
    if (v->athread > 0) {
        sceKernelWaitThreadEnd(v->athread, NULL);
        sceKernelDeleteThread(v->athread);
        v->athread = 0;
    }
    if (v->src_reserved) {
        for (int i = 0; i < 100 && sceAudioSRCChRelease() < 0; i++)
            sceKernelDelayThread(2000);
        v->src_reserved = 0;
    }
    free(v->pcm);
    v->pcm = NULL;
}

/* the AAC decoder of the PSP (sceAudiocodec, codec 0x1003) for an MP4 */
static int aac_open(Video *v)
{
    if (!load_aac_module())
        return 0;
    /* whole cache lines each (the decoder's own processor writes into the structure, and the CPU must not write a stale line of it back over that) */
    v->codec = memalign(64, 512);
    v->aac_in = memalign(64, 8192 + 64);
    v->aac_out = memalign(64, AAC_FRAME_BYTES + 64);
    if (!v->codec || !v->aac_in || !v->aac_out)
        return 0;
    memset(v->codec, 0, 512);
    memset(v->aac_in, 0, 8192 + 64);
    memset(v->aac_out, 0, AAC_FRAME_BYTES + 64);
    v->codec[10] = (unsigned long)mp4_audio_rate(v->mp4); /* the sample rate, at offset 0x28 */
    sceKernelDcacheWritebackInvalidateAll();
    if (sceAudiocodecCheckNeedMem(v->codec, PSP_CODEC_AAC) < 0)
        return 0;
    sceKernelDcacheWritebackInvalidateAll();
    if (sceAudiocodecGetEDRAM(v->codec, PSP_CODEC_AAC) < 0)
        return 0;
    sceKernelDcacheWritebackInvalidateAll();
    if (sceAudiocodecInit(v->codec, PSP_CODEC_AAC) < 0) {
        sceAudiocodecReleaseEDRAM(v->codec);
        return 0;
    }
    sceKernelDcacheWritebackInvalidateAll();
    v->codec_up = 1;
    return 1;
}

static void aac_close(Video *v)
{
    if (v->codec && v->codec_up)
        sceAudiocodecReleaseEDRAM(v->codec);
    v->codec_up = 0;
    free(v->codec);
    free(v->aac_in);
    free(v->aac_out);
    v->codec = NULL;
    v->aac_in = NULL;
    v->aac_out = NULL;
}

/* one AAC frame of the MP4: 1 when it gave sound */
static int decode_aac(Video *v)
{
    if (v->anext >= mp4_audio_count(v->mp4)) {
        v->src_eof |= 2; /* (the sound is at its end) */
        return 0;
    }
    int i = v->anext++;
    int cfg_len;
    const unsigned char *cfg = mp4_audio_config(v->mp4, &cfg_len);
    int size = mp4_audio_size(v->mp4, i);
    if (size <= 0 || size > 8000 || mp4_audio_read(v->mp4, i, v->aac_in + 7, 8192 - 7) != size || !mpegps_adts(cfg, cfg_len, size, v->aac_in))
        return 1; /* a frame that cannot be read is skipped: the sound goes on with the next */
    if (!v->a_has_base) {
        v->a_base = mp4_audio_time(v->mp4, i);
        v->a_has_base = 1;
    }
    v->codec[6] = (unsigned long)v->aac_in;
    v->codec[7] = (unsigned long)(size + 7);
    v->codec[8] = (unsigned long)v->aac_out;
    v->codec[9] = AAC_FRAME_BYTES;
    if (!(v->stage & 4)) {
        v->stage |= 4;
        crumb("first-sound", size, 0);
    }
    sceKernelDcacheWritebackInvalidateAll(); /* the structure, the frame and the place for the sound all go out to the memory the decoder reads, and nothing of them stays in the cache of the CPU */
    int r = sceAudiocodecDecode(v->codec, PSP_CODEC_AAC);
    sceKernelDcacheWritebackInvalidateAll();
    if (r < 0) {
        VLOG("aac decode %08x\n", r);
        if (++v->dec_errors > 40)
            v->audio_ok = 0; /* it does not work: the video goes on without sound */
        return 1;
    }
    pcm_push(v, v->aac_out, 1024);
    return 1;
}

/* one audio unit of a PMF */
static int decode_atrac(Video *v)
{
    SceInt32 unk = 0;
    int r = sceMpegGetAtracAu(&v->mpeg, v->s_atrac, &v->au_atrac, &unk);
    if (r != 0) {
        if ((v->src_eof & 1) && ++v->audio_dry > 30)
            v->src_eof |= 2; /* the stream is read to its end and no more sound comes out of it */
        return 0; /* nothing yet (NO_DATA) */
    }
    v->audio_dry = 0;
    if (!v->a_has_base && au_sec(&v->au_atrac) < v->skip_until - 0.03)
        return 2; /* a jump: the sound before the time that was asked for is taken out of the stream and not played */
    if (!v->a_has_base) {
        v->a_base = au_sec(&v->au_atrac);
        v->a_has_base = 1;
    }
    r = sceMpegAtracDecode(&v->mpeg, &v->au_atrac, v->pcm_tmp, v->atrac_first);
    v->atrac_first = 0;
    if (r != 0) {
        if (++v->dec_errors > 40)
            v->audio_ok = 0;
        return 1;
    }
    sceKernelDcacheWritebackInvalidateRange(v->pcm_tmp, v->atrac_out);
    pcm_push(v, (const short *)v->pcm_tmp, v->atrac_out / 4);
    return 1;
}

/* keeps the queue of sound at about half a second */
static void pump_audio(Video *v)
{
    if (!v->audio_ok || !v->pcm)
        return;
    for (int guard = 0, skipped = 0; guard < 8 && skipped < 600 && pcm_count(v) < v->rate / 2 && pcm_count(v) + 2048 < PCM_FRAMES; guard++) {
        int got = v->kind == VK_MP4 ? decode_aac(v) : decode_atrac(v);
        if (!got)
            break;
        if (got == 2) { /* skipped, not decoded: does not count as one of the units of this round */
            guard--;
            skipped++;
            continue;
        }
        if (!v->started && pcm_count(v) >= v->rate / 4) { /* enough to start with */
            v->started = 1;
            v->clock_audio = 1;
            VLOG("sound starts: %d frames, at %.3f s\n", pcm_count(v), v->a_base);
        }
    }
    if (!v->started && (v->src_eof & 2) && pcm_count(v) > 0)
        v->started = 1, v->clock_audio = 1;
}

/* ---------------------------------------------------------------------- */
/* the session of sceMpeg */

static void make_header(Video *v, unsigned char *h)
{
    /* what the PSP's library reads of the head of a PSMF: "PSMF0014", where the stream starts (the second sector) and how long it is, the first and last time stamp, one video stream with its size in
     * blocks of 16. Copied from a PMF of the PSP's own, with the numbers of this video put in. */
    static const unsigned char T[0x92] = {
        0x50, 0x53, 0x4d, 0x46, 0x30, 0x30, 0x31, 0x34, 0x00, 0x00, 0x08, 0x00, 0x00, 0x04, 0x08, 0x00, [0x50] = 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x01, 0x5f, 0x90, 0x00, 0x00, 0x00, 0x0a, 0x14, 0x5a,
        0x00, 0x00, 0x61, 0xa8, 0x00, 0x01, 0x5f, 0x90, 0x01, 0x01, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x01, 0x5f, 0x90, 0x00, 0x00, 0x00, 0x0a, 0x14, 0x5a, 0x00, 0x01, 0x00, 0x00, 0x00, 0x12,
        0x00, 0x01, 0xe0, 0x00, 0x20, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x05
    };
    memset(h, 0, 2048);
    memcpy(h, T, sizeof(T));
    unsigned long long last = (unsigned long long)(v->duration * 90000.0) + PTS0;
    unsigned size = (unsigned)mp4_video_count(v->mp4) * PKT; /* (about: the real length is only known when the end is reached) */
    h[12] = (unsigned char)(size >> 24); h[13] = (unsigned char)(size >> 16); h[14] = (unsigned char)(size >> 8); h[15] = (unsigned char)size;
    for (int k = 0; k < 6; k++) {
        h[0x5A + k] = (unsigned char)(last >> (8 * (5 - k)));
        h[0x74 + k] = (unsigned char)(last >> (8 * (5 - k)));
    }
    h[0x8E] = (unsigned char)((v->w + 15) / 16);
    h[0x8F] = (unsigned char)((v->h + 15) / 16);
}

/* one step of the closing: its line goes into the crash file first, and it is left out when it crashed this PSP before (see TD_NAME) */
#define TD_STEP(step) (crumb(TD_NAME[step], 0, 0), (s_skip & (1u << (step))) ? (crumb("td-skipped", step, 0), VLOG("closing: %s left out\n", TD_NAME[step]), 0) : 1)

static void session_destroy(Video *v)
{
    if (!v->mpeg_up)
        return;
    SceInt32 stop_status = 0;
    /* the end of the decoding and the flush belong to a decoder that has been used: a PSP died in them when it was closed before it had got a single picture */
    if (v->decoded) {
        if (TD_STEP(TD_STOP))
            sceMpegAvcDecodeStop(&v->mpeg, FRAME_W, &v->frame, &stop_status);
        if (TD_STEP(TD_FLUSH))
            sceMpegFlushAllStream(&v->mpeg);
    }
    v->decoded = 0;
    if (v->es_atrac)
        free(v->es_atrac);
    if (v->es_avc && TD_STEP(TD_FREE_ES))
        sceMpegFreeAvcEsBuf(&v->mpeg, v->es_avc);
    /* A PSP died in sceMpegUnRegistStream when it was given the handle itself, as the SDK's header says (the first test, with a clean session of the PSP's own PMF): like all the other calls it takes
     * the address of the handle. After decode errors the stream is left registered (sceMpegDelete ends it). */
    if (v->dec_errors > 0 || v->failed) {
        crumb("td-unregist-skipped", v->dec_errors, v->failed);
    } else if (TD_STEP(TD_UNREG)) {
        if (v->s_avc)
            sceMpegUnRegistStream((SceMpeg)&v->mpeg, v->s_avc);
        if (v->s_atrac)
            sceMpegUnRegistStream((SceMpeg)&v->mpeg, v->s_atrac);
    }
    if (TD_STEP(TD_DELETE))
        sceMpegDelete(&v->mpeg);
    if (TD_STEP(TD_RING))
        sceMpegRingbufferDestruct(RING(v));
    crumb("td-done", 0, 0);
    free(v->pcm_tmp);
    v->es_atrac = v->es_avc = NULL;
    v->pcm_tmp = NULL;
    v->s_avc = v->s_atrac = NULL;
    v->mpeg_up = 0;
}

static int session_create(Video *v)
{
    if (!v->ring_data || !v->mpeg_mem)
        return 0;
    crumb("session", 0, 0);
    int rc = sceMpegRingbufferConstruct(RING(v), RING_PACKETS, v->ring_data, v->ring_size, ring_cb, v);
    VLOG("construct %08x\n", rc);
    if (rc < 0) {
        crumbh("fail-construct", (unsigned)rc, 0, 0, 0);
        return 0;
    }
    crumb("mpeg-create", v->mpeg_size, v->ring_size);
    int cr = sceMpegCreate(&v->mpeg, v->mpeg_mem, v->mpeg_size, RING(v), FRAME_W, 0, 0);
    VLOG("create %08x\n", cr);
    if (cr < 0) {
        crumbh("fail-create", (unsigned)cr, 0, 0, 0);
        sceMpegRingbufferDestruct(RING(v));
        return 0;
    }
    v->mpeg_up = 1;
    SceMpegAvcMode mode = { -1, SCE_MPEG_AVC_FORMAT_5650 };
    SceInt32 off = 0, size = 0;
    crumbh("decode-mode", (unsigned)sceMpegAvcDecodeMode(&v->mpeg, &mode), 0, 0, 0);
    if (v->kind == VK_MP4)
        make_header(v, v->header);
    int qo = sceMpegQueryStreamOffset(&v->mpeg, v->header, &off), qs = sceMpegQueryStreamSize(v->header, &size);
    VLOG("query offset %08x (%d) size %08x (%d)\n", qo, (int)off, qs, (int)size);
    if (qo < 0 || qs < 0) {
        crumbh("fail-query", (unsigned)qo, (unsigned)qs, (unsigned)off, (unsigned)size);
        goto fail_up;
    }
    v->s_avc = sceMpegRegistStream(&v->mpeg, 0, 0);
    VLOG("regist avc %p\n", (void *)v->s_avc);
    if (!v->s_avc) {
        crumb("fail-regist", 0, 0);
        goto fail_up;
    }
    if (v->kind == VK_PMF && v->pmf_atrac && v->audio_ok)
        v->s_atrac = sceMpegRegistStream(&v->mpeg, 1, 0);
    v->es_avc = sceMpegMallocAvcEsBuf(&v->mpeg);
    int ia = v->es_avc ? sceMpegInitAu(&v->mpeg, v->es_avc, &v->au_avc) : -1;
    VLOG("es %p init au %08x\n", v->es_avc, ia);
    crumbh("setup", (unsigned)(uintptr_t)v->s_avc, (unsigned)(uintptr_t)v->es_avc, (unsigned)ia, (unsigned)v->au_avc.iEsBuffer);
    if (ia < 0) {
        crumb("fail-initau", ia, 0);
        goto fail_up;
    }
    if (v->s_atrac) {
        SceInt32 es = 0, out = 0;
        if (sceMpegQueryAtracEsSize(&v->mpeg, &es, &out) < 0 || es <= 0 || out <= 0)
            goto fail_up;
        v->es_atrac = memalign(64, (size_t)es);
        v->pcm_tmp = memalign(64, (size_t)out);
        v->atrac_out = out;
        if (!v->es_atrac || !v->pcm_tmp || sceMpegInitAu(&v->mpeg, v->es_atrac, &v->au_atrac) < 0)
            goto fail_up;
    }
    return 1;
fail_up:
    session_destroy(v);
    return 0;
}

/* tops the ring buffer up from the source */
static void feed(Video *v)
{
    if (v->src_eof & 1)
        return;
    int free_packets = sceMpegRingbufferAvailableSize(RING(v));
    if (free_packets <= 0)
        return;
    int want = free_packets > 32 ? 32 : free_packets;
    if (!(v->stage & 1)) {
        v->stage |= 1;
        crumb("first-feed", want, free_packets);
    }
    int r = sceMpegRingbufferPut(RING(v), want, free_packets);
    if (!(v->stage & 8)) {
        v->stage |= 8;
        crumb4("first-put", r, want, free_packets, v->src_eof);
    }
    if (r < 0) {
        VLOG("ring put %08x\n", r);
        if (v->stream_error < 3)
            crumb("put-error", r, v->stream_error);
        v->stream_error++;
        v->err_total++;
    } else if (r == 0) {
        v->src_eof |= 1;
        crumb("source-end", v->n_put, 0);
    } else {
        v->n_put += r;
    }
}

/* ---------------------------------------------------------------------- */
/* open, start, close */

static void pmf_header_info(Video *v)
{
    const unsigned char *h = v->header;
    unsigned long long first = be48(h + 0x54), last = be48(h + 0x5A);
    v->first_ts = (unsigned)first;
    v->duration = last > first ? (float)((double)(last - first) / 90000.0) : 0;
    int streams = (h[0x80] << 8) | h[0x81];
    for (int i = 0; i < streams && i < 8; i++) {
        const unsigned char *e = h + 0x82 + 16 * i;
        if ((e[0] & 0xF0) == 0xE0 && !v->w) {
            v->w = e[12] * 16;
            v->h = e[13] * 16;
            unsigned ep_off = be32(e + 4), n = be32(e + 8);
            if (n > 0 && n < 100000 && ep_off + n * 10 <= v->stream_off) {
                /* the entry points are in the part of the header that is read in full (stream_off bytes) */
                v->n_ep = (int)n;
            }
        } else if (e[0] == 0xBD && (e[1] & 0xF0) == 0) {
            v->pmf_atrac = 1;
        }
    }
}

Video *video_open(const char *path)
{
    crumb_start();
    crumb("open", 0, 0);
    if (!load_modules()) {
        crumb("open-fail", __LINE__, 0);
        crumb_keep(); /* (it failed, it did not crash) */
        return NULL;
    }
    Video *v = calloc(1, sizeof(*v));
    if (!v) {
        crumb("open-fail", __LINE__, 0);
        crumb_keep();
        return NULL;
    }
    int mi = sceMpegInit();
    VLOG("mpeg init %08x\n", mi);
    if (mi < 0) {
        crumb("open-fail", __LINE__, mi);
        free(v);
        crumb_keep();
        return NULL;
    }
    v->lib_up = 1;
    const char *dot = strrchr(path, '.');
    int pmf = dot && (!strcasecmp(dot, ".pmf") || !strcasecmp(dot, ".psmf"));
    v->frame_dur = 1001.0 / 30000.0; /* (a PMF: the usual 29.97; an MP4 sets its own below) */
    if (pmf) {
        v->kind = VK_PMF;
        v->fp = fopen(path, "rb");
        if (!v->fp || fread(v->header, 1, PKT, v->fp) != PKT || memcmp(v->header, "PSMF", 4))
            { crumb("open-fail", __LINE__, 0); goto fail; }
        v->stream_off = be32(v->header + 8);
        v->stream_size = be32(v->header + 12);
        if (v->stream_off < PKT || (v->stream_off & (PKT - 1)) || v->stream_off > (1 << 20))
            { crumb("open-fail", __LINE__, 0); goto fail; }
        /* the whole head of the file (the entry points can be after the first sector) */
        unsigned char *whole = malloc(v->stream_off);
        if (!whole)
            { crumb("open-fail", __LINE__, 0); goto fail; }
        fseek(v->fp, 0, SEEK_SET);
        if (fread(whole, 1, v->stream_off, v->fp) != v->stream_off) {
            free(whole);
            { crumb("open-fail", __LINE__, 0); goto fail; }
        }
        const unsigned char *e0 = NULL;
        int streams = (whole[0x80] << 8) | whole[0x81];
        for (int i = 0; i < streams && i < 8; i++)
            if ((whole[0x82 + 16 * i] & 0xF0) == 0xE0)
                e0 = whole + 0x82 + 16 * i;
        pmf_header_info(v);
        if (e0 && v->n_ep > 0) {
            unsigned ep_off = be32(e0 + 4);
            v->ep = malloc((size_t)v->n_ep * sizeof(*v->ep));
            if (v->ep)
                for (int i = 0; i < v->n_ep; i++) {
                    const unsigned char *x = whole + ep_off + 10 * (size_t)i;
                    v->ep[i].pts = be32(x + 2);
                    v->ep[i].off = be32(x + 6);
                }
            else
                v->n_ep = 0;
        }
        free(whole);
        v->audio_ok = v->pmf_atrac;
    } else {
        v->kind = VK_MP4;
        v->mp4 = mp4_open(path);
        if (!v->mp4)
            { crumb("open-fail", __LINE__, 0); goto fail; }
        v->w = mp4_width(v->mp4);
        v->h = mp4_height(v->mp4);
        v->duration = mp4_duration(v->mp4);
        v->frame_dur = mp4_video_count(v->mp4) > 0 && v->duration > 0 ? (double)v->duration / mp4_video_count(v->mp4) : 1001.0 / 30000.0;
        v->params_len = mp4_param_sets(v->mp4, v->params, sizeof(v->params));
        v->au_cap = mp4_video_max_size(v->mp4);
        {   /* does the first sample carry the parameter sets itself */
            int s0 = mp4_video_size(v->mp4, 0);
            unsigned char *tmp = s0 > 0 && s0 <= MAX_AU ? malloc((size_t)s0) : NULL;
            if (tmp && mp4_video_read(v->mp4, 0, tmp, s0) == s0)
                v->inband = mpegps_sample_has_params(tmp, s0, mp4_nal_size(v->mp4));
            free(tmp);
        }
        if (v->au_cap <= 0 || v->au_cap > MAX_AU)
            { crumb("open-fail", __LINE__, 0); goto fail; }
        v->sample = malloc((size_t)v->au_cap);
        v->au[0] = malloc((size_t)v->au_cap + 4096);
        v->au[1] = malloc((size_t)v->au_cap + 4096);
        if (!v->params_len || !v->sample || !v->au[0] || !v->au[1]) {
            VLOG("no memory for the samples\n");
            { crumb("open-fail", __LINE__, 0); goto fail; }
        }
        v->audio_ok = mp4_has_audio(v->mp4);
    }
    VLOG("open %s: %dx%d, %.2f s, kind %d\n", path, v->w, v->h, v->duration, v->kind);
    crumb(v->kind == VK_MP4 ? "mp4" : "pmf", v->w, v->h);
    if (v->kind == VK_MP4 && v->params_len > 8 && (v->params[4] & 0x1F) == 7) {
        crumb("avc-profile-level", v->params[5], v->params[7]);
        int n = v->params_len < 24 ? v->params_len : 24; /* the head of the parameter sets, as they were given to the decoder (profile, sizes, flags) */
        crumbh("params", (unsigned)v->params[0] << 24 | v->params[1] << 16 | v->params[2] << 8 | v->params[3], (unsigned)v->params[4] << 24 | v->params[5] << 16 | v->params[6] << 8 | v->params[7],
               n >= 12 ? (unsigned)v->params[8] << 24 | v->params[9] << 16 | v->params[10] << 8 | v->params[11] : 0, n >= 16 ? (unsigned)v->params[12] << 24 | v->params[13] << 16 | v->params[14] << 8 | v->params[15] : 0);
    }
    if (v->w <= 0 || v->h <= 0 || v->w > FRAME_W || v->h > 512)
        { crumb("open-fail", __LINE__, 0); goto fail; }
    /* the decoder writes whole blocks of 16 and the PSP's own library decides how far it goes: the buffer is always the biggest a video can need (512 by 512 of 16 bits), so nothing can be written past it */
    size_t frame_bytes = (size_t)FRAME_W * 512 * 2;
    v->frame = memalign(64, frame_bytes);
    v->ring_size = sceMpegRingbufferQueryMemSize(RING_PACKETS);
    v->mpeg_size = sceMpegQueryMemSize(0);
    if (v->ring_size > 0 && v->mpeg_size > 0) {
        v->ring_data = memalign(64, (size_t)v->ring_size);
        v->mpeg_mem = memalign(64, (size_t)v->mpeg_size);
    }
    if (!v->frame || !v->ring_data || !v->mpeg_mem) {
        VLOG("no memory for the picture and the ring buffer\n");
        { crumb("open-fail", __LINE__, 0); goto fail; }
    }
    memset(v->frame, 0, frame_bytes);
    sceKernelDcacheWritebackInvalidateRange(v->frame, (unsigned)frame_bytes);
    v->pic.w = v->w;
    v->pic.h = v->h;
    v->pic.tw = FRAME_W;
    v->pic.th = pow2(v->h);
    v->pic.stride = FRAME_W;
    v->pic.psm = GU_PSM_5650;
    v->pic.data = v->frame;
    v->pic.vram = 1; /* (the picture's memory is the engine's: gfx never frees it) */
    /* the sound: the queue and its thread, and the decoder of an MP4's AAC */
    if (v->audio_ok) {
        int rate = v->kind == VK_MP4 ? mp4_audio_rate(v->mp4) : 44100;
        crumb("sound-open", rate, v->kind == VK_MP4);
        if (v->kind == VK_MP4 && (!HW_AAC || !aac_open(v)))
            v->audio_ok = 0; /* (an MP4 is silent: see HW_AAC) */
        crumb("sound-out", rate, v->audio_ok);
        if (v->audio_ok && !audio_start(v, rate))
            v->audio_ok = 0;
    }
    v->has_sound = v->audio_ok;
    crumb("opened", v->has_sound, 0);
    return v;
fail:
    VLOG("open failed\n");
    v->failed = 1;
    video_close(v);
    return NULL;
}

void video_close(Video *v)
{
    if (!v)
        return;
    crumb("closing", 0, 0);
    v->started = 0;
    audio_stop(v);
    session_destroy(v);
    free(v->ring_data);
    free(v->mpeg_mem);
    if (v->lib_up && TD_STEP(TD_FINISH))
        sceMpegFinish();
    aac_close(v);
    crumb4("end", v->n_put, v->n_au, v->n_dec, v->err_total);
    if (v->failed || v->err_total > 0)
        crumb_keep(); /* it did not play right: the file stays (video-failed.txt) */
    else
        crumb_end(); /* it was closed the right way and played: nothing to leave behind */
    free(v->frame);
    free(v->ep);
    free(v->sample);
    free(v->au[0]);
    free(v->au[1]);
    if (v->fp)
        fclose(v->fp);
    mp4_close(v->mp4);
    free(v);
}

int video_width(const Video *v) { return v->w; }
int video_height(const Video *v) { return v->h; }
float video_duration(const Video *v) { return v->duration; }
int video_has_sound(const Video *v) { return v->has_sound; }
int video_ended(const Video *v) { return v->ended; }
int video_failed(const Video *v) { return v->failed; }
const Texture *video_picture(const Video *v) { return v->have_pic ? &v->pic : NULL; }

int video_play(Video *v, float at)
{
    if (at < 0)
        at = 0;
    if (v->duration > 0 && at > v->duration - 0.5f)
        at = v->duration > 1 ? v->duration - 1 : 0;
    /* stop what runs: the sound is emptied, the session of sceMpeg is made again */
    v->started = 0;
    v->clock_audio = 0;
    if (v->audio_ok || v->pcm) {
        v->reset = 1;
        for (int i = 0; i < 100 && v->reset && v->athread > 0; i++)
            sceKernelDelayThread(3000);
        v->head = v->tail = 0;
        v->played = 0;
    }
    session_destroy(v);
    v->src_eof = 0;
    v->have_au = 0;
    v->have_ts = 0;
    v->have_pic = 0;
    v->ended = 0;
    v->no_data = 0;
    v->audio_dry = 0;
    v->dec_errors = 0;
    v->stream_error = 0;
    v->stage = 0;
    v->decoded = 0;
    v->n_put = v->n_au = v->n_dec = 0;
    v->n_seen = v->hb_lines = 0;
    v->hb = v->since = 0;
    v->a_has_base = 0;
    v->atrac_first = 1;
    v->idle = 0;
    v->clock = at;
    v->skip_until = at;
    if (v->kind == VK_MP4) {
        v->vnext = mp4_video_at(v->mp4, at);
        v->anext = v->audio_ok ? mp4_audio_at(v->mp4, at) : 0;
        v->au_sel = 0;
        ps_init(&v->ps, v->vnext, mp4_video_count(v->mp4), mp4_size, mp4_pull, v);
    } else {
        unsigned off = 0;
        for (int i = 0; i < v->n_ep; i++) { /* the last entry point at or before the time */
            if ((double)(v->ep[i].pts - v->first_ts) / 90000.0 > at)
                break;
            off = v->ep[i].off;
        }
        if (at <= 0.01f)
            off = 0;
        fseek(v->fp, (long)(v->stream_off + (unsigned long)off * PKT), SEEK_SET);
    }
    int ok = session_create(v);
    VLOG("play at %.2f: session %d\n", at, ok);
    if (!ok)
        v->failed = 1; /* (the crash file is kept) */
    return ok;
}

void video_pause(Video *v, int paused)
{
    v->paused = paused;
}

float video_time(const Video *v)
{
    double t = v->clock;
    if (t < 0)
        t = 0;
    if (v->duration > 0 && t > v->duration)
        t = v->duration;
    return (float)t;
}

int video_update(Video *v, float dt)
{
    if (!v->mpeg_up || v->failed)
        return 0;
    if (v->paused)
        return 0;
    for (int i = 0; i < 3; i++)
        feed(v);
    pump_audio(v);
    /* the clock: the sound that has been played, or the time that has gone when there is no sound */
    if (v->audio_ok && v->clock_audio && v->a_has_base && !((v->src_eof & 2) && pcm_count(v) == 0))
        v->clock = v->a_base + (double)v->played / v->rate;
    else if (!v->audio_ok || v->idle > 1.0f || (v->started && (v->src_eof & 2))) /* (no sound, none came, or it has all been played: the time goes by itself) */
        v->clock += dt; /* (no sound, or none came in a second: the picture does not wait for it) */
    else
        v->idle += dt;
    /* signs of life in the crash file, a few seconds apart, and the end of a session that gives no picture at all (the screen would stay black for ever) */
    v->hb += dt;
    if (v->hb >= 2.0f && v->hb_lines < 40) {
        v->hb = 0;
        v->hb_lines++;
        crumb4("alive", v->n_put, v->n_au, v->n_dec, (int)(v->clock * 1000));
    }
    if (!v->have_pic && (v->since += dt) > 8.0f) {
        crumb4("no-picture", v->n_put, v->n_au, v->n_dec, v->src_eof);
        v->failed = 1;
        return 0;
    }
    int shown = 0;
    for (int n = 0; n < 4; n++) {
        if (!v->have_au) {
            SceInt32 unk = 0;
            unsigned r = (unsigned)sceMpegGetAvcAu(&v->mpeg, v->s_avc, &v->au_avc, &unk);
            if (r != 0) { /* the first different answers other than "a unit": what a PSP says when it gives none */
                int known = 0;
                for (int i = 0; i < v->n_seen; i++)
                    known |= v->seen[i] == r;
                if (!known && v->n_seen < 6) {
                    v->seen[v->n_seen++] = r;
                    crumb4("get-au", (int)r, v->n_put, v->n_au, v->src_eof);
                }
            }
            if (r == NO_DATA) {
                feed(v);
                if ((v->src_eof & 1) && ++v->no_data > 20)
                    v->ended = 1;
                break;
            }
            if (r != 0) {
                VLOG("get au %08x\n", r);
                v->err_total++;
                if (++v->stream_error > 30)
                    v->failed = 1;
                break;
            }
            v->no_data = 0;
            v->have_au = 1;
            v->n_au++;
            if (v->n_au <= 4)
                crumbh("unit-ts", v->au_avc.iPtsMSB, v->au_avc.iPts, v->au_avc.iDtsMSB, v->au_avc.iDts);
            /* The time the unit is decoded at: its decoding time. A unit that has no time stamp of its own (only about every 16th has one in the PSP's own streams) is given -1, so it is the one
             * before it and one picture later. */
            if (ts_valid(v->au_avc.iDtsMSB, v->au_avc.iDts))
                v->au_ts = ((double)v->au_avc.iDts - PTS0) / 90000.0;
            else if (ts_valid(v->au_avc.iPtsMSB, v->au_avc.iPts))
                v->au_ts = au_sec(&v->au_avc) - 0.034;
            else if (v->have_ts)
                v->au_ts = v->last_ts + v->frame_dur;
            else
                v->au_ts = v->clock;
        }
        double pts = v->au_ts;
        if (!(v->stage & 16)) {
            v->stage |= 16;
            crumb4("first-unit", (int)(pts * 1000), (int)(v->clock * 1000), (int)v->au_avc.iAuSize, (int)v->au_avc.iPtsMSB);
        }
        if (pts > v->clock + 0.004)
            break; /* not yet */
        SceInt32 got = 0;
        void *buf = v->frame;
        if (!(v->stage & 2)) {
            v->stage |= 2;
            crumb("first-picture", v->au_avc.iAuSize, 0);
            crumbh("au", v->au_avc.iEsBuffer, v->au_avc.iAuSize, v->au_avc.iPts, v->au_avc.iDts);
            crumb_mem("es-head", v->au_avc.iEsBuffer);
            crumbh("frame", (unsigned)(uintptr_t)v->frame, FRAME_W, 0, 0);
        }
        v->decoded = 1;
        int r = sceMpegAvcDecode(&v->mpeg, &v->au_avc, FRAME_W, &buf, &got);
        v->have_au = 0;
        v->n_dec++;
        v->last_ts = v->au_ts;
        v->have_ts = 1;
        if (v->n_dec == 1)
            crumbh("first-decode", (unsigned)r, (unsigned)got, v->au_avc.iAuSize, (unsigned)(uintptr_t)buf);
        if (r != 0) {
            VLOG("avc decode %08x\n", r);
            v->err_total++;
            if (v->dec_errors < 3)
                crumbh("decode-error", (unsigned)r, (unsigned)v->dec_errors, v->au_avc.iAuSize, 0);
            if (++v->dec_errors > 30)
                v->failed = 1;
            continue;
        }
        v->dec_errors = 0;
        if (got == 1 && pts + 0.034 >= v->skip_until - 0.03) {
            shown = 1;
            v->have_pic = 1;
        }
    }
    if (v->duration > 0 && v->clock > v->duration + 0.5 && (v->src_eof & 1))
        v->ended = 1;
    return shown;
}
