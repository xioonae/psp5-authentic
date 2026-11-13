/* psp5 authentic – the PSP's own H.264 decoder (on the Media Engine) for a CAVLC stream: the way PMPlayer Ultraviolet (cooleyes' player, GPL) reaches it. None of that player's code is here, only what
 * can be read in it about the order and the structures of the calls.
 *
 * The decoder of the library sceMpeg that games load (mpeg.prx) does not take what this route needs: PMPlayer Ultraviolet loads the VSH's library (flash0:/kd/mpeg_vsh.prx) beside the AV codec
 * modules, starts the Media Engine with its Baseline image (meboot.h), opens the library in its mode 4 with 2 MB of the Engine's memory (the first 64 KB are the library's, the access unit lives at
 * +0x10000), and gives it the samples of the MP4 as they are in the file with sceMpegGetAvcNalAu; sceMpegAvcDecode, sceMpegAvcDecodeDetail2 and sceMpegBaseCscAvc then give the picture as RGB.
 * Every step writes a line to the log (hwavc_log): the first run on a PSP-1000 of a version that used the game's library stopped at sceMpegCreate (0x807f00ff).
 *
 * Only for a stream the PSP's decoder in its Baseline mode may take (unit.c hw_ok: CAVLC, no B pictures); video.c falls back to the software decoder when any step fails. */
#ifndef HWAVC_H
#define HWAVC_H

typedef struct HwAvc HwAvc;

/* where the log lines go (tag, four numbers); NULL: nowhere */
void hwavc_log(void (*fn)(const char *tag, unsigned a, unsigned b, unsigned c, unsigned d));

/* The library that the route needs: the AV codec modules and flash0:/kd/mpeg_vsh.prx loaded and started. 0 when it is up (or was), else a negative number of the step that failed (-1 AV codec,
 * -2 loading the module, -3 starting it). hwlib_down() undoes only what hwlib_up() did. */
int hwlib_up(void);
void hwlib_down(void);
int hwlib_is_up(void);

/* Opens a decoder (the library must be up and the Engine started with the Baseline image): sps and pps are the NAL units as the file has them (header byte first, no start code), nal_prefix the
 * size of the length in front of each NAL unit of a sample (1 to 4). NULL when a step failed (the log says which). */
HwAvc *hwavc_open(const unsigned char *sps, int sps_len, const unsigned char *pps, int pps_len, int nal_prefix);
void hwavc_close(HwAvc *h);

/* One sample of the file (in memory aligned to 64 bytes, room for its size rounded up to 64): mode 3 for the first sample and for a key frame, else 0. When the decoder gives a picture (*pics > 0)
 * it is written as RGBA (8 bits each, GU_PSM_8888) with 512 pixels a row to rgb (aligned to 64 bytes, room for 512 x the height rounded up to 16); *w and *h are its size. 0 when it is done, a
 * negative number of the step that failed otherwise (-1 sceMpegGetAvcNalAu, -2 sceMpegAvcDecode, -3 sceMpegAvcDecodeDetail2, -4 sceMpegBaseCscAvc); *err is the library's answer. */
int hwavc_decode(HwAvc *h, void *sample, int size, int mode, void *rgb, int *pics, int *w, int *h_out, unsigned *err);

#endif
