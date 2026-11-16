/* psp5 authentic – the PSP's own keyboard (see kbd.h). Moved here from files.c (where it was osk_input) so that the text editor can use it too. */
#include <pspkernel.h>
#include <psputility.h>
#include <stdio.h>
#include <string.h>
#include "kbd.h"
#include "gfx.h"
#include "testrun.h"

#define UNITS (KBD_MAX + 24) /* the room for the text, in UTF-16 units */

static void to_utf16(const char *s, unsigned short *o, int n)
{
    int k = 0;
    while (*s && k < n - 1) {
        unsigned c = (unsigned char)*s++;
        if (c >= 0xE0 && s[0] && s[1]) {
            c = ((c & 0x0F) << 12) | ((s[0] & 0x3F) << 6) | (s[1] & 0x3F);
            s += 2;
        } else if (c >= 0xC0 && s[0]) {
            c = ((c & 0x1F) << 6) | (s[0] & 0x3F);
            s++;
        }
        o[k++] = c;
    }
    o[k] = 0;
}

static void to_utf8(const unsigned short *s, char *o, int n)
{
    int k = 0;
    for (; *s; s++) {
        unsigned c = *s;
        int len = c < 0x80 ? 1 : c < 0x800 ? 2 : 3;
        if (k + len >= n)
            break;
        if (len == 1) {
            o[k++] = c;
        } else if (len == 2) {
            o[k++] = 0xC0 | (c >> 6);
            o[k++] = 0x80 | (c & 0x3F);
        } else {
            o[k++] = 0xE0 | (c >> 12);
            o[k++] = 0x80 | ((c >> 6) & 0x3F);
            o[k++] = 0x80 | (c & 0x3F);
        }
    }
    o[k] = 0;
}

static int s_done;

static void step(void)
{
    switch (sceUtilityOskGetStatus()) {
    case PSP_UTILITY_DIALOG_VISIBLE: sceUtilityOskUpdate(1); break;
    case PSP_UTILITY_DIALOG_QUIT:    sceUtilityOskShutdownStart(); break;
    case PSP_UTILITY_DIALOG_NONE:    s_done = 1; break;
    }
}

int kbd_input(const char *desc, const char *init, char *out, int out_size, int limit, int flags, void (*draw)(void))
{
    static unsigned short desc16[64], in16[UNITS], out16[UNITS];
#ifdef PSP5_TEST
    { /* a test script (testrun.h: "text ..." / "textcancel") answers instead of the keyboard, which cannot be pressed in a script */
        int t = testrun_take_text(out, out_size);
        if (t)
            return t > 0;
    }
#endif
    if (limit < 1 || limit > KBD_MAX)
        limit = KBD_MAX;
    to_utf16(desc, desc16, 64);
    to_utf16(init, in16, UNITS);
    memset(out16, 0, sizeof(out16));
    SceUtilityOskData data;
    memset(&data, 0, sizeof(data));
    data.language = PSP_UTILITY_OSK_LANGUAGE_DEFAULT;
    data.lines = 1;
    data.unk_24 = 1;
    data.inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL;
    data.desc = desc16;
    data.intext = in16;
    data.outtextlength = UNITS;
    data.outtextlimit = limit;
    data.outtext = out16;
    SceUtilityOskParams params;
    memset(&params, 0, sizeof(params));
    params.base.size = sizeof(params);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &params.base.language);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP, &params.base.buttonSwap);
    params.base.graphicsThread = 17;
    params.base.accessThread = 19;
    params.base.fontThread = 18;
    params.base.soundThread = 16;
    params.datacount = 1;
    params.data = &data;
    if (sceUtilityOskInitStart(&params) < 0)
        return 0;
    s_done = 0;
    while (!s_done) {
        gfx_begin(RGBA(20, 20, 24, 255));
        if (draw)
            draw();
        gfx_end_with(step);
    }
    if (data.result == PSP_UTILITY_OSK_RESULT_CANCELLED)
        return 0;
    if (out16[0] || ((flags & KBD_ALLOW_EMPTY) && data.result == PSP_UTILITY_OSK_RESULT_CHANGED))
        to_utf8(out16, out, out_size);
    else
        snprintf(out, out_size, "%s", init);
    if (!(flags & KBD_KEEP_SPACES))
        while (out[0] && out[strlen(out) - 1] == ' ') /* FAT tar ändå bort mellanslag på slutet */
            out[strlen(out) - 1] = 0;
    return 1;
}
