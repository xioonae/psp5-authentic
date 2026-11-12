/* psp5 authentic – an AAC-LC decoder in software (ISO/IEC 14496-3), for the sound of an MP4.
 *
 * The PSP's own AAC decoder (sceAudiocodec) crashed a real PSP on its first frame, and what its parameter block wants cannot be found out without crashing more of them, so the sound of an MP4
 * is made here: raw AAC frames (as an MP4 stores them) with the AudioSpecificConfig of the file in, 16-bit stereo out. Low complexity profile, 1024 samples a frame, one or two channels, the
 * tools of ordinary encoders (short blocks, M/S and intensity stereo, noise substitution, TNS, pulse data). Not supported (the call returns NULL, the video plays without sound): other profiles,
 * HE-AAC (SBR), more than two channels, 960-sample frames, coupling channels. */
#ifndef AAC_H
#define AAC_H

typedef struct AacDec AacDec;

#define AAC_FRAME 1024 /* the samples a frame gives (per channel) */

/* the decoder for a stream whose AudioSpecificConfig is cfg; NULL when it is not one this decoder can do. *rate and *channels are what the config says. */
AacDec *aac_new(const unsigned char *cfg, int cfg_len, int *rate, int *channels);
void aac_free(AacDec *d);
/* forgets the overlap of the frame before (after a jump in the stream) */
void aac_flush(AacDec *d);

/* One frame: pcm gets AAC_FRAME stereo frames (interleaved 16 bits; a mono stream is made two equal channels). 1 when the frame was decoded, 0 when it was damaged (pcm is then silent or what
 * could be made of it), so the sound goes on with the next one. */
int aac_decode(AacDec *d, const unsigned char *frame, int len, short *pcm);

#endif
