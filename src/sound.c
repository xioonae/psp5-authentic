/* psp5 – made by Mille Weber */
/* Gränssnittsljud i stil med moderna konsolmenyer: mjuka, korta och luftiga.
 * De inbyggda ljuden syntetiseras här (sinustoner, filtrerat brus, envelopes). Finns inspelade ljud i
 * <namn>.wav bredvid EBOOT.PBP (t.ex. ms0:/PSP/GAME/PSP5/select.wav) används de i stället (load_wav) – saknas en fil spelas det inbyggda ljudet. */
#include <pspkernel.h>
#include <pspaudio.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sound.h"
#include "config.h"
#include "system.h"

#define RATE 44100
#define BLOCK 512 /* stereo-par per utmatning, ~12 ms */
#define VOICES 4
#define PI 3.14159265f
/* A sound that is started again before it has ended (holding a direction in a menu starts "move" every few tens of milliseconds, and the recording is 0.37 s
 * long) is not cut: it fades out in a straight line over this many samples (5.8 ms) while the new one plays from its start in another voice. A cut in the
 * middle of the wave is a step of up to 10 % of full scale (the recording itself never steps more than 0.7 %), and ten of them a second are a crackle. */
#define RELEASE 256

typedef struct {
    short *data;
    int len;
} Sample;

typedef struct {
    const short *data;
    int len;
    volatile int pos;
    volatile int active;
    volatile int release; /* > 0: the samples of the fade-out that are left (RELEASE at the start of it); the voice ends when it reaches 0 */
} Voice;

unsigned char g_sound_on = 1;

static Sample s_samples[SND_COUNT];
static Voice s_voices[VOICES];
static SceUID s_thread = -1;
static int s_channel = -1;
static volatile int s_quit;
static volatile int s_built;
static volatile int s_fade; /* >0: block kvar av uttoningen */
static volatile int s_boot_req; /* startljudet ska räknas fram igen */
static int s_skip_boot;         /* felsäker start: ingen startskärm */
/* två buffertar: den ena spelas medan nästa fylls – annars skrivs ljudet över
 * mitt i uppspelningen och små bitar upprepas */
static short __attribute__((aligned(64))) s_out[2][BLOCK * 2];

/* --- syntes --- */

static unsigned s_seed = 0x1234567;
static float noise(void)
{
    s_seed = s_seed * 1664525u + 1013904223u;
    return (int)(s_seed >> 9) / 4194304.0f - 1.0f;
}

/* mjuk attack, exponentiell avklingning */
static float env(float t, float attack, float tau)
{
    if (t < 0)
        return 0;
    float a = t < attack ? t / attack : 1.0f;
    return a * expf(-(t - (t < attack ? t : attack)) / tau);
}

/* sinus via tabell – sinf är för långsam för de längre ljuden */
#define SIN_N 2048
static float s_sin[SIN_N + 1];
static float tone(float t, float freq)
{
    float ph = t * freq;
    ph = (ph - floorf(ph)) * SIN_N;
    int i = (int)ph;
    if (i >= SIN_N)
        i = SIN_N - 1;
    return s_sin[i] + (s_sin[i + 1] - s_sin[i]) * (ph - i);
}

/* Fyller en buffert med dur sekunder ljud från gen(t), normaliserat till peak. */
static void render(SoundId id, float dur, float peak, float (*gen)(float t))
{
    int n = (int)(dur * RATE);
    float *tmp = malloc(n * sizeof(float));
    short *out = malloc(n * sizeof(short));
    if (!tmp || !out) {
        free(tmp);
        free(out);
        return;
    }
    float mx = 1e-6f;
    for (int i = 0; i < n; i++) {
        tmp[i] = gen((float)i / RATE);
        if (fabsf(tmp[i]) > mx)
            mx = fabsf(tmp[i]);
    }
    for (int i = 0; i < n; i++) {
        float fade = i > n - 64 ? (n - i) / 64.0f : 1.0f; /* inga klick i slutet */
        out[i] = (short)(tmp[i] / mx * peak * fade * 32767.0f);
    }
    free(tmp);
    s_samples[id].data = out;
    s_samples[id].len = n;
}

/* bläddra och byta flik: kort, ljust "tick" med en mjuk kropp under */
static float gen_move(float t)
{
    float click = noise() * env(t, 0.0004f, 0.0015f) * 0.35f;
    return tone(t, 2600) * env(t, 0.0008f, 0.006f) * 0.7f + tone(t, 1300) * env(t, 0.001f, 0.014f) * 0.5f + click;
}

/* välja, reglage, öppna meny och starta spel: två ljusa toner (C6, G6) med en liten fördröjning */
static float gen_select(float t)
{
    return tone(t, 1046.5f) * env(t, 0.003f, 0.09f) + tone(t, 1568.0f) * env(t - 0.045f, 0.003f, 0.11f) * 0.8f +
           tone(t, 3136.0f) * env(t - 0.045f, 0.002f, 0.03f) * 0.15f;
}

/* tillbaka: samma klang men nedåt (E6 → B5) och lite mörkare */
static float gen_back(float t)
{
    return tone(t, 1318.5f) * env(t, 0.003f, 0.06f) * 0.8f + tone(t, 987.8f) * env(t - 0.04f, 0.003f, 0.09f);
}

/* fel: dov, mjuk dubbelpuls */
static float gen_error(float t)
{
    float s = tone(t, 330) + tone(t, 990) * 0.25f;
    return s * (env(t, 0.004f, 0.04f) + env(t - 0.09f, 0.004f, 0.05f));
}

/* startskärmen (spelas från hälsovarningen, loggan visas i tystnad):
 * ett mjukt, luftigt ackord som sväller fram under de mörka cirklarna, en liten
 * dipp, och sedan ett skimrande svep med glittrande toner när dammet virvlar upp */
#define BOOT_SHIMMER 2.0f /* dammet, sekunder efter ljudets start */
static float s_bt_lo, s_bt_band, s_air_lo;
static const float s_ping_t[] = { 1.86f, 1.97f, 2.05f, 2.13f, 2.22f, 2.30f, 2.41f, 2.52f, 2.66f, 2.83f, 3.05f, 3.32f };
static const float s_ping_f[] = { 2093.0f, 2637.0f, 3136.0f, 2349.3f, 3520.0f, 2793.8f, 4186.0f, 3136.0f,
                                  2637.0f, 3520.0f, 2349.3f, 3136.0f };
static float gen_boot(float t)
{
    /* ackordet (A-dur med nia), varje ton i två lätt ostämda kopior */
    static const float notes[] = { 110.0f, 164.8f, 220.0f, 277.2f, 329.6f, 493.9f };
    static const float gain[] = { 0.50f, 0.45f, 0.40f, 0.30f, 0.26f, 0.14f };
    float pad_env = fminf(t / 1.3f, 1.0f);
    pad_env *= pad_env;
    if (t > 1.55f)
        pad_env *= 1.0f - 0.45f * fminf((t - 1.55f) / 0.35f, 1.0f);   /* dippen före dammet */
    if (t > BOOT_SHIMMER)
        pad_env *= 0.55f + 0.45f * expf(-(t - BOOT_SHIMMER) / 1.4f);
    pad_env *= 1.0f - fminf(fmaxf(t - 3.6f, 0) / 1.5f, 1.0f);          /* tonar ut */
    float pad = 0;
    for (int i = 0; i < 6; i++)
        pad += (tone(t, notes[i] * 1.0021f) + tone(t, notes[i] * 0.9979f)) * gain[i];
    /* luft: lågpassat brus under ackordet */
    s_air_lo += 0.04f * (noise() - s_air_lo);
    float out = (pad * 0.16f + s_air_lo * 0.9f) * pad_env;

    /* svepet: brus genom ett bandpass som sveper uppåt och klingar av */
    float d = t - (BOOT_SHIMMER - 0.35f);
    if (d > 0 && d < 3.0f) {
        float f = 700 + 4800 * fminf(d / 0.7f, 1.0f);
        float c = 2 * sinf(PI * f / RATE);
        float hi = noise() - s_bt_lo - 0.4f * s_bt_band;
        s_bt_band += c * hi;
        s_bt_lo += c * s_bt_band;
        float sw = d < 0.45f ? d / 0.45f : expf(-(d - 0.45f) / 0.55f);
        out += s_bt_band * sw * sw * 0.32f;
    }
    /* glitter */
    for (unsigned i = 0; i < sizeof(s_ping_t) / sizeof(s_ping_t[0]); i++) {
        float p = t - s_ping_t[i];
        if (p >= 0 && p < 1.4f)
            out += tone(p, s_ping_f[i]) * env(p, 0.004f, 0.28f) * 0.075f;
    }
    return out;
}

/* --- recorded sounds ---
 * <name>.wav (move, select, back, error, boot) lying next to EBOOT.PBP, e.g. ms0:/PSP/GAME/PSP5/select.wav, replaces the built-in sound of that name.
 * 16-bit PCM at 44.1 kHz, mono or stereo (stereo is mixed down - the mixer is mono). A missing or unusable
 * file is not an error: the built-in sound is used. tools/convert_sounds.py makes these files. */
#define WAV_MAX_BYTES (3 << 20) /* ~17 s of mono; the longest sound is the 5 s startup sound */
#define WAV_MIN_FRAMES 64       /* the end fade needs at least this many */
/* The recordings are about 8 dB quieter than the built-in sounds were (average level over the five
 * sounds; they keep their levels relative to each other). One gain for all of them. */
#define FILE_GAIN 2.5f

static unsigned rd16(const unsigned char *p) { return p[0] | p[1] << 8; }
static unsigned rd32(const unsigned char *p) { return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24; }
static int rds16(const unsigned char *p) { return (int)(rd16(p) ^ 0x8000u) - 0x8000; }

/* Opens the recording of that name. The places, in this order (the first file that can be opened is the one; a bad file there means the built-in sound):
 *  1. the folder psp5 was started from, next to EBOOT.PBP (g_self_dir)
 *  2. the game folder of the storage, <root>/PSP/GAME/PSP5 (psp5 started as ARK's launcher lies in ARK_01234, which has no sounds)
 *  3. <data dir>/sounds, where 1.2.0 and older had them (so an update that only replaces EBOOT.PBP keeps its sounds) */
static SceUID open_wav(const char *name)
{
    char path[256];
    SceUID fd = -1;
    if (g_self_dir[0]) {
        snprintf(path, sizeof(path), "%s/%s.wav", g_self_dir, name);
        fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    }
    if (fd < 0) {
        snprintf(path, sizeof(path), "%s/PSP/GAME/PSP5/%s.wav", g_root, name);
        fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    }
    if (fd < 0) {
        snprintf(path, sizeof(path), "%s/sounds/%s.wav", PSP5_DATA_DIR, name);
        fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    }
    return fd;
}

/* Reads the file into s_samples[id]. Returns 1 if that worked, 0 if there is no usable file (nothing changed). */
static int load_wav(SoundId id, const char *name)
{
    SceUID fd = open_wav(name);
    if (fd < 0)
        return 0;
    int size = sceIoLseek32(fd, 0, PSP_SEEK_END);
    sceIoLseek32(fd, 0, PSP_SEEK_SET);
    unsigned char *b = size >= 44 && size <= WAV_MAX_BYTES ? malloc(size) : NULL;
    int got = 0;
    while (b && got < size) {
        int n = sceIoRead(fd, b + got, size - got);
        if (n <= 0)
            break;
        got += n;
    }
    sceIoClose(fd);
    if (!b || got != size) {
        free(b); /* too small or too large, or cut off while reading */
        return 0;
    }

    /* walk the RIFF chunks: "fmt " must come first, then "data" (other chunks, e.g. LIST, are skipped) */
    unsigned ch = 0, data_off = 0, data_len = 0;
    int fmt_ok = 0;
    if (!memcmp(b, "RIFF", 4) && !memcmp(b + 8, "WAVE", 4)) {
        for (int p = 12; p + 8 <= size;) {
            unsigned len = rd32(b + p + 4);
            int body = p + 8;
            if (!memcmp(b + p, "fmt ", 4) && len >= 16 && body + 16 <= size) {
                ch = rd16(b + body + 2);
                fmt_ok = rd16(b + body) == 1 /* PCM */ && (ch == 1 || ch == 2) && rd32(b + body + 4) == RATE &&
                         rd16(b + body + 14) == 16;
            } else if (!memcmp(b + p, "data", 4)) {
                data_off = body;
                data_len = len > (unsigned)(size - body) ? (unsigned)(size - body) : len; /* cut-off file: use what is there */
                break;
            }
            if (len > (unsigned)(size - body))
                break;
            p = body + len + (len & 1); /* chunks are padded to an even size */
        }
    }
    unsigned frames = fmt_ok && data_off ? data_len / (2 * ch) : 0;
    if (frames < WAV_MIN_FRAMES) {
        free(b);
        return 0;
    }

    /* convert in place to mono + gain: sample i is written at byte 2i, which is always in front of the data
     * of every frame still to be read (data_off >= 20) */
    short *out = (short *)b;
    for (unsigned i = 0; i < frames; i++) {
        const unsigned char *s = b + data_off + i * 2 * ch;
        int m = ch == 2 ? (rds16(s) + rds16(s + 2)) / 2 : rds16(s);
        float v = m * FILE_GAIN;
        out[i] = (short)(v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : v);
    }
    for (unsigned i = frames - 64; i < frames; i++)
        out[i] = (short)(out[i] * (int)(frames - i) / 64); /* no click at the end, as in render() */
    short *fit = realloc(out, frames * sizeof(short));
    s_samples[id].data = fit ? fit : out;
    s_samples[id].len = (int)frames;
    return 1;
}

/* a built-in sound, or the recording of the same name when there is one */
static void make(SoundId id, const char *name, float dur, float peak, float (*gen)(float t))
{
    if (!load_wav(id, name))
        render(id, dur, peak, gen);
}

static void make_boot(void)
{
    if (load_wav(SND_BOOT, "boot"))
        return;
    s_bt_lo = s_bt_band = s_air_lo = 0;
    render(SND_BOOT, 5.2f, 0.40f, gen_boot);
}

static void build_sounds(void)
{
    for (int i = 0; i <= SIN_N; i++)
        s_sin[i] = sinf(2 * PI * i / SIN_N);
    /* startljudet behövs bara första gången (inte efter USB-läge o.d.) */
    static int boot_done;
    if (!boot_done && g_boot_anim && !s_skip_boot)
        make_boot();
    boot_done = 1;
    make(SND_MOVE, "move", 0.06f, 0.22f, gen_move);
    make(SND_SELECT, "select", 0.42f, 0.28f, gen_select);
    make(SND_BACK, "back", 0.32f, 0.24f, gen_back);
    make(SND_ERROR, "error", 0.26f, 0.24f, gen_error);
    s_built = 1;
}

/* --- mixer --- */

/* One block of the output: the voices added up (mono, copied to both sides), the fade-out of sound_fade_out() and the limit at full scale. */
static void mix_block(short *out)
{
    for (int i = 0; i < BLOCK; i++) {
        int mix = 0;
        for (int v = 0; v < VOICES; v++) {
            Voice *vo = &s_voices[v];
            if (!vo->active)
                continue;
            int pos = vo->pos;
            if (pos >= vo->len) {
                vo->active = 0;
                continue;
            }
            int sample = vo->data[pos];
            int rel = vo->release;
            if (rel > 0) { /* started again: down to nothing in a straight line, and then the voice is free */
                sample = sample * rel / RELEASE;
                vo->release = --rel;
                if (rel == 0)
                    vo->active = 0;
            }
            mix += sample;
            vo->pos = pos + 1;
        }
        if (s_fade > 0)
            mix = mix * (s_fade * BLOCK - i) / (24 * BLOCK);
        if (mix > 32767) mix = 32767;
        if (mix < -32768) mix = -32768;
        out[i * 2] = out[i * 2 + 1] = (short)mix;
    }
}

static int audio_thread(SceSize args, void *argp)
{
    /* ljuden räknas fram här, i bakgrunden, så att starten inte blir långsammare */
    if (!s_built)
        build_sounds();
    int buf = 0, tail = 0;
    while (!s_quit) {
        if (s_boot_req) { /* startskärmen efter viloläge – inget annat spelas då */
            make_boot();
            s_boot_req = 0;
        }
        int any = 0;
        for (int v = 0; v < VOICES; v++)
            any |= s_voices[v].active;
        if (any) {
            tail = 8; /* efter sista ljudet: några block tystnad (~0,1 s) */
        } else if (tail > 0) {
            tail--; /* kanalen ska sluta på tystnad, inte på en bit av ljudet */
        } else {
            s_fade = 0; /* inget spelas – en begärd uttoning gäller inte nästa ljud */
            sceKernelDelayThread(4000);
            continue;
        }
        short *out = s_out[buf];
        buf ^= 1;
        mix_block(out);
        if (s_fade > 0 && --s_fade == 0)
            for (int v = 0; v < VOICES; v++)
                s_voices[v].active = 0;
        sceAudioOutputPannedBlocking(s_channel, PSP_AUDIO_VOLUME_MAX, PSP_AUDIO_VOLUME_MAX, out);
    }
    return 0;
}

void sound_init(void)
{
    s_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, BLOCK, PSP_AUDIO_FORMAT_STEREO);
    if (s_channel < 0)
        return;
    s_quit = 0;
    memset(s_voices, 0, sizeof(s_voices));
    s_thread = sceKernelCreateThread("psp5_audio", audio_thread, 0x12, 0x4000, PSP_THREAD_ATTR_USER, NULL);
    if (s_thread >= 0)
        sceKernelStartThread(s_thread, 0, NULL);
}

void sound_term(void)
{
    if (s_thread >= 0) {
        s_quit = 1;
        sceKernelWaitThreadEnd(s_thread, NULL);
        sceKernelDeleteThread(s_thread);
        s_thread = -1;
    }
    if (s_channel >= 0) {
        sceAudioChRelease(s_channel);
        s_channel = -1;
    }
    for (int i = 0; i < SND_COUNT; i++) {
        free(s_samples[i].data);
        s_samples[i].data = NULL;
    }
    s_built = 0;
}

void sound_play(SoundId id)
{
    if (!g_sound_on || s_thread < 0 || !s_built || id >= SND_COUNT)
        return;
    /* The same sound again (fast scrolling): the one that is playing is not cut, it fades out (see RELEASE) and the new one gets a voice of its own. */
    for (int v = 0; v < VOICES; v++)
        if (s_voices[v].active && s_voices[v].release == 0 && s_voices[v].data == s_samples[id].data)
            s_voices[v].release = RELEASE;
    Voice *pick = NULL;
    for (int v = 0; v < VOICES && !pick; v++)
        if (!s_voices[v].active)
            pick = &s_voices[v];
    if (!pick) { /* all busy (more than four starts between two blocks): one that is fading out already, else the one that is furthest on */
        for (int v = 0; v < VOICES && !pick; v++)
            if (s_voices[v].release > 0)
                pick = &s_voices[v];
        if (!pick) {
            pick = &s_voices[0];
            for (int v = 1; v < VOICES; v++)
                if (s_voices[v].pos > pick->pos)
                    pick = &s_voices[v];
        }
    }
    /* rösten stängs först så att mixern aldrig läser en halvt bytt röst */
    pick->active = 0;
    pick->release = 0;
    pick->pos = 0;
    pick->data = s_samples[id].data;
    pick->len = s_samples[id].len;
    pick->active = 1;
}

int sound_ready(void)
{
    return (s_built && !s_boot_req) || s_thread < 0;
}

void sound_prepare_boot(void)
{
    if (g_sound_on && s_thread >= 0 && !s_samples[SND_BOOT].data)
        s_boot_req = 1;
}


void sound_skip_boot(void)
{
    s_skip_boot = 1;
}

void sound_fade_out(void)
{
    s_fade = 24; /* ~0,3 s */
}

void sound_release(SoundId id)
{
    if (id >= SND_COUNT || !s_samples[id].data)
        return;
    for (int v = 0; v < VOICES; v++)
        if (s_voices[v].data == s_samples[id].data)
            s_voices[v].active = 0;
    sceKernelDelayThread(30000); /* mixern kan vara mitt i ett block */
    free(s_samples[id].data);
    s_samples[id].data = NULL;
    s_samples[id].len = 0;
}

void sound_wait(int timeout_ms)
{
    for (int t = 0; t < timeout_ms && s_thread >= 0; t += 10) {
        int any = 0;
        for (int v = 0; v < VOICES; v++)
            any |= s_voices[v].active;
        if (!any)
            return;
        sceKernelDelayThread(10000);
    }
}

#ifdef PSP5_TEST
void sound_dump(const char *dir)
{
    static const char *names[SND_COUNT] = { "move", "select", "back", "error", "boot" };
    while (!s_built)
        sceKernelDelayThread(10000);
    sceIoMkdir(dir, 0777);
    for (int i = 0; i < SND_COUNT; i++) {
        char path[200];
        if (!s_samples[i].data)
            continue;
        snprintf(path, sizeof(path), "%s/%s.wav", dir, names[i]);
        SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
        if (fd < 0)
            continue;
        int bytes = s_samples[i].len * 2;
        unsigned hdr[11] = { 0x46464952, 36 + bytes, 0x45564157, 0x20746D66, 16, 0x00010001, RATE, RATE * 2,
                             0x00100002, 0x61746164, bytes };
        sceIoWrite(fd, hdr, sizeof(hdr));
        sceIoWrite(fd, s_samples[i].data, bytes);
        sceIoClose(fd);
    }
}
#endif
