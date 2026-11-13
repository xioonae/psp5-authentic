# Changelog

All notable changes of **psp5 authentic**, a fork of [psp5](https://github.com/08Weber/psp5), originally made by Mille Weber (08Weber) and forked by Xioonae.
Newest first. Changes that are not in a release yet are under *Unreleased*.

## Unreleased

### Media and Music

- **Software video, sixth step: the software decoders are a few percent faster (the fallback for a video the PSP's own decoder will not take; not tried on a PSP).** Where the time goes (measured by counting instructions on a PC on a busy 480 x 272 video with 3 reference frames, with the loop filter on): the loop filter 37 %, the luma motion compensation 23 %, the coefficient parser and the slice loop 18 %, the 4x4 transform 7 %, the chroma motion compensation 6 %; of the decoder thread's time on the PSP-1000 about 13 % is the sound decoder, 4 % the colours, 1 % reading, and, while the video is slowed down, 4 to 5 % of the processor the sound that keeps its pitch. Changed (the pictures stay identical to the last bit, checked on 13 streams against ffmpeg; the sound within one step of 16 bits as before): **the loop filter** has its own code for vertical and horizontal edges and for the normal and the strong filter (no address worked out for a sample; the two corrections that are clipped to zero are not made; the strength of an edge between two macroblocks with one motion each is worked out once, and the inside of a macroblock without coefficients and with one motion is skipped): 10 % fewer instructions in the filter, **the 4x4 transform** 12 % fewer, **the sound's inverse transform** uses radix-4 passes (7 % fewer instructions in the whole AAC decoder; the samples differ from before by at most one step in 555 of 819,200), and **the search of the sound that keeps its pitch** goes coarse to fine on the same grid (a third of the comparisons: 46 to 22 million instructions per 400 pieces; the pitch, length and click test passes). Together about 5 % fewer instructions with the loop filter on and 1 % with it off (PPSSPP: 24.9 to 24.0 ms per picture), and 3 % of the processor back while the video is slowed down. **This is not enough to take the hard parts of the HandBrake video to full speed on a PSP-1000**: the interpolation of the motion compensation is already at about 13 instructions a pixel and needs the vector unit (VFPU) or the Media Engine to go further, so the hardware route (`hw=1`) is still the way to full speed.
- **Software video, fifth step: a way to full speed, the PSP's own decoder in its Baseline mode (experimental, off by default, not tried on a PSP).** The software decoder cannot reach 30 pictures a second in the hard parts of the HandBrake video on a PSP-1000 (it needs 1.4 to 1.75 times the speed the processor has), so the only way to play it at full speed is to let the PSP's own decoder do it, the one that runs on the second processor (the Media Engine) and uses none of the main one. Its default firmware gives the fatal error to CAVLC, but PMPlayer Ultraviolet (cooleyes' player, GPL) starts the Engine with another image, number 4, for Baseline streams, and says that Baseline streams with one reference frame play. **New switch `hw=1` in `PSP5/video-opts.txt`:** a CAVLC stream without B pictures (Baseline, or Main as the HandBrake preset labels it) is then tried on the hardware decoder: the Engine is started with image 4 (`meboot.c`: `sceMeBootStart` of the kernel's sceMeCore driver, reached through ARK's kubridge like the volume of the OSD; only on a 6.60 / 6.61 PSP with ARK), the library is opened in its mode 4 with 2 MB of the Engine's memory, and the SPS of the stream is made a Baseline one (profile 66, constraint flags 0xC0; `hw=3` leaves the profile as it is). **If anything fails** (the function is not found, the library does not open, the first pictures give an error, no picture comes in 8 seconds) **the software decoder takes over at once from where the video is**, so the worst case is what played before. When the video is closed the Engine gets its old image back (3), so music and other videos are not affected. The crash file is kept after every video that was tried this way (`hw-try`, `hw-boot`, `hw-fallback`, `first-decode`, ... lines). Not tried on a PSP: nothing in PPSSPP can start the Engine, so only the steps around it were tested there (the way back to the software decoder from a failed start and from a failed first picture, the SPS rewrite, the choice of streams). Risk: a decoder of the Engine that hangs would freeze the PSP (then take the battery out, and do not use `hw=1`).
- **Software video, fourth round, after the third test on a PSP-1000: the crackle is gone; the video still slowed down more and more, because of a wrong rule about the loop filter, and the slowed sound was too low (not tried on a PSP).** The log (22 s of the same HandBrake video): `soft-aud 0 0 0 23` (no sound frame failed, no block of sound was late, the blocks came every 23 ms), the decoder thread used 92 % of the processor, the screen's thread 2 %, the sound's thread 1 %; the speed went from 1.00 to the floor of 0.60 in 22 seconds. **What the numbers say:** a picture takes 24 ms in the easy parts of this video and 47 ms in the hard ones, and the rule "no loop filter for a picture that is late" switched the filter on again as soon as the slowing down had made pictures not late (it costs about a third of the time of a picture), so the speed fell further, which switched it on more. **Changed:** the loop filter stays off as long as the speed is below one and for 15 seconds after (it is tried again in the easy parts); the speed comes back up by 3 % a second (was 2 %) and does not go below 70 % (was 60 %); and **the sound is slowed down without lowering its pitch** (`tscale.c`: overlap and add with a search for the best place, 1024 frame pieces, tested on tones at 1.0 to 0.7: the pitch within 1.5 %, the length 1 / speed, no click) instead of being played at a lower rate. What to expect: in the hard parts of this video the PSP decodes about 25 pictures a second even with no loop filter, so these parts play at about 80 % of their speed (with the voice as it is, only slower); the easy parts play as they are. A video that does not need to slow down is not touched. `slow=0` in `PSP5/video-opts.txt` still switches the slowing down off. The crash file is now kept after a video in software that had to slow down (before: only when pictures were late).
- **Software video, third round, after the second test on a PSP-1000 (same HandBrake video, still not smooth, the crackle still there): what the log says, and what was changed (not tried on a PSP).** The log (21 s): the decoder thread was busy all the time (decoding 17.7 s, sound 3.5 s, making the pictures ready 0.5 s, reading 0.3 s of 21.8 s), **about 25 pictures a second against the 30 of the video**, the loop filter was left out of 494 of its 553 pictures, one jump (2.0 s late). **The sound never ran dry** (`soft-ms` ... 0) and its clock ran true (2 seconds of sound for every 2 seconds of the screen), so the crackle is not the sound queue running empty. Reading the file was never the problem (0.3 s in all). What the processor is short of is simply time: a picture takes about 25 ms with no loop filter, the sound decoder 3.7 ms for every 23 ms of sound (this includes the time other threads take), and the screen loop runs 60 times a second. **Changed:** the sound decoder is faster (Huffman codes by table, the FFT without its permutation pass: 15 % fewer instructions); the screen loop runs at half speed (a picture every second vertical blank) while a video is decoded in software; the sound thread writes two blocks in turn (a block is never written over right after it was given to the output); a video that is 1 second late (it was 2) goes on at a key frame that is due or comes within a second (the picture stands still till then); the CAVLC level prefix is counted with one instruction. **New in the log** (every 2 s): `soft-aud <sound frames that could not be decoded> <line of aac.c that said so> <blocks of sound later than 34 ms> <longest time between two blocks in ms (23 is right)>` and `soft-cpu <processor ms of the screen's thread> <of the decoder's> <of the sound's> <ms since the start>`; and with `pcm=1` in `PSP5/video-opts.txt` the first 8 seconds of the sound as it was given to the output are written to `PSP5/video-pcm.raw` when the video is closed (raw 16 bit stereo, 44.1 kHz): **please send `video-failed.txt` and, with that switch on, `video-pcm.raw`**: it shows whether the crackle is in the sound as it is decoded (then it can be found and fixed here) or only in what the PSP's output does with it. **And the main change, because the processor is the limit: the video now slows down instead of falling behind.** A video that is decoded in software is played at the speed the PSP can decode it: when the pictures are late the sound is played slower (resampled with a linear filter, the pitch goes down by the same amount) and the clock goes slower with it, so the picture and the sound stay together and nothing is skipped; when the pictures have caught up the speed goes back up by 2 % a second, to one (a video the PSP can decode in time plays as it is). A 30 picture video that the PSP decodes at 27 pictures a second plays at about 90 %. In PPSSPP, with a slower processor made on purpose (`burn=25`), the speed went from 1.0 to 0.85 in 8 seconds and stayed there, with no jump and no clicks in the sound. The lowest speed is 60 %; a video that is more than a second late (after a jump, or a stall) still goes on at a key frame. `slow=0` in `PSP5/video-opts.txt` switches this off. The log has `soft-spd <speed now> <slowest> <late pictures> <pictures without loop filter>` (1000 = as it is) and `soft-prio` (the priorities of the three threads, once).
- **Software video, second round, after the first test on a PSP-1000: the HandBrake video played, but laggy and with crackling sound; the pictures and the sound are now made on a thread of their own, and the colours by the graphics chip (not tried on a PSP).** The log of the first test (`video-failed.txt`, 18 seconds of a 480 x 272 HandBrake video with 3 reference frames): a picture took **25 ms to decode on average even though the loop filter was left out of 355 of its 377 pictures**, nearly every picture was late (the time the video is behind the sound), and it jumped ahead twice (63 and 86 pictures skipped, to the next key frame). What cost the rest of the time, found by reading the code: (1) everything ran in the loop that draws the screen, and a picture that takes more than a sixtieth of a second makes the loop wait for the next one, so about a tenth of the time went to waiting; (2) the sound and the pictures were read from the Memory Stick through **one file handle with one buffer**, so each time the player went from a sound frame to a picture and back the buffer was thrown away and 16 KB read again; (3) turning a picture into colours on the CPU cost about as much as decoding it (10 ms against 12.7 ms in PPSSPP for the test video); (4) the sound was filled up only once in each turn of that loop. Now: **a thread of its own** (`psp5_vdec`, below the screen's thread) reads the file, decodes the sound and the pictures and keeps the sound filled, three pictures (on the screen, ready, being made) change hands without waiting; **the sound has a file handle of its own** (`mp4.c`); **the graphics chip makes the colours** (`gfx_yuv()` in `gfx.c`: the three planes are 8 bit textures with palettes, drawn in five passes on top of each other with the BT.601 factors; the CPU only copies the planes, 0.4 ms against 10 ms; PPSSPP draws the colour bars of the test video the same as the CPU version, green and cyan and yellow within a few steps at most for a very bright pixel with a lot of chroma; `PSP5/video-opts.txt` with `ge=0` brings the CPU version back); the decoder files are compiled with `-O3` (a picture 8 % faster in PPSSPP). The log has `soft-ms <ms reading the file> <ms making the colours> <ms decoding sound> <times the sound ran dry>` after every `alive` line. What this should do on the PSP: the decoding itself still takes what it took (about 25 ms for that video without the loop filter; with it, more), so a video like that may still not reach 30 pictures a second with the loop filter on; the pictures that are late are still made without it. **Please send `video-failed.txt` again** (it is kept when a video in software was late with more than one picture in five).
- **Baseline (CAVLC) videos such as HandBrake's PSP preset play, and so does the sound of an MP4: psp5 decodes them itself, in software (not tried on a PSP).** The PSP's own decoder (the one `sceMpeg` reaches) only took Main profile with CABAC and one or two reference frames: a Baseline stream gave its fatal error whatever its header said, three reference frames gave decode errors, and its AAC decoder crashed a real PSP. Headers cannot change that, so two decoders were written from the standards and are used when the PSP's cannot do it: **`aac.c`** (AAC-LC, mono or stereo, 8 to 48 kHz: short blocks, TNS, intensity and mid/side stereo, noise substitution, pulse data; the sound of every MP4 now) and **`h264dec.c` / `h264rec.c` / `h264lf.c`** (H.264 CAVLC, I and P slices, several slices to a picture, any number of reference frames with the list changes and the long-term tools, the loop filter, cropping; `h264tab.inc` and `aactab.inc` are the standards' code tables). **Checked on a PC against ffmpeg:** the sound of ffmpeg's own AAC encoder (the one HandBrake uses) comes out within one step of 16 bits (9 streams: short blocks, TNS, intensity, mid/side, noise, mono, 8 / 22 / 44.1 / 48 kHz); 19 x264 and HandBrake-preset streams (3 and 16 reference frames, slices, constrained intra, loop filter settings, quality from qp 1 to crf 48, intra only, cropping) give **every picture identical to the last bit**; damaged samples and parameter sets give errors, never a wrong memory access (AddressSanitizer / UBSan fuzzing). Streams it cannot do are said so and refused: CABAC (the PSP's decoder is the one for those), B slices, interlaced coding, slice groups, weighted prediction, the 8x8 transform. **Speed is the open question** (measured in PPSSPP only, whose clock counts about one instruction a cycle, so a PSP is slower): a 480 x 272 picture takes about 12 ms in PPSSPP for a light video and 27 ms for a busy one with 1 Mbit/s (9 million instructions), and turning it into RGB costs about the same again (only the blocks that changed are converted). When a picture is late it is made without the loop filter, when the video is 2 seconds behind it goes on at the next key frame, the pictures that are not shown are not converted, and the sound keeps its time and is the clock. A jump (← →, L / R) decodes from the key frame before it, with the loop filter left out for all but the last half second, or goes to the key frame after when that is closer and the run up would be longer than two seconds. The crash file has a line `alive <microseconds a picture took> ...` every two seconds and `soft-end` at the end: **send `video-failed.txt` / `video-crash-last.txt` if a video is too slow on your PSP** (the file is kept after a video in software that was late with more than one picture in five). A video that is decoded in software does not touch the PSP's video library at all (so it also plays after that library's fatal error). Test switch `PSP5/video-opts.txt`: `soft=1` for the software decoder for every stream, `soft=0` for none.
- **The first x264 videos play on a PSP, with the PSP's timing written in: Main profile, CABAC, one or two reference frames. Baseline (CAVLC) is refused by the decoder, so psp5 now refuses it, and High profile, before the decoder can break.** R1 (Main, CABAC, 1 reference frame) and R3 (Main, CABAC, 2 reference frames) played perfectly on the maintainer's PSP: the timing and the Baseline-to-Main relabel of the last builds work. R2 (Baseline, CAVLC, 1 reference frame, relabelled Main) gave the decoder's fatal error `0x80628002` on its first picture, which breaks the library for every video after it until psp5 is started again; so **CAVLC is what the decoder refuses** (the relabel does not help), and the maintainer's video (HandBrake PSP preset) is CAVLC and has 3 reference frames. An MP4 whose PPS says CAVLC, or whose SPS is of a High profile (the PSP's own player says "corrupted" for those), is now refused at once with the usual *This video can't be played* and a line in `video-failed.txt` (`unsupported 1` = CAVLC, `2` = High), before the decoder is asked anything (a file `video-opts.txt` with `force=1` in it tries it anyway; PPSSPP builds for the tests do not refuse). **Not known whether 3 reference frames work:** M1 (Main, CABAC, 3 reference frames, weighted prediction on) failed, R3 (2, no weighted prediction) played; two new test files separate the two. To play a CAVLC video the entropy coding would have to be converted to CABAC (a large piece of work that can be checked on a PC picture for picture), or the decoder of the PSP's own player used (no layout known). The README and INSTALL.md say what plays now.
- **What the PSP's decoder takes is narrowed down further: it has only decoded streams with ONE reference frame so far; psp5 now logs what it gives the decoder (not a fix).** The newest logs: the maintainer's video relabelled as Main, a plain Main stream with the PSP's timing written in (M1) and a Main stream with CAVLC and B-frames (M2, which the XMB refuses too) all failed on the first picture again. Comparing the headers of every stream that has been tried: the streams whose first picture decoded (the PSP's own movie, psp5's copy of it, plain Main V1) all have `max_num_ref_frames` 1 and one default reference picture in the PPS; the ones that fail (M1, the HandBrake clones, the maintainer's video) have 3 (x264's default for its `medium` preset and the PSP preset). Pictures that predict from three earlier ones cannot be made to look like pictures that predict from one by changing the headers, so if that is the reason, an MP4 has to be made with one reference frame for this route to work, and playing the others would need another way into the decoder. To find out, three new test files are made (Main with 1 reference frame, Baseline with 1 reference frame, Main with 2): psp5 writes the answer into the log. The log of every MP4 now also holds the reference frames, the size in macroblocks, CABAC, weighted prediction, whether the profile was relabelled and the default reference pictures (`stream` line), and a file `video-opts.txt` in the `PSP5` folder with `synth=0` / `main=0` in it (for tests) leaves the PSP's timing out / the profile of a Baseline stream as it is.
- **A Baseline video is given to the PSP's decoder as a Main one (not tried on a PSP): the decoder refused every Baseline stream, whatever timing it had.** The PSP's own player plays all four of the maintainer's files (the video made with HandBrake's PSP preset, and three clones of that preset: plain, with x264's own timing messages, and the same with short groups of pictures), and psp5 failed all four in the same way: the decoder's error `0x80628001` on the first picture, with the PSP's timing written in (the video and the first clone) and with x264's own (the other two). So the timing was not what was missing. What every failure has in common, and what none of the streams that decoded has: **Baseline profile**. The PSP's own stream and psp5's copy of it (T1, T2) are Main and decode; plain Main (V1) decoded its first picture; Baseline (V2, the clones, the maintainer's video) did not. The PSP's own player takes Baseline by another route. A Baseline stream that has no slice groups (FMO) and no redundant slices is a Main stream too, so psp5 now writes `profile_idc` 77, the constraint flags of the PSP's own streams and a level of at least 2.1 into its SPS (`h264_sps_rewrite()` and, for a stream that has the PSP's timing already, `h264_sps_relabel()`; the PPS is read to be sure there are no slice groups). The entropy coding stays what it is (CAVLC for the PSP preset); whether the decoder takes CAVLC under a Main profile is the next question, and two new test files (Main with CABAC, Main with CAVLC and B-frames) are for that. Decoding in ffmpeg is picture for picture the same as before.
- **A video that was not made for the PSP is given the timing the PSP's own movie files have, which is probably what its decoder needs (not tried on a PSP).** The PSP's own player plays the maintainer's video (HandBrake's PSP preset: x264 Baseline level 1.3, 480 x 272) but the library psp5 uses for the pictures (`sceMpeg`, the one that plays a PMF) froze on, or gave a fatal error for, every plain x264 stream so far (V1, V2, and T3, which is the PSP's own stream with its SEI messages taken out). The PSP's own streams have in the SPS a VUI with timing and HRD parameters, in every picture a "picture timing" SEI message and in every key frame a "buffering period" and a "recovery point" message. psp5 now writes exactly those into an MP4 whose SPS has no HRD parameters (`h264sei.c`, `unit.c`): the SPS gets the VUI of the PSP's own (the clock tick is a field of the video, 4004 / 240000 for 29.97 pictures a second, else 1 / (2 x the rate); the aspect ratio, range and colours of the video are kept), the SEI messages of the stream are left out and the PSP's are put in front of the first slice of every picture, with the counts worked out from the container's times (the countdown to decoding, the time from decoding to showing: it follows the reordering of B-frames). **Checked on the PC against the PSP's own stream, byte for byte:** an SPS rewritten with the PSP's numbers is the real SPS, and the buffering period, picture timing (twelve pictures) and recovery point messages are the real ones; ffmpeg decodes the converted streams of five encodes (Main, Baseline, High, B-frames) to exactly the pictures of the originals. A stream that has HRD parameters already (the PSP's own, or x264 with `--nal-hrd`) is left as it is. The decoding time in the program stream is now every picture's one picture earlier (it was clamped, which put B-frames out of order). If this is what the decoder needed, the maintainer's own video should play; if not, `video-failed.txt` says how far it got.
- **After a fatal error of the PSP's video decoder no later video can be decoded until psp5 is started again; psp5 now says so (`Restart psp5 to play videos again`, 11 languages) instead of failing every video the same way.** The maintainer played the six test videos one after the other without restarting: V1 (plain Main profile) decoded its first picture and then the PSP froze after about four (T3, the PMF without its SEI messages, did the same); V2 (Baseline) got the decoder's fatal error `0x80628002` on its first picture; and from then on V3, V4, V5, V6 and the maintainer's own video all failed in the same way on their very first call of `sceMpegGetAvcAu` (`0x80020199`, a semaphore that is gone), whatever their kind: the library was broken by V2, they were not tested. A fatal decoder error or that answer now marks the library as broken (`video_poisoned()`), later videos are refused at once with the new text and the log of the video that did it stays in `video-failed.txt`. **What the XMB says** (the PSP's own player is the judge of what its decoder can take): the maintainer's own video (made with HandBrake's PSP preset: x264 Baseline level 1.3, 480 x 272, 30 fps peak, AAC 160 kbps) plays with sound; the six test videos, which have no audio track, are "unsupported" (V4, High profile: "corrupted"). **What this means for psp5:** the PSP's decoder can take the maintainer's video, and psp5's way of giving it to the decoder (the `sceMpeg` library, as the PSP's own PMF files do) fails for streams that are not like the PSP's own. The PSP's own streams have, in every picture, timing messages (SEI) and an SPS that announces them; T3 and V1 (no such messages) froze, V2 (Baseline) was a fatal error. That is a guess to be tested: three new test files (a clone of the HandBrake preset, the same with such messages and long groups of pictures, the same with short groups) are for it. Videos have to be played one at a time with psp5 started again before each.
- **A video on a PSP plays, closes and goes: T1 and T2 played completely on a real PSP (the first time anything of the video player did); an ordinary video of the maintainer says "can't be played" and nothing is known of why yet.** T1 (the PSP's own movie file) and T2 (the same video as an MP4, made into the same stream by psp5) played to the end and closed without a crash, with the new timing, the new layout and the closing of the last build. T3 (the same without the SEI messages its parameter sets announce) froze the PSP again after the first picture; its crash file shows that a unit without a time stamp of its own is given as -1 in all four words (as the code now expects). **The log of a video that did not play is kept now:** when a video cannot be opened, the decoder gives errors or the video is given up, the crash file is not deleted but kept as `video-failed.txt` in the `PSP5` folder (with the reason of every failed step of the opening and a line `end` with the numbers of packets, units, pictures and errors); please send it. *Another converter bug, found with test videos:* an encoder that puts the stream headers in front of the first picture and its access unit delimiter behind them (x264 with the delimiters on does) made a unit with two delimiters; the sample's own delimiter is moved to the front now and there is only one. Six small test videos (416 x 240, no sound; Main, Baseline, Main with timing SEI messages, High, Main with B-frames, Baseline as the maintainer's encoder makes it) go with this, to show which kinds of video the PSP's decoder takes: they are not part of psp5.
- **The PSP's own movie file plays on a PSP; an MP4 is now made into the same stream (the three test files of the maintainer).** *T1, the PMF of PPSSPP's tests, on the PSP:* setup, feeding and all 190 pictures went through the PSP's decoder without an error; two things were wrong around that. The clock ran away (the PSP's library gives **-1** as the time of a picture that has no time stamp of its own, which is most of them in its own streams, and psp5 took it for a time): the time of a picture is now its decoding time, or the one before plus one picture when it has none. And the console died in `sceMpegUnRegistStream` again, in a perfectly clean session: it is now given the address of the handle like every other call (the SDK's header says the handle itself, which is what crashed), and every step of the closing that has ever crashed a PSP is left out from then on (`video-skip.txt` in the `PSP5` folder is written from the crash file the next time a video is opened). *T2, the same video as an MP4:* the picture decoder answered `0x80628001` for some pictures. Two causes. The converter put the parameter sets of the MP4's header in front of a key frame whose group relied on the ones the stream itself had changed to (an ffmpeg decode of the converted stream was wrong from that key frame on, 51 frames of 190; the source MP4 and the PMF decode alike); a stream that carries its parameter sets in its samples is left as it is now, and a group starts at a picture that has a sequence header. And the program stream was laid out another way than the PSP's: now the pictures of a group run on through the sectors, the delimiter says the kinds of slices (`10`, `30`, `50`), a start code has three bytes except in front of the delimiter and the parameter sets, only about every 16th picture has a time stamp (the 16 comes from the PMF: the rule gives its 14 stamped sectors exactly), and the padding is only at the end of a group. **The program stream of the MP4 made from the PMF's video is the PMF's, sector for sector, all 129 of them** (packets, lengths, flags, tables, stream), checked on the PC. *T3, without the SEI messages:* the PSP froze after the first picture; the PMF's SPS announces timing and HRD information, so pictures without the matching SEI messages are probably not decodable for it: a video like the maintainer's (no such information in its SPS) is the next test. Not known: whether an ordinary MP4 (Baseline, High) decodes on a PSP; the sound.
- **The PSP's library now gives units, and its picture decoder refuses the first one; the converter had a unit-order bug, found with a stream of the PSP's own.** The crash file of the fourth test (a 416 x 240 Baseline level 1.3 video, no sound) shows the table of the picture sizes worked: `first-unit` came with the size the table gave, so the library cut the stream into pictures. Then `sceMpegAvcDecode` answered `0x80628001` for the first picture and every one after it, and the console died in `sceMpegUnRegistStream` when the video was closed. *Closing:* the stream is left registered after decode errors (`sceMpegDelete` ends it), so a video that cannot be decoded can be closed. *The decoder's refusal:* not understood yet. What differs from the PSP's own stream (the sample file of PPSSPP's tests): its pictures carry SEI messages (buffering period, picture timing, recovery point) and a delimiter that says the slice type (`09 10` / `09 30`), and it is Main profile; the 416 x 240 video had none of that and is Baseline. **A bug found on the way:** when an MP4 sample already starts with its own access unit delimiter, the parameter sets were put in front of it, so the unit began with the sets and not with the delimiter; they now come after it, and are not added when the sample brings them. The crash file now also holds the answers of the setup calls, the head of the parameter sets, the unit's address and size and its first 16 bytes as the library gave them to the decoder, and the decoder's answer in hex. **Test files** made from the PSP's own stream (the same pictures as a PMF, as an MP4 with and without the SEI messages) are for telling which of these matters: they are not part of psp5.
- **The video player closes without crashing the PSP, and the stream an MP4 is turned into now looks like a PSP's own movie files (not tried on a PSP).** The crash file of the third test on a PSP (no sound this time) ended `... first-feed 32 256`, then `closing 0 0`: **no picture was ever asked for, and the console died while the video was being closed.** Two things came out of it. *Closing:* the end of the decoding and the flush (`sceMpegAvcDecodeStop`, `sceMpegFlushAllStream`) are only called when the decoder has been used, and every step of the closing writes a line (`td-stop`, `td-flush`, `td-free-es`, `td-unregist`, `td-delete`, `td-ring`, `td-finish`), so the next crash file names the call. *No picture:* the PSP's library never gave a unit. A real PMF file (the one of PPSSPP's tests) has, at the start of every group of pictures, a system header and a "private stream 2" packet with a **table of the size of every picture of the group** (its layout is worked out and checked against that file), and the first PES packet of a picture has both time stamps and the P-STD extension. The program stream of an MP4 had none of that, which PPSSPP (it hands the stream to ffmpeg) does not care about and a PSP very likely does. Now every group of an MP4 starts with the same system header and table (groups end at a key frame or after 59 pictures), and the PES packets have the extension and a decoding time one picture before the time to show. A video that gives no picture for 8 seconds is given up (*This video can't be played*) instead of staying black, and the crash file also holds what the library said (`get-au`, `first-unit`, `first-decode`, a sign of life every 2 seconds). **Whether this is the reason is not known:** it is a guess from the difference between the two streams.
- **An MP4 is played without sound, because the PSP's own AAC decoder crashed a real PSP.** The crash file of the second test on a PSP (a 426 x 240 High profile level 2.1 video with 44.1 kHz AAC) ended with `first-sound`, written just before the first call to `sceAudiocodecDecode`: the console died in that call, before the picture decoder had been asked for anything (the stream was fed, the session was made, the AAC decoder's memory and `Init` had worked). What a PSP wants in the parameter block of that decoder cannot be found out without crashing one, so the call is not made any more (`HW_AAC` in `video.c`; a PPSSPP test build can switch it on again with `-DVIDEO_HW_AAC`). The video runs on its own clock. PMF files keep their sound (the library's own ATRAC decoder, not tried on a PSP either). The README and INSTALL.md say so. **Whether the picture of that video (High profile) can be decoded by a PSP is still not known**: this build gets that far for the first time.
- **The image viewer is faster, and the video player has been made safer for a real PSP (neither is tried on one: both were found by reading, and tested in PPSSPP only).** A first test on a PSP showed pictures that came up slowly, and a 240p video with AAC sound that crashed the console. *Pictures:* a JPEG is decoded with the fast integer transform and without the smoothing steps (the picture is made smaller to the screen's size anyway), JPEG and PNG files are read through a buffer of 64 KB instead of 1 KB (small reads are slow on a Memory Stick; a BMP is left as it was, as it is read row by row from the bottom and a big buffer would be read again for every row), the processor runs at 333 MHz while a picture or a video is open (and is put back as it was when it is closed), the reading thread works below the thread of the screen, **the picture before and after the one you see is read in advance** (so L / R show it at once; a PNG is only read when it is the one you want, as it cannot be stopped half way), and a reading that is no longer wanted is stopped at once (leaving the viewer or pressing R again does not wait for a big photo). *Video:* the PSP's AAC decoder is a module of its own (`PSP_MODULE_AV_AAC`) that was never loaded, the program stream that an MP4 is turned into was written by the CPU into its cache and not out to the memory the PSP's decoder reads, the cache is written out and emptied around every AAC frame (and the structure of the decoder has whole cache lines of its own), and the picture buffer is always 512 x 512 so that nothing can be written past it. **If a video still crashes the PSP**, the player leaves a small file behind: `video-crash-last.txt` in the `PSP5` folder (made by the next video you open) says how far it got (no file names or other personal things are in it); please send it with the video's size, profile and sound rate if you can.
- **Videos and Images: two more cards in Media.** The Media tab has three cards now: Music, **Videos** (the film frame of the reference picture) and **Images** (the picture frame of the reference picture), both drawn in code. Each opens a list screen that is the Music list over the whole width (no player): ✕ opens a folder or a file, ○ goes up a folder,
  the top bar says *Videos* or *Images*. **Videos** reads `ms0:/VIDEO` and `ef0:/VIDEO`; **Images** reads `ms0:/PICTURE`, `ms0:/PSP/PHOTO`, `ef0:/PICTURE` and `ef0:/PSP/PHOTO` (a PSP with both storages first asks which; ef0: is left out when ARK has put the internal storage at ms0:). A folder the PSP answers with the wrong folder is refused here as in Music.
- **SELECT's power menu (Exit to XMB, Sleep, Shut down, Restart, Recovery menu) shows in the Media tab, in its Music, Videos and Images screens, and over a picture and a video.** In the Media tab and in the screens of its cards it was not drawn at all: SELECT opened it, but it could not be seen, and it took the buttons (○ or SELECT closed it again). Over a video it also pauses the film until it is gone, and the picture or the video gets no buttons while it is open.
- **The video player** (✕ on a video): MP4 with H.264 video (baseline or main profile) and AAC sound, and PMF / PSMF, on the whole screen, up to 512 x 512 pixels. The pictures come from the PSP's own decoders through the `sceMpeg` library: a PMF is read as it is; an MP4 is read sample by sample (its tables are kept as the file has them, so a long film needs about 4 bytes
  a sample), turned into H.264 with its parameter sets and written as the program stream the library reads, a sector at a time, when its ring buffer asks for more. The AAC sound goes through the PSP's AAC decoder (`sceAudiocodec`), the sound of a PMF through the library's ATRAC decoder; the sound that has been played is the clock the pictures follow.
  ✕ pauses and plays, ← → jump 10 seconds (hold to go on; the bar shows where the jump goes, and it is made when the buttons are let go), **L / R** play the video before or after, ○ leaves, ✕ at the end plays it again. The overlay (L, the number of the video and R at the top; the time, the bar with a round button on it, and the length at the bottom) shows when a video starts or a button is pressed and goes away after about 3 seconds. The PSP is kept from dimming the screen or sleeping while a video plays, and a video is paused when the PSP goes to sleep.
  A video that was left in the middle is remembered by file name and length (in `videomem.txt` in psp5's folder, the last 200 videos): next time you are asked *Continue from 12:34* or *From the beginning*; the second forgets the old place, and so does seeing the film to the end or leaving it in its first 5 or last 10 seconds. The music is stopped when a video starts. **Tried in the PPSSPP emulator only** (an MP4 with and without B-frames, an MP4 with the movie box at its end, a PMF, jumps, the end, the question): the PSP's AVC decoder is not in the emulator, which uses its own, so how an MP4 made by psp5's own writer
  is taken by a real PSP, how the AAC decoder is set up on one, and the sound of a PMF are **not tried**. Not played: AVI, MKV, MPEG-2 and other kinds the PSP cannot decode, H.264 above main profile, anything over 512 x 512.
- **The image viewer** (✕ on a picture): JPEG, PNG and BMP on the whole screen, as large as the screen lets the picture be (a small one is made bigger, a big one smaller; a JPEG is decoded at half, a quarter or an eighth of its size when that is enough, and read a row at a time; a BMP of 1, 4, 8, 16, 24 or 32 bits is read a row at a time and made smaller as it is read), turned the way a camera's EXIF says (all eight ways). A thread of its own reads the pictures, so the buttons never wait: **L / R** (and ← →) show the picture before and after (the last one asked for wins), ○ leaves. The overlay (L, the number and R) shows when a picture is opened or a button is pressed and goes away after about 3 seconds.
  A picture that cannot be read says so (*This image can't be shown*). Limits: a progressive JPEG that needs more than 5 MB for its coefficients, and a PNG of more than 2.2 million pixels, are not shown. The music keeps playing, and the ♪ popup works over a picture.
- **The Music tab is now the Media tab.** It is a row of cards like the Games tab (a big card, the *Open* button under it, the name next to it). It has the cards Music, Videos and Images. The card has the music note of the reference picture, drawn
  in code. The background of the Media tab is drawn in code as well, from the reference picture, and in your theme's colours (the lamp's light, the haze and the line come from the theme, as in the Library). Opening the Music card keeps that background.
- **The Music screen is new.** The songs are listed like the files of the File Manager: a thin frame round the line you are on, no rounded highlight, the folders first (a folder picture), the songs with their ending, a long name scrolls, and the song that plays
  has three moving bars. ✕ opens a folder or plays a song, ○ goes up one folder (and out of the screen from the top). **Only the `MUSIC` folder is read** (`ms0:/MUSIC` and `ef0:/MUSIC`, nothing else on the card): if the PSP has one, you are in it at once; if it has both
  (a PSP Go with a Memory Stick) the first thing you see is the choice of *Memory Stick* or *System Storage*. Folders are shown as folders; the songs of the folder you opened are what plays, one after the other.
- **The player was made new, after the reference picture, and works with the buttons.** △ moves between the list and the player. The buttons are round and have no frame: the one you are on turns white (as the *Play* and *...* buttons and the cog wheel of the Games tab do), so Play / Pause has no
  background unless it is chosen. Their symbols are a little smaller. At the top right of the player is the △ hint: a white triangle on a black circle (the usual hint, turned round) while the list has the focus, and a black triangle on a white circle once you are in the player. In the player ← → move over the buttons
  (volume, previous, play / pause, next, **⋯**), ✕ presses one, and ↑ selects the **bar**, which then shows a dot where the song is now (no frame round it): ← → (or the analog stick) jump **10 seconds** back or forward. **⋯** opens a list with **Shuffle**, **Repeat** and **Equalizer**; ✕ on one of them opens a popup with its values, as the Settings do (Shuffle: Off, On; Repeat: Off,
  Current Song, All, where *All* goes round the whole folder; Equalizer: the PSP's own, Off, Heavy, Pops, Jazz or Unique, the same setting as *Settings → Screen and Sound*, shown only when the PSP can say it). Nothing is switched by a press on the row any more. The volume button shows only the launcher's own volume bar (the player has none
  of its own now): ← → change the PSP's volume while it is chosen, ✕ or ○ ends. The little key signs on the icons are gone, and so are all the buttons that only belonged to the music (L / R for the track, □ for pause, ← → for seeking and ✕ for pause in the list).
- **The music keeps playing when you leave the Media tab, and the ♪ (sound) button of the PSP shows the player alone as a popup in the middle of the screen** (over every screen of the launcher: Games, Media, Library, Music, Settings, Information and Manage Saved Data, and also in the **File Manager** and the **FTP server**, where the music
  now goes on too; USB Mode stops it, as the Memory Stick is not the PSP's then, and a game that starts stops it): the same buttons, bar and popups, shown as the dark rounded player alone (no lighter panel behind it); ○ or ♪ closes it. While it is open the screen below gets no buttons.
  **Not tried on a real PSP yet:** the ♪ button can only be read in kernel mode (as the volume bar does), and the firmware may show its own picture at the same time.
- **○ while the ♪ popup is open is the popup's, also in the moment it closes it:** the same press no longer reaches the screen below (it used to close that screen or go back in it as well).
- **The ♪ popup no longer doubles the buttons** when it is opened in the Music screen. The popup has a player of its own now (its chosen button, held button and open list), so it and the Music screen behind it never share them; a held ← or → keeps repeating in the popup.
- **The top bar does not show the song** that plays any more (the little note with the title in the other tabs is gone).
- **More kinds of song, the ones the PSP can play itself:** MP3 as before; **WAV** (plain PCM: 8, 16, 24 or 32 bits, mono or stereo, any of the rates the PSP's sound output has: 8, 11.025, 12, 16, 22.05, 24, 32, 44.1 and 48 kHz) and **ATRAC3 / ATRAC3plus** (`.at3`, `.oma`, `.aa3`,
  the PSP's own music format, through the library of the firmware). A file that cannot be played is skipped. **Not in yet: AAC (`.m4a`, `.aac`) and WMA** (the firmware has an AAC decoder, but it can't be used safely without a PSP to try it on), and Ogg and FLAC, which the PSP has no decoder for.
  **None of the decoding has been tried on a real PSP yet:** it was tested against stand-ins of the PSP's libraries, so the player's own logic is checked, not the sound of a codec.
- **A folder that the PSP answers with the wrong folder is not entered.** A folder inside `MUSIC` could show the top of the Memory Stick instead of its songs, and every ✕ on it after that made the path longer (`ms0:/MUSIC/OwO/MUSIC/OwO ...`). Music asks the PSP for exactly the path it shows
  (the File Manager does the same), so the answer came from the PSP and not from a wrong path. Now a folder whose lines are those of the folder it is in, or those of the top of its storage, is not entered: you stay where you are and the line above the list says *This folder can't be opened* for a few seconds instead of the path.
  This stops the loop; it can't make the songs of such a folder play, because the PSP does not hand them over. A folder like that is best looked at on a computer (a disk check of the card, or making the folder again).
- The old Music screen's code was replaced: the scan of the whole card for tags is gone (only the song that plays has its tags read), and the Music tab's list, position bar and keys went with it.
- **The background goes dark behind a card that is opened, and bright again when it is closed.** On the cards (Games, Media, and the Library card of Games) the background has its full brightness; when the **Music**, **Videos** or **Images** card is opened
  the whole screen behind it is darkened (150 of 255), from the top of the screen to the bottom (the old Music screen only had a shade behind its list, from the folder name down), and when the **Library** card is opened its background goes darker in the same way
  and by the same amount. It fades in a quarter of a second, and out again when you go back. The top bar stays bright.
- **README pictures:** new ones of the Media tab and the Music screen (taken in the PPSSPP emulator, with a song of the invented demo stick playing), and six others (Games, Options, Theme, Power menu, Library, Library with folders) taken again because the top bar says *Media* now, or because the opened Library is darker now. The Music picture was taken once more for the round buttons and the △ hint.

### Memory

- **The launcher leaves memory free again.** psp5 authentic is meant to keep 7 MB of the PSP's memory free for what is not its own: ARK's Recovery menu (it needs 4 MB), the MP3 decoder, Wi-Fi and the threads. But the toolchain it is built with read the
  setting for that as "take everything except 512 KB", so on a PSP-1000 only 0.6 MB of the 24 MB was free while the launcher ran (measured on a real PSP-1000). It keeps 8 MB free now, and its own memory (the heap) is about 13 MB instead of about 21, which is more than it needs. *Settings → System → Console Information → Free RAM* shows how much is free.
- **A progressive JPEG album cover that needs a lot of memory is not decoded.** Such a cover has to be held whole while it is decoded: up to 15 MB for one of 1600 × 1600 pixels. A cover that would need more than 5 MB (about 1300 × 1300 pixels at the most, 930 × 930 when the colours
  are kept at full size) is not shown any more (the limit was only the number of pixels, 1600 × 1600). Ordinary covers, and every JPEG that is not progressive, are not affected.

### Devices and downloads

- **The PS Vita is supported.** psp5 authentic works on a PS Vita (1000 and 2000) with ARK-5, the standalone install ([FasterARK](https://github.com/PSP-Arkfive/FasterARK)).
  On the Vita it starts as ARK's launcher: copy the `PSP` folder into `ux0:pspemu` and `VBOOT.PBP` into `ux0:pspemu/PSP/SAVEDATA/ARK_01234`. ARK-4 on the Vita should work the same way, but nobody has confirmed it yet.
- **Two downloads instead of four.** There is one zip for ARK-5 and one for ARK-4, and each is for every device (PSP-1000, 2000, 3000, PSP Go and PS Vita). The PSP Go zips are gone: they held the same program as the others,
  with another READ ME. psp5 authentic finds the PSP Go's internal storage by itself.
- **The sounds lie next to `EBOOT.PBP`.** They are in `PSP/GAME/PSP5` with the program, so the zips have no separate `PSP5` folder any more: copy the `PSP` folder and you are done. If a sound file is not there,
  psp5 authentic looks in the game folder of the storage (when it is ARK's launcher) and then in the old place `PSP5/sounds`, so an update that only replaces `EBOOT.PBP` keeps the sounds it had. The `PSP5` folder
  that psp5 authentic makes for its settings and covers is still there; leave it alone.
- **A new `READ ME.txt` in the zips**, longer and made for people who have never done this before: what is in the zip, how to unzip it, and then the steps for each device on its own (PSP-1000, 2000, 3000, PSP Go, PS Vita),
  how to make psp5 authentic ARK's launcher, how to update, what to do when something goes wrong and what to tell us in an issue.
- **The internal storage (`ef0:`) is shown whenever the device has one.** Before, a few places (the File Manager, Plugins and *Information*) also asked whether the model is a PSP Go. Now the only question is
  whether the storage answers, as it always was for the games, the music, Saved Data, Console Storage and the FTP server. On a PSP that has no internal storage nothing about it is shown anywhere.
- **The File Manager lists the internal storage once.** When ARK points `ms0:` at the internal storage (a PSP Go started from it), that is one storage: the File Manager now shows it once, as *System Storage*, and opens straight into it,
  like the game scan, the music, the plugins, Console Storage and Information already did. With a real Memory Stick as well, the File Manager still starts with the list of both.

### Network

- **New: FTP Server** (*Settings → Network → FTP Server*). Copy files to and from the PSP over Wi-Fi: type the address that psp5 authentic shows into an FTP program or into the file manager of your
  computer. The screen is made like USB Mode: it shows the address, whether a computer is connected, and the file that is being sent or received. ○ stops it, and you are back in Settings
  on the same row.
  - psp5 authentic opens the PSP's own Wi-Fi window (the one games show, the same as *Settings → Network → Wi-Fi connection*). Choose one of your saved connections there (the ones from the XMB: *Settings → Network Settings*); psp5 authentic
    then starts the server. If the Wi-Fi switch is off, the screen says so and opens the window when you switch it on. If you close the window without choosing (or a connection fails or is lost), the screen says that the
    FTP server is not running: ✕ *Start* opens the window again, ○ goes back.
  - The computer sees `ms0` (the Memory Stick) and, on a PSP Go, `ef0`. Nothing else of the PSP can be reached (not flash0, not flash1).
  - **There is no password:** any user name and password work, so use it on your own network. It uses passive mode, which FTP programs, Windows Explorer and curl do by default.
  - Download, upload, resume, rename, delete and new folders work. Up to three computers can be connected at the same time. The PSP does not go to sleep while it runs.
  - *Settings → Network* now says what both of its rows do. The new texts are in all 11 languages.
  - **The network is started the way ARK's own launcher does it** (arkMenu): the same sizes and thread priorities (a pool of 256 KB, the net threads at priority 42) and an event handler for the access point control.
    The Wi-Fi connection row of Settings starts it the same way. This is for the case that a computer says "No route to host" although the PSP shows its address (it was reported on a real PSP). The change has not been tried on a PSP.
  - **A hint for WLAN Power Save.** When the PSP's own *WLAN Power Save* (XMB: *Settings → Power Save Settings*) is on, the PSP's Wi-Fi sleeps and may not answer the computer at all ("No route to host"). The FTP screen
    then says so under "Waiting for a computer", until a computer is connected. The new text is in all 11 languages. INSTALL.md has a list of what to check.
- **A guide for when the computer cannot find the PSP** ([docs/FTP-TROUBLESHOOTING.md](docs/FTP-TROUBLESHOOTING.md), linked from INSTALL.md and the README). The PSP joins the Wi-Fi and shows its address, but the computer says *No route to host* or times out. The guide starts with the quick checks (*WLAN Power Save*, the same
  network, client isolation) and then shows the workaround for some routers: a *static ARP entry* on the computer, with the commands to add, check and remove it on Linux, Windows and macOS, how to find the PSP's address and MAC address (the MAC address is in *Settings → System → Console Information*), and what the FTP screen
  means when a connection times out (five minutes of silence from one program) or the server stops (the PSP lost its Wi-Fi: *The connection was lost*).

### Library

- **The pictures of the games are drawn faster, which matters most in a Library with many games.** A full Library draws up to 25 cards, each with its own picture of 144 x 80 pixels that the PSP's graphics chip reads from the main memory every frame, and the more games you have the more of the
  screen is cards. The pictures (and the picture behind the cards on Games) are now stored in the order the chip reads fastest, instead of row after row (this is called swizzling). **They look exactly the same**: checked pixel for pixel in a PSP emulator, in Home with the game's own picture behind the cards,
  in the Library and in its folders. What costs the same however many games you have: the program's own work for a frame (measured with 6 to 4000 games: the same) and the number of calls to the graphics chip (70 for a full Library). Nothing here was measured on a real PSP, so how much faster it is there is not known. If the
  pictures of the cards look scrambled, striped or torn on your PSP, please open an issue with your PSP model and ARK version: that is the one thing that could go wrong on a real PSP and not in the emulator, and it is easy to take out. If the Library is still slow, please tell us how many games you have and
  when it is slow (scrolling fast, standing still, opening it, the first time you see a game).
- **The Library's background is softer.** The thin lines of its picture (the fine lines of the petal and the fan, and the big rings at the lower right) were one pixel wide and showed the steps of the pixel grid, so they looked sharp and dotted. The picture is now shown a little smoothed
  (every pixel is the mean of 2 x 2 pixels of it, and the picture sits half a pixel further down and to the right), so the lines are continuous and soft. The four rays of the little star are soft lines too. The picture is still made once and kept in video memory, so this costs nothing
  per frame but the star's rays (about 2 KB of the display list); the backgrounds of the themes are as they were. This is new and nobody has seen it on a real PSP yet: if the Library's background looks wrong on yours, please open an issue and tell us your PSP model and your ARK version.
- **The Library has a background of its own.** The picture of a game is **not** shown behind the Library any more (the Library card has it at full brightness, and the opened Library darkens it, see *Media and Music*): in every theme it has the same drawn picture on black, at the
  right: a twisted ribbon of fine dotted lines, a fan of lines to a point, big rings at the bottom right, a little twinkling star and some specks of dust, with a copper light along its edges. It is drawn by the program
  (no picture file), like the themes and icons. The colours follow your theme: the black has a trace of the theme's own dark, the lines carry some of its light, and the glow is the gold of the PS5 colours (the
  accent colour of PS1 to PS4). It fades up from black with the cards when you open the Library. *Show Game art* now only matters for the Games tab. The picture is made once and kept in video memory, as the themes are, so
  after the first frame the Library draws it as one image.
- **The Library card shows the Library's background.** With the Library card chosen in the Games tab, the screen behind the cards is the Library's own background, so opening the Library changes nothing behind the cards
  (it does not come up out of the dark when it is opened from its card) and closing it brings you back to the same background. Moving from a game with a picture to the Library card fades the picture out over the Library's
  background. Moving from the Library card to a game keeps the Library's background until the game's picture has loaded, and the picture fades in over it. A game with no picture of its own shows its icon, blurred, as before;
  a game that has neither gets the theme's background. Only then (and with *Show Game art* off) the screen dips to black for a quarter of a second while the two backgrounds change places (they cannot be mixed in video memory).
- **Six ways to sort.** The Sort button has a new popup with a check mark at the one in use: *Name (A-Z)*, *Name (Z-A)*, *Date added (Newest-Oldest)*, *Date added (Oldest-Newest)*,
  *Size (Large-Small)* and *Size (Small-Large)*. A folder goes by the newest (or oldest) game in it, or by the size of all its games together. The disc is always first. The size of a game that
  lives in a folder is counted in the background, so after the start the order by size can change for a moment. *Recently played* is not a choice any more (the Games tab already shows what you
  played last). A `psp5.cfg` of an older version still works: *Name* is *Name (A-Z)*, *Recently added* is *Date added (Newest-Oldest)*, and *Recently played* is *Name (A-Z)*.
- **psp5 authentic now remembers when you played each game** (`lastplayed.txt` in psp5 authentic's folder). It notes the time when psp5 authentic starts a game, and with ARK the helper plugin of *Time played*
  tells it about games you started from the XMB (psp5 authentic learns it the next time it starts). Games you played before this version have no time yet, but they keep their place in the
  Games tab. The Games tab, *Information* and the new *Games and Apps* list use these times.
- **The Library's cards are rectangles**, as on the console: no rounded corners. The selected card keeps its size (it was a little bigger) and has a straight white frame. The cards in the Games tab keep
  their rounded corners.

### Power menu

- **New choice: Exit to XMB**, at the top of the power menu (SELECT). The power menu opens on *Sleep* as before, so SELECT and then Confirm is still Sleep; ↑ goes to *Exit to XMB*. The rows *Exit to XMB*, *Sleep*,
  *Shut down*, *Restart* and *Recovery Menu* are **gone from Settings**: the power menu has them.
- **SELECT opens the power menu on every screen**, not only in Games, Library and Music: also in *Settings*, *Information*, *Manage Saved Data*, the *File Manager* and the *USB* screen. Sleep keeps you where you are;
  Exit to XMB, Shut down, Restart and Recovery menu close the screen first (from the USB screen USB is stopped first). It does not open while the screen has a popup of its own open or a job running (a copy, a move or a
  delete), so nothing is left half done. The PSP's own Wi-Fi screen and the start-up screen are not psp5 authentic's and keep their own buttons.

### Information

- **The list of the Information screen scrolls like Settings, the File Manager and Manage Saved Data.** The highlight used to stay on the last row shown and the list moved with every step; now the highlight moves down through the rows that are shown (seven) and the list moves only when it would leave them. A caption (such as *PARAM.SFO*) comes into view with the row under it when you go back up. Every other list already scrolled this way (the Music list, the Library, Manage Saved Data, Games and Apps, Settings and the choices of a setting, the File Manager).

- **Console Information: new rows *Last IP address* and *Free RAM*, and no storage rows.** *Last IP address* is the last address that the PSP's own Wi-Fi window or the FTP Server got. It is kept after the Wi-Fi is closed and after a restart (`lastip.txt` in psp5 authentic's folder), and until there is one it says *Not recorded yet*.
  It is an old address, not a sign that the Wi-Fi is on: it may have changed, and an FTP program needs the address on the open FTP screen. A connection that is made outside psp5 authentic is not seen. *Free RAM* is the memory that is free right now, outside psp5 authentic's own heap; it is read when the page is opened.
  The rows for the Memory Stick and for the PSP Go's internal storage are gone: the space of both is in *Settings → Storage → Console Storage*. The new texts are in all 11 languages.
- **Console Information shows the whole version of ARK.** The row that said *CFW* and *ARK-5* is now called *CFW (ARK-4)* or *CFW (ARK-5)* (the number is ARK's major version), and on the right it shows the whole version that ARK reports, for example *4.20.69*
  (major, minor and micro version, as the PSP's SystemControl gives them). If ARK gives no sensible number, the row shows a dash. Without ARK there is no row, as before. There is nothing new to translate.
- **New row: Last played** (date and time), right under *Modified*. It is there for every game and app psp5 authentic knows a time for, **also for the UMD disc in the drive** (by its disc ID,
  from psp5 authentic or from the XMB, like the time played, which the disc already had). Games you played before this version, games you never played and a disc without a disc ID have no such row.

### Settings

- **The title of a Settings page is its own name** (*Appearance*, *Date and Time*, *Install New Plugin*), not the whole way to it (*System > Appearance*).
- **Settings look like the console's.** One list at a time instead of two columns: the first list has six pages, each with an icon, and Confirm opens one; Back goes back one list, and Back on the first list closes
  Settings. The list scrolls like the file manager's and Manage Saved Data's: the highlight moves down the rows you can see and the list only moves when the highlight would leave the screen (before, the list kept the
  highlight on the last row and moved with every step, both up and down). A thin bar at the right shows where you are in a long list. The short description at the top of each page is gone. **← and → do nothing in Settings any more** (the D-pad and the analog stick sideways): with one list at a time they had nothing left to do; ✕ opens, ○ goes back, ↑ ↓ move.
- **New order and names.** The first list is *psp5 authentic*, *Network*, *System*, *Storage* (was *Memory*), *Screen and Sound* (was *Display and Sound*) and *Controls*. The new names are in all 11 languages.
- **System** is a list of six pages that were categories of their own: *Console Information* (was *About*), *Appearance* (was *Theme*), *Language* (it was a row in the *psp5* category; now a page with the 11 languages, Confirm chooses one),
  *ARK-5 Settings* or *ARK-4 Settings* (the old *System* category: USB charge, WPA2, hide pictures and so on; it is called after the ARK your PSP has), *Performance* and *Plugins*.
- **The title says where you are**, for example *System > Appearance*.
- **Console Information** says where the app comes from: *Made by 08Weber* and *Forked by Xioonae*, under the version number.
- **New category: Storage**, with the *File Manager* and *USB Mode* (in that order). *Power and USB* is gone (its power rows are in the power menu, see above).
- **Closing the File Manager or USB Mode brings you back to Settings → Storage**, on the row you came from (before, to the main screen). The Wi-Fi screen already did this.
- **The switch is called *Show Game art*** now (it was *Game art in the background*), in all 11 languages.
- **Three themes are gone:** *Starlight Blue*, *Chroma Indigo* and *Chroma Pearl* looked almost like *Cobalt Blue*, *Galactic Purple* and *Sterling Silver*. If you had chosen one of
  them, psp5 authentic starts with the PS5 theme instead.
- **Shut down** is written with a small d again (the other names keep their capitals).
- **The first list uses the same letters as the other lists** (the names of the six pages were bigger than the settings in them).
- **The cog wheel looks like the Play and ⋯ buttons when it is selected:** a white circle, as high as the frame of a selected tab, with a black cog in it (it was a frame).
- **New: Console Storage** (*Settings → Storage → Console Storage*, the first row of *Storage*). At the top (it can't be chosen) it shows the size of the storage, a bar with a colour for what is on it, and the free space.
  The bar shows what is on the storage in the console's own colours, one after the other: blue for *Games and Apps*, purple for *Media*, orange for *Saved Data* and grey for *Other*; a part that is not empty is at least 2 pixels wide, so that a small one is seen.
  Under it are four rows with a colour and a size: *Games and Apps*, *Media* (the `MUSIC`, `VIDEO` and `PICTURE` folders and `PSP/PHOTO`), *Saved Data* and *Other* (everything else that is used).
  The sizes are counted in the background, so they grow for a moment. Confirm on *Games and Apps* opens the list of all your games and apps, Confirm on *Saved Data* opens *Manage Saved Data*, and Back comes back
  to the same row of Settings. *Media* and *Other* do nothing yet. On a PSP Go with a Memory Stick the page has two blocks, the internal storage and the Memory Stick, each with its own four rows.
- **New: Games and Apps.** The list of the games and apps on the console, made like *Manage Saved Data*: the same list and buttons (✕ Delete, □ Mark, △ Select all, ○ Back), a Sort button with a popup, the picture of
  the game, its ID, when you played it last and its size at the right. It sorts by *Least used*, *Date used (Newest-Oldest)*, *Date used (Oldest-Newest)*, *Name (A-Z)*, *Name (Z-A)*, *Size (Large-Small)* and
  *Size (Small-Large)*. A game you delete (a file, or the folder of an app) is gone from Home and from the Library too. psp5 authentic itself and anything outside `ISO` and `PSP/GAME` can't be deleted.
- **Manage Saved Data sorts by *Last updated*, *Name* and *Size*** (it was *Last updated*, *Size* and *Name (A-Z)*).
- **New: Manage Plugins** (*Settings → System → Plugins → Manage Plugins*, the first row, above the switches). It installs and uninstalls plugins: it adds and removes the lines of `PLUGINS.TXT`.
  - **Install New Plugin** (the first row) opens the File Manager to choose the plugin. It shows only folders and `.prx` files, and nothing else works there (no copy, move, rename or delete). Confirm on a plugin brings you back.
    The page shows the file's name and where it rests. Then you choose **which `PLUGINS.TXT` to edit** (the one in the ARK folder `ARK_01234` or in `SEPLUGINS`, on the Memory Stick, and on the internal storage of a PSP Go)
    and switch on *XMB*, *Games* and *PS1* (`vsh`, `game`, `pops`). After a question psp5 authentic adds one line for each switch to that `PLUGINS.TXT`, and the line points to the plugin **where it is**:
    **the plugin file is not moved, copied or deleted.** A `PLUGINS.TXT` (and its folder) that isn't there is made. A line that is there already is not added twice.
  - **Under it, every plugin that is installed**, with where it is (*XMB*, *Games*, *PS1*, *All*). Confirm opens its page: **Install to** lists only the places it is not in yet (every runlevel of every `PLUGINS.TXT` that has no line for it, also one that
    isn't there yet; the new line points to the file where it rests), **Uninstall from** only the places it is in (one row for each line of the plugin, in each of the four files). Each is a list of switches and a button; the button asks first.
    *Uninstall from* removes the lines and never the plugin file, which stays where it is.
  - The page *Plugins* now reads all four files: the ARK folder's and `SEPLUGINS` on the Memory Stick, and on a PSP Go with both storages the same two on the internal storage. Switches you changed but did not save are saved before a line is added or removed.
  - New texts in all 11 languages (239 strings).

### Ideas from the original project's issues

psp5 authentic is a fork of [psp5](https://github.com/08Weber/psp5) by Mille Weber (08Weber). Two reports in the issues of the original project described problems that psp5 authentic could have as well, and the changes
below are inspired by what the original project found and did about them. The code is psp5 authentic's own. Thank you to 08Weber, and to the people who reported the problems there:
[issue 37](https://github.com/08Weber/psp5/issues/37) (TempGBA single-game folders show as grey cards without a name) and [issue 19](https://github.com/08Weber/psp5/issues/19) (unstable overclock and settings on a PSP Go).

- **A game folder with a `PBOOT.PBP` shows the name and the pictures from it** (inspired by issue 37). TempGBA's single-game folders keep the name and the icon of the game in `PBOOT.PBP`; the `EBOOT.PBP` beside it is the same in
  all of them, with no title and no pictures, so every game was a grey card without a name. Now the title, the icon and the picture of `PBOOT.PBP` win, as in the XMB, and whatever it lacks stays as the `EBOOT.PBP` has it.
  The game is still started from the `EBOOT.PBP`, and its kind (homebrew, PSN, PS1) is still the `EBOOT.PBP`'s. A `PBOOT.PBP` that is not a real PBP, or has no title or pictures, changes nothing, and a folder that has only a
  `PBOOT.PBP` (an installed update) is still not listed. The first start after the update reads the games once more (the list that psp5 authentic remembers is made again; it is also made again when a `PBOOT.PBP` is added, changed or removed).
- **A game or image with no title is named after its folder or its file.** If the title in a game is missing, empty, only blanks or only a ™ or ®, psp5 authentic now shows the name of the game's folder (`PSP/GAME/<folder>`), or, for an
  `.iso`, `.cso`, `.zso`, `.dax` or `.jso`, the name of the file without its ending. (An empty title used to be shown as an empty name.) Blanks around a title are dropped.
- **The overclock and the storage cache are put in force before anything is read** (inspired by issue 19). ARK-5 does that when the first module is loaded after the system has started, and psp5 authentic loaded its first one
  (the music codecs) after the intro and the game scan, so the PSP could change its clock and cache in the middle of reading files. On a PSP Go with an overclock of 333 MHz or more, the original project saw it lag or hang. psp5 authentic now loads and
  unloads one module first of all, before any thread is started or any file is read.
- **A short wait before the PSP restarts, switches off or goes back to the XMB** (inspired by issue 19). The file that was just saved (`SETTINGS.TXT`, `PLUGINS.TXT`) gets 50 milliseconds to be written first, so a settings change
  is not cut off by the reset.

### The name

- **The app is called psp5 authentic** wherever you can see it: its name in the XMB (*Game → Memory Stick*) and the picture next to it, the texts of the menus in all 11 languages (*Boot into psp5 authentic*,
  *psp5 authentic is ARK's launcher*), the greeting of the FTP server, and the documents. The folders and files (`PSP/GAME/PSP5`, `PSP5`, `psp5.cfg`, `psp5time.prx`) keep their names, so an installed version keeps
  its settings, play times and covers.

### Games

- **The Games tab is always full.** It shows the games and apps you played last first, then the ones you played before, and then the ones you added last: nine games and the Library card as the tenth.
  A game you copy onto the PSP shows up on Home by itself, without waiting to be played.
- **New: Keep in Home.** The options popup (START) of a game has a switch, *Keep in Home*, in the style of the switches in Settings. A game that is kept stays among the cards even if you have not played it for a long
  time: it keeps its place in the order, and if that place is beyond the nine it takes one of the last places (the games that come last make room for it). Up to nine games can be kept; the tenth says so and the switch stays off.
  Confirm turns the switch and the popup stays open; Home changes when you close the popup.
- **Gone: Pin to Home, Add to Home and Remove from Home.** Nothing is pinned or taken off Home any more. A game you pinned with an earlier version is kept now. The Library card is always the last card of Home and has no
  options: no popup, and no ⋯ button under it (only *Open*). **Manage Saved Data is not in the options popup any more:** it is in *Settings → Storage → Console Storage* (see Settings).
- **Games you played from the XMB move to the front of Home** when psp5 authentic starts, the one you played last in front (before, only the games started from psp5 authentic did).
- **The name and the type tag (PSP, APP ...) next to the big card are as large as in the Library.** *Play*, the time played and the name all have the same size now, in Games and in the Library.

### Lists, names and popups

- **A name that does not fit scrolls** instead of shrinking or ending in "...". It waits a moment, scrolls to the end, waits again and starts over. This is for the name next to the big card (Games), the name
  of the selected card in the Library, and the highlighted row of the File Manager, *Manage Saved Data* and *Games and Apps* (the rows that are not highlighted are still cut with "..."). The names in
  Settings and in the popups are not changed.
- **The bar that shows how full a storage is ends with a straight line** where the used part stops (it was round). This is for *Console Storage*, *Manage Saved Data*, *Games and Apps* and the File Manager.
- **The theme is the background only.** A theme colours the background (the surface, the light, the haze, the dust, the shapes of the old consoles) and the start-up animation, and nothing else: everything on top of it looks the same in every theme, with the colours of the console's own screens.
  - **Bars** (the free space in the File Manager, *Manage Saved Data* and *Games and Apps*, the bar of the FTP Server, the panel of a copy, a move or a delete, the song position in Music, the volume): one background (a trace of white), one light grey for the part that is done or used, and a straight end on that part. *Console Storage* keeps the colours of what is on the storage, on the same background.
    A file that comes in over FTP, whose size is not known, shows the whole bar in a fainter grey. USB Mode has no such bar: the computer reads and writes the card directly, and the PSP cannot see which file moves or how far it has got.
  - **Switches** (Settings, and *Keep in Home* in a card's popup): a grey track with a light knob at the right when it is on, a darker track with a knob at the left that is only a ring when it is off.
  - **Ticks** (a marked row in the File Manager, *Manage Saved Data* and *Games and Apps*, and *Apply to All*): a rounded square with a light fill and a lighter edge, not a circle, with a white check mark in it when it is ticked; a row that is not marked has the same box without the check mark. A marked row is **not highlighted** any more, only ticked.
  - **The buttons in the hints** (✕ ○ □ △ and the START / SELECT pills): a light grey plate with a dark grey mark, and **no dark capsule behind them** (the File Manager, *Manage Saved Data*, *Games and Apps*, the FTP Server, USB Mode, the progress panel and the Controls page of Settings).
  - **Cards** that have no picture (yet), and the picture places of Information and *Manage Saved Data*, are one near-black colour. The three dots of the Loading screens are the colour of the text. The small previews in *System → Appearance* have a white strip under the picture in every theme.
- **The text of the Library's top row is one size and one colour.** The view text and the number of cards (*All: 14*) are as small as the text of the Sort button (they were larger), and the Sort text is white like the view text (it was grey until the button was chosen). In *Manage Saved Data* and *Games and Apps* the Sort text is white too, like the rest of the text, and as large as the Library's.
- **The plate of the type tag (PSP, ISO, CSO …) and the outline of the battery are opaque.** They were 92 % and 85 % opaque, so a little darker than the face of the clock next to the time played; all are the same light grey now (the tag in Games and in the Library, the battery in the top bar).
- **Marking with □ does not move the highlight.** In the File Manager, *Manage Saved Data* and *Games and Apps* it only marks (or unmarks) the row; ↓ goes to the next one.
- **Hold ↑ or ↓ in a popup to move fast.** The first step comes at once, then it repeats after a third of a second. This was missing in the options popup of a game, the Sort and view popups of the Library
  and the power menu (SELECT); the popups of the other screens did it already. A direction that is held when a popup opens still does not move it.
- **Games and Apps starts on *Least used***, the first choice of its Sort popup (it started on the second). **Choosing a sort starts the list at the top**, in *Games and Apps* and in *Manage Saved Data*
  (the list did not scroll back before).

### File Manager

- **A question when a name is taken.** When you paste (copy or move) a file or a folder where something of that name is already, psp5 authentic asks instead of making `name (2)` by itself: **Overwrite**, **Skip**, **Merge** (a folder onto a folder only), **Rename** or **Cancel**. Skip is chosen first, so a press of ✕ loses nothing; Overwrite is red.
  - *Overwrite* is safe: the old one is only moved aside, the new one is made, and then the old one is removed. If the copy fails, the card is full, or you press ○, the old one comes back as it was.
  - *Merge* puts the files into the folder that is there; a name that is taken inside it is asked in the same way. With a move, the original folder is removed when it is empty (what you skipped stays in it).
  - *Rename* opens the keyboard with `name (2)` already typed in. A name that is taken or cannot be used keeps the question open.
  - **Apply to All** (△ in the question, shown when more names may be taken): the answer is used for the rest without asking again. One answer is kept for a folder onto a folder and one for everything else, so *Merge* for all does not decide what happens to the files inside. *Rename* for all does not open the keyboard: each item gets the next free name by itself (`name (2)`, `name (3)` and so on).
  - A copy into the folder the item is in has no old one to overwrite: it only offers *Rename* and *Cancel* (with *Apply to All* when there is more than one item: then *Rename* numbers them all by itself).
  - ○ cancels the whole copy or move. What was done stays done; a half-made copy is removed (but a merge keeps the files that arrived whole, and never touches what was there).
- **Rename and New Folder look first.** A name that is taken says *That name is already taken.* instead of being left to the PSP (which, on some systems, replaces the file without a word). A change of capitals only (`a.txt` to `A.txt`) works. A name that ends in a dot or a space, has a control character or is longer than 255 bytes is refused as a bad name.
- **A copy keeps the dates of the original** (a file made by a copy used to get the time of now, so *Modified* in Information changed with every copy or move to the other storage). Folders keep theirs too.
- **A move on the same card is checked.** The PSP's rename call may take only the name of the new place and rename the thing where it is (the PSP emulator does; real PSPs were not tested). If that happens the file is put back and moved by copying, so it never ends up renamed in the wrong folder.
- **A move to the other storage** that cannot remove one of the originals says *Copied, but the original could not be removed.* and goes on with the rest. A folder cannot be put into itself: *A folder can't be put into itself.*
- **Names**: a copy of `a.tar.gz` is `a (2).tar.gz` (it was `a.tar (2).gz`); a name that is near the longest a FAT name can be is cut, not in the middle of a character, so that ` (2)` fits.
- **A card that is nearly full**: the size of what has a name that is taken is not counted at the start any more (it may be skipped, merged or replaced); a big file is checked when its turn comes, and a write that fails on a full card says *Not enough space* instead of *Failed*.
- 9 new texts in all 11 languages.

### Date and time

- **New: *Settings → System → Date and Time*** (under *Language*). It has four rows:
  - *Current Date and Time* shows the date and time that psp5 authentic shows now, for example `2026.10.28 14:05`. It is only shown: it is not a button.
  - *Daylight Saving*: *Remove One Hour*, *Keep Unchanged* (the standard) or *Add One Hour*.
  - *Date Format*: `YYYY.MM.DD` (the standard), `MM.DD.YYYY` or `DD.MM.YYYY`.
  - *Time Format*: *24-Hour Clock* (the standard) or *12-Hour Clock* (`2:05 PM`).
- **They change what psp5 authentic shows, everywhere**: the clock at the top, the *Modified* and *Last played* dates in Information, and the dates in Manage Saved Data. The PSP's own clock and settings are never changed: the XMB, the games and the files that psp5 authentic writes keep the real time, and the FTP Server still tells the computer the real dates of the files.
- **Dates are written with points now**: `2026.10.28 14:05`, where it was `2026-10-28 14:05` (the same order, so nothing changes for you unless you pick another format).
- The three choices are kept in `psp5.cfg` (`dst`, `datefmt`, `timefmt`).
- New texts in all 11 languages (10 strings).

### Themes

- **PS4: the wave has no artifact at its lower end, and it never disappears.** Two things were wrong with the wave.
  - *The artifact.* From about five seconds into every round the lower end of the wave folded over itself and drew thin crossing streaks and small triangles in the bright band at the bottom of the screen (and a few stray pixels at the top right corner, where the wave leaves the screen,
    showed the same fault). The lowest point of the curve that the wave is drawn from had wandered off (it had been measured where the reference animation has already faded out) and turned the end of the wave sideways. The curve is straight and smooth to both ends of the screen now.
    A third, shorter fault is gone too: for about a tenth of a second, at about 2.5 seconds, a small star-shaped streak flashed at the top right, where a nearly flat piece of the wave dips by a hair and the code turned that piece of its band upside down.
    Everything in the middle of the wave is as it was.
  - *The loop.* At the end of every round the wave faded out completely and then faded in again, so for a moment there was no wave. Now the next wave fades in while the old one fades out (the two always add up to the brightness of one), so there is a wave at every moment, and there is no jump
    where the round starts again. A round is 6 seconds instead of 8 (the wave moves as fast as before). For the last 2 seconds of a wave's life two waves are on the screen, so the wave then asks the graphics chip for twice the drawing (200 calls instead of 100; the ribbon of PS3 makes 300).
    **None of this has been tried on a real PSP**: if PS4 is slower than the other themes on yours, please open an issue with your PSP model and ARK version.
- **The little shapes behind the dust and the cards' shadows are kept in the PSP's video memory.** The dust of the Default theme and the nine cover colours is some 470 tiny pictures a frame, each shrinking a whole 64 x 64 shape to a few pixels, which makes the graphics chip read texels spread over all of it, and it read them from the
  main memory, which is slow for that; the cards' shadows and the rings use the same shapes. They are now made once into the video memory (swizzled, 55 KB of it; it is made again after the PSP has slept), which the chip reads much faster. **Nothing looks different**: 24 pictures in a PSP emulator (the Default theme, Cobalt Blue, PS1 to PS4, Home with the games' pictures, the
  Library, three moments each) are the same, pixel for pixel, as before. This is the part of the PS5-style themes that the other themes do not have, and it was the one that was left after the dust was drawn in a few calls; how much faster it is on a real PSP is not known yet. If the Default theme is still not smooth on yours,
  please open an issue with your PSP model, ARK version and which screen (Games tab, a menu without cards).
- **The dust and the Library's background take far fewer calls to the graphics chip, so menus run smoother.** The dust of the Default theme and the nine cover colours is some 470 little pictures a frame, and each was a call of its own to the PSP's graphics chip (the PSP does two calls into its
  system for every one of them, which is slow); the dust made the menus run at less than the full speed. Now the dots are given to the chip together: about 5 calls and 4 changes of texture a frame instead of 467 and 74, in the same order, so **the picture is exactly the same**
  (checked pixel for pixel against the old one, also in a PSP emulator, at several moments and in two themes). The Library's own background (about 370 specks of dust, drawn once and kept) is drawn in about 90 calls instead of 480 when you come to it from a game or a theme's background,
  which is the one moment of it that was heavy. The sine that moves the dust also costs less, and the colours of the specks are kept instead of made again every frame. This was measured with the PC's test programs and the emulator, not on a real PSP: how much faster it runs there is not known yet;
  if a menu still runs slowly on yours, please open an issue and tell us your PSP model, your ARK version, the theme and the screen.
- **The theme called PS5 is called *Default*.** It is the console as it comes: grey with a warm lamp. Nothing changes for you: if you had chosen it, it stays chosen (`psp5.cfg` still says `theme=ps5`).
- **The dust moves.** In the Default theme and in the nine cover colours the dust now drifts a few pixels and twinkles, and a few stars flare, in the colour of the theme. It lies in a wide area across the screen: from the bottom left, flat and wide in
  the middle, and tall at the right end. (The dust used to be a still picture; it was drawn once.) Everything comes back to where it started every 8 seconds, so there is no jump. It is drawn on every screen that has the theme's background, and it
  uses about 23 KB of the 128 KB that the PSP's graphics chip has for a frame.
- **PS1: the four symbols float smoothly, and a little more.** They went up and down by two pixels in steps of one whole pixel, which looked like jerks. They now float round a gentle figure of eight, 4 pixels up and down and 2 to each side, once in 8 seconds, on fractions of a pixel, so the
  movement is smooth. At the start of every round they stand exactly where they stood (on whole pixels, with sharp lines, the square and the cross and the triangle); in between a line is as soft as any moving line is (the circle, the square and the triangle are drawn with the same smooth lines as the cross, which
  is also new: they used to be drawn on whole pixels only, which cannot move by less than a pixel). The rest of the PS1 symbols is as it was: about 60% larger than in the first version, at the right, a little nearer the centre of the screen.
- **PS2: glowing dots on a ring that tumbles, instead of the towers and orbs.** Eight dots go round a ring that turns in space: seen from the front it is an ellipse, and about every 1.7 seconds it turns edge-on, the dots gather in a line, and then it opens
  again on the other side, so that they seem to turn round. They go round three times in 8 seconds, each a little faster or slower than the next. Nothing else is drawn (no ring, no towers). The moments when the dots are in a line are the ones measured on the reference animation.
- **PS3: a silky ribbon, instead of the three wave lines.** It twists across the screen with a bright strand and two fine threads in it. Its edges are soft (a hard edge shows its steps) and it has no particles. It loops in 6 seconds, and there is no jump where it loops.
- **PS4: one wave from the bottom of the screen to its top right corner, instead of the waves and the rising particles.** A bright rim with a soft band on its left rolls to the right and fades (how it loops is in the PS4 entry at the top of this section).
- **Every theme has the same background.** The cone of light from the lamp, the lamp's glow, the haze at the bottom left and the second light at the lower right are drawn by the same code with the same strengths in all 13 themes, and a theme only gives
  the colours. Before, PS1 to PS4 were set up stronger than the others (their lamp glow was up to 2.7 times as strong and their haze up to 2.2 times), so they did not look like the same scene. The nine cover colours were given new numbers for the
  shared strengths and look the same as before (the difference to the old picture is under one step of 255). PS1 to PS4 keep the colours of the pictures of the real themes (the mean colour of each corner is close to the picture's), but their lamp and haze
  are as strong as the others', so they are a little softer than before, and the dark base of PS2, PS3 and PS4 is a little different.
  These animations are new and nobody has seen them on a real PSP yet. If a theme is slow or looks wrong on yours, please open an issue and tell us your PSP model and your ARK version.
- **PS1, PS2, PS3 and PS4 have the colours of the real consoles' themes.** PS1 is grey with a warm lamp, PS2 is black with blue-violet light, PS3 is black with gold and cream (it was blue), and PS4 is blue-violet with purple at the lower right (it was a bright cyan-blue).
  The colours were measured from pictures of the four themes. (The switches, the bars and the cards no longer follow the theme: see *The theme is the background only* below.)
  The lamp, the beam of light and the haze are the same for every theme, so the look is close to the pictures but not the same pixel for pixel.

### Sound

- **New sounds: all five are as loud as one another.** *Move* and the startup sound used to be quieter than *select*, *back* and *error*; now they are balanced (the same average level, peaks between -4 and -8 dBFS). They are 16-bit mono files at 44.1 kHz in `PSP/GAME/PSP5`, as before, and psp5 authentic plays them
  **as they are**: the gain that it used to apply (2.5 times, 8 dB, for the older, quieter recordings) is gone, because it would clip the new ones. That goes for your own `.wav` files too: if you made some for an earlier version, they play 8 dB quieter now (make them louder with `tools/convert_sounds.py`'s notes in mind: peaks around -4 to -8 dBFS).
- **No more crackling when you scroll fast.** Holding a direction, or pressing it quickly, starts the *move* sound again before the last one has ended (the recording is 0.37 s long). psp5 authentic used to start the sound over from its beginning in the middle of the old one. That cuts the wave and makes a click, and
  a held direction repeats 12 to 22 times a second, so that is a crackle. Now the old one fades out in a straight line over 5.8 milliseconds while the new one starts in a voice of its own. The recordings were never the problem: a cut made steps up to 16 times larger than the largest step between two neighbouring samples inside the recording. The same goes
  for every other sound and for your own `.wav` files. Two different sounds at the same time (a *select* over a *move*) are still just added, as before.

### Docs

- New picture in the README: the File Manager's question about a name that is taken (`file-manager-conflict.png`; the folders in it are the demo's).
- New picture in the README: *System → Date and Time* (`settings-date-time.png`). The pictures of *System*, *Appearance*, *Console Information*, *Console Storage*, *Manage Plugins* and *Install New Plugin* are made again for the new titles (see *Settings*) and, for *System*, the new row; the time in *Date and Time* is a test value.
- The pictures of the Library in the README (`library.png`, `library-folders.png`) are made again: the background is softer.
- The picture of the themes in the README (`theme.png`) is made again twice: PS3 is black and gold now, and then with the new ribbon. The picture of *Appearance* (`settings-appearance.png`) shows the Default theme under its new name.
- The picture of *Install New Plugin* in the README shows the page as it is now: *File*, *Location* (where the file is), *PLUGINS.TXT* (the one that is edited) and the three switches. The folder and the file name in it are the demo's.
- The picture of *Console Information* in the README is scrolled down to the new rows: *CFW (ARK-5)* with the version of ARK, *Last IP address* and *Free RAM*. The version of ARK, the MAC address and the RAM in it are the demo PSP's (it has no ARK), and the demo has not been on Wi-Fi, so it says *Not recorded yet*.
- The pictures of *Games*, *Library*, *Library with folders* and *Games and Apps* in the README show the new look (rectangular cards, the title as large as in the Library, *Games and Apps* on *Least used*).

## 1.2.0 – Controls, new Settings pages, folders in the Library and time played

### Controls

- **Every button means one thing on every screen:** the D-pad and the analog stick **navigate**, ✕ **confirms**, ○ goes **back**, START opens the
  **options**, SELECT opens the **power menu**, L / R **switch tabs** (or page up / down in a long list). □ and △ are only for extras (mark
  and select all in a list, pause the music).
- **Second buttons for the same job are gone.** △ and □ no longer close popups, *Information*, *Manage Saved Data* or Settings (○ goes back one
  step; from the list of categories it closes Settings). The USB screen is left with Back only. In the File Manager the options are on START.
  The File Manager now follows the PSP's own ✕ / ○ setting like everything else.
- **New: Settings → Controls.** *PSP confirm button* (✕ or ○ – this is the PSP's own setting and changes the XMB too), then **Confirm**, **Back**,
  **Options** and **Power menu**. Confirm and Back can be any of ✕ ○ □ △ (or *as the PSP's setting*); give one the button the other has and the two
  swap, so no button ever has two jobs. Options and Power menu are START and SELECT, either way round. Your choices are kept in `psp5.cfg`.
- **○ cancels before it leaves:** in the File Manager and in *Manage Saved Data* the first ○ clears your marks. In the File Manager, at the top, a
  waiting *Copy* or *Move* is cancelled before ○ leaves.

### Settings

- **A choice opens a popup.** Settings that used to change with ← → (language, CPU speed, regions, ...) show all their values when you press ✕. ↑ ↓
  choose, ✕ takes it, ○ keeps the old one. A stray press can't change a setting any more. Long lists scroll with a thin bar.
- **New: Display and Sound** – the PSP's own **Brightness** (1 to 4, like the XMB), **Backlight auto-off**, **Volume**, **Equalizer** and **AVLS**. They change in the XMB too.
  The backlight time you choose here is **kept**: the PSP forgets it at a restart, so psp5 sets it again every time it starts.
- **New: Network** – opens the PSP's own Wi-Fi screen (the one games show) and brings you back to Settings. The SDK only lets it *choose and
  connect to a saved connection*; adding, editing or removing connections still has to be done in the XMB.
- **New: About** – nickname, model, system software, CFW, psp5 version, MAC address, space left on the Memory Stick (and the internal storage of a PSP
  Go) and the battery (level, voltage, temperature, capacity). The battery temperature shows whole degrees (it showed 3687 °C before: the PSP gives
  hundredths of a degree).
- The category list scrolls. The version and CFW rows are gone from *psp5* (they are in *About*).
- **More room:** Settings and *Information* start 24 px from the screen's edges, like the File Manager, with a wider list of categories and 12 px
  between the columns.
- **Names with capitals:** *File Manager*, *Shut Down*, *Recovery Menu*, *USB Mode*, *Disable UMD Drive*, *Disable Analog Stick*, *Display and Sound*, *About*
  – in all 11 languages, each with its own capital rules (for example *O Zařízení* in Czech).

### Library

- **Folders.** The title at the top of the Library is a button now (like **Sort by**) with four views: **All**, **In folders**, **Outside of folders** and
  **Both in and out of folders**. A folder card shows a folder with its first letter and the tag *FOLDER*; ✕ opens it, ○ goes back. Folders come from
  `PSP/GAME` and `ISO` (a game's folder is the last one before it; `CAT_` and sort digits are dropped, like Categories Lite shows them).
  The view button shows the number of cards (*All: 15*) and the Sort button says *Sort by:*. The Library remembers your sort and view after a restart.
- **The Start button says *Play*** (the Library card's says *Open*).
- **.jso games** (an ISO packed with LZO) show up in the Library, and can be started, deleted and looked at.
- The number of cards at the top is level with the text before it. The PS1 theme's four symbols sit lower, under the cards.

### Time played

- **Each game shows how long you played** – a small pill (a clock and *54h* or *45min*) at the right end of the bottom row, in Games and in the Library.
  Minutes up to an hour, then whole hours. Nothing for a game you never played, the Library card or a folder.
- **It is counted by a small helper plugin** (`psp5time.prx`) that psp5 installs by itself (two lines in `PLUGINS.TXT`, shown as *Games* and *PS1* in
  *Settings → Plugins*; turn them off to stop counting). It counts only while the game runs and the PSP is awake, also for games started from the XMB,
  and psp5 adds the times up at its next start (`playtime.txt`). Games without a disc ID are named by their ISO file name.
  *The maintainer reported on 27 October 2026 that the plugin and the ISO naming work on their PSP.*

### Top bar

- **A battery like the PS5's:** four bars lit from the right (red at 15 % or less) with the terminal on the left. While charging the bolt is back
  left of the battery and the bars above the level light up one after the other.
- **A Wi-Fi sign** while the PSP's Wi-Fi switch is on (it shows the switch, not a connection). *The maintainer reported that it works.* No Bluetooth
  sign on the PSP Go: there is no way for a program to read it.
- The cog wheel is at the very end, then the clock, the battery and the Wi-Fi sign, 12 px apart.

### File Manager and start-up screen

- **The File Manager's folders have the Library's folder picture** now (they were two plain boxes).
- **The start-up screen shows the PlayStation logo**, drawn by psp5 from its outline (not a picture), a quarter of the screen high, in the same grey.
  The logo is Sony's trademark (see the README).

### If something doesn't work

Please [open an issue](../../issues) and tell us what you saw, which PSP you have and which ARK version. The new words are translated into all 11
languages by Claude, not by native speakers – tell us if one sounds wrong.

### Docs

- README and INSTALL describe the buttons by what they do, and have new screenshots.

### Downloads and installing

`psp5-authentic-v1.2.0-ARK5.zip`, `-ARK4.zip`, `-PSPGo-ARK5.zip` and `-PSPGo-ARK4.zip`: see [INSTALL.md](INSTALL.md). They use
the same folders as before (`PSP/GAME/PSP5`, `PSP5`), so they replace an installed 1.1.0 (or 1.0.0, or psp5) and keep your settings and
covers.

## 1.1.0 – the Library card, the battery percentage and a PSP Go fix

### PSP Go

- **PSP Go with ARK-4: the games on the Memory Stick Micro show up now**, together with the ones on the internal storage, when
  psp5 is started from the internal storage. psp5's start file now asks ARK-4 to start it from the internal storage with the
  Memory Stick as `ms0:`. A PSP Go owner with ARK-4 reported that this works. It needs a recent ARK-4. The setting is in every
  download.

### Library

- **The Library is a card in Games now, not a tab.** It is the last card of the Games row, always there, and can't be removed.
  It has its own picture (a game controller in front of a grid of squares) on your theme's colour. It counts as one of the ten
  cards: up to nine games and the Library card.
- **Open and close:** ✕ on the card (or on its **Open** button) opens the Library, **○ closes it** and you are back on the card.
  Inside, the top bar shows a bold **Library** as a title (it can't be selected) and the cog wheel. L and R do nothing there.
- **You can pin the Library card** (START → *Pin to Home*). It moves to the front with the pinned cards but doesn't count as a
  pinned game, so you can now pin 8 games instead of 9. (If you already pinned nine, they stay pinned.)
- START on the Library card shows only **Pin to Home** / **Unpin from Home**.
- **↑ goes to the Sort button first** from every card in the first row (before, only from the cards right under it). ↑ again goes
  to the cog wheel, and ↓ from the cog wheel comes back to the Sort button.
- **No more stutter** just before the card grows into the big card. Its picture was calculated again for every size on the way,
  which a PSP couldn't keep up with; the pictures are now ready-made.
- **The picture is polished:** the controller is now the same on the left and the right (the right side was cut off a little) and it
  is drawn on whole pixels like the other icons, so it no longer wiggles or changes shape while the cards slide.
- With no games at all, *No games found* and where to put them show under the Library card. (The "Home is empty" note is gone.)

### Top bar

- **New setting: Show battery percentage** (*Settings → psp5*, under *Startup animation*), off by default. It shows the real
  battery level as a number (like *87%*) in the clock's font, 5 px right of the battery, so it reads as part of the battery icon.
  The group sits close to the clock.
- The battery and the cog wheel stay where they are at every level; a shorter number just moves away from the clock. The tabs
  make room too (long languages shrink their names a little more).
- **The cog wheel is 5 px further from the battery**, so its highlight no longer covers the charging bolt. With the percentage on
  the cog wheel stays where it was; with it off, it is 5 px further left and the tabs get 5 px less room.
- **A PSP without a battery shows no battery icon and no number** (before, an empty battery was drawn). The cog wheel stays put.

### Settings

- The **Version** row in *Settings → psp5* shows 1.1.0.

### Docs

- The README is rewritten: shorter and clearer, with new screenshots. INSTALL.md has the new download names.

### Downloads and installing

`psp5-authentic-v1.1.0-ARK5.zip`, `-ARK4.zip`, `-PSPGo-ARK5.zip` and `-PSPGo-ARK4.zip`: see [INSTALL.md](INSTALL.md). They use
the same folders as before (`PSP/GAME/PSP5`, `PSP5`), so they replace an installed 1.0.0 (or psp5) and keep your settings and
covers.

## 1.0.0 – the first release of psp5-authentic

Based on **psp5 v1.5.3** by Mille Weber (08Weber), with one goal: to be **as authentic as possible to its home console
counterpart**. Below is everything that changed compared with v1.5.3. Some fixes that psp5 1.5.4 and 1.6.0 made on their side
are in too (marked *also fixed in psp5 1.5.4 / 1.6.0*); they were written again for this code, not copied.

Written by Claude, an AI assistant made by Anthropic. The person who runs the fork has almost no coding experience and tests
every update on their own PSP.

> **Something not working?** Please open an issue and say which PSP and ARK you have.

### Home and menu, the console way

- **Games is now Home.** The Apps tab is gone. Home shows the games and apps you opened last (newest first, up to ten) and the
  ones you pinned (pinned come first, up to nine). A UMD in the drive is always the first card and doesn't count.
- Home is remembered in `home.txt` in psp5's data folder. On the first start it is filled with the ten newest.
- **Library** shows everything in a grid (80 × 44 cards). Its **Sort** button, reached with ↑, opens a popup with two sortings:
  name or recently added. The old white square icon is gone.
- **Start and ⋯ (Options) are real buttons** under the big card: ↓ selects **Start**, ← → move between Start and **⋯**, ↑ or ○
  goes back. START (or ✕ on ⋯) opens the options of the card, START closes them. START no longer starts a game or plays a track.
- **The whole menu works with the D-pad alone** and the hint pills are gone. ↑ from the first thing on a tab selects the
  **top bar** (Games, Library, Music, cog wheel): ← → move, ✕ opens, ↓ goes back down, ○ goes back to the card. L and R still
  switch tabs. △ no longer opens Settings.
- In the Library, ↑ stopped at the Sort button only from the cards right under it.
- **The selected card keeps its size** when the focus is somewhere else (Sort, Start, Options, top bar). Only its white ring
  fades out, and the ring fits the picture exactly.
- The **type tag** (PSP / APP / PSN / PS1 / UMD) sits before the name next to the big card, as on the console. The game id moved
  to the Information screen.
- **Options popup:** Manage Saved Data, Information, Delete, Remove from / Add to Home, Pin to / Unpin from Home.
- **New: power popup on SELECT**, on every tab. Sleep, Shut down, Restart, Recovery menu: the same names and order as at the
  bottom of *Settings → Power and USB*. ✕ does it, ○ or SELECT closes the popup.
- **"Press again to confirm" is gone.** Sleep, Restart, Shut down and Recovery menu run on the first ✕. In *Power and USB* the
  order is now Sleep, Shut down, Restart, Recovery menu.

### Information, Delete and saved data

- **Information** (options popup): where the game is and how much room is left there, file name, size, date, volume name, the size
  of an EBOOT game's whole folder, the unpacked size and ratio of CSO / ZSO / DAX images, and DISC_ID, DISC_VERSION, APP_VER and
  PSP_SYSTEM_VER from the game's PARAM.SFO. The UMD has it too.
- **Delete** (options popup): asks first (*Cancel* is preselected), shows a progress panel you can cancel, then removes the card
  from the list. Only ISO / CSO / ZSO / DAX files and game folders can be deleted: never the UMD, psp5's own folder, SAVEDATA
  or anything else. A cancelled or failed delete never leaves a half game in the list.
- **Manage Saved Data** (options popup): lists every folder in `PSP/SAVEDATA` (also on a PSP Go's internal storage) with its game,
  title, description, size and date. Mark saves with □, select all with △, sort with the same Sort button as the Library (last
  changed / size / name) and delete after a question. `ARK_*` (ARK's settings and `VBOOT.PBP`) is locked and can't be marked or
  deleted.
- The **file manager** and Manage Saved Data share the look and the questions of the rest of the app.

### Backgrounds, themes and startup

- **The console-cover colours have the console's scene:** a wide beam of light from a lamp at the top left, a band of fine golden
  dust with soft bokeh, and a cool haze along the bottom left. Each colour tints them, and the beam is a little softer (60 %)
  than the rest. **PS1–PS4** get the beam and the haze behind their own decorations, but no dust.
- **One background on every screen** (Games, Library, Music, Settings, Information, file managers, USB). Library and Music lay the
  same darkening over it. A game's own picture is shaded at the top, left and bottom so the tabs, cards and text stay readable.
  The old tint a game's icon gave the background is gone.
- The **PS1 symbols** sit in one place (top right) on every screen.
- **Startup:** the grey logo, the health notice (one larger paragraph) over dark out-of-focus circles, then floating dust, and
  then it fades straight into the menu (the still picture at the end is gone). The circles and dust take your theme's colours:
  a Cosmic Red console boots in red.
- The **volume popup** sits 12 px from the top.
- **Faster background:** the part that never changes (surface, glows, beam, haze, dust) is drawn once into video memory and shown
  from there. Only the moving shapes of PS1–PS4 are drawn every frame.

### A cleaner, smoother picture

- **Everything at rest is drawn on whole pixels:** no more borders that are 1 px on one side and 2 px on the other, no soft icons.
  Selection frames are centred on their text, also in long languages.
- **Smooth icons and shapes:** button symbols, L/R key caps, cog wheel, charging bolt, play and seek triangles, music note and
  speaker are pixel-exact anti-aliased images. Rounded rectangles, circles and drawn lines (check marks, PS1 symbols, waves, boot
  logo) have soft edges.
- **Popups look the same everywhere:** one dark, square panel with a soft shadow and a thin frame round the chosen entry. The
  question before a delete is in the middle of the screen; progress panels and toasts use the same style in a lighter colour.
- **Czech** is drawn correctly: psp5 draws the háček and ring marks itself, because the PSP's fonts don't have them.

### Languages and sounds

- **Czech is a new language** (*Settings → psp5 → Language*), so there are 11 now: English, Swedish, German, French, Spanish,
  Italian, Dutch, Portuguese, Russian, Japanese and Czech.
- Titles that are capitalised in English (*Manage Saved Data*, *Disable Pause Game*, *System Storage*) are capitalised the same
  way in the other languages. 64 texts changed in all: those capitals, and shorter wording where a text was much wider than
  the English one.
- **Interface sounds** are WAV files in `PSP5/sounds` (the zips include them): *move*, *select*, *back*, *error* and *boot*. The
  nine old sounds were merged into these five. If a file is missing, psp5 plays the built-in sound of the same name.

### Settings

- The **analog stick** works in Settings: up / down move, sideways acts like ← / → (enter, go back, change a value).
- The **Version** row in *Settings → psp5* shows 1.0.0.
- Strings and code that nothing uses any more are removed (among them *Press again to confirm*).

### Fixes

*Also fixed in psp5 1.5.4 / 1.6.0:*

- **.DAX games** are listed like ISO, CSO and ZSO (also in the file manager, the delete guard and Information).
- **USB mode and the file manager closed at once** when opened with your confirm button. They now wait until every button is
  released. Starting USB no longer gives up when the system reports an error.
- **PSP Go, psp5 started from the internal storage:** games were started as if they were on a Memory Stick (PS1: error 800201A8,
  ISO: freeze). `ef0:` is now always scanned and `ms0:` only when it is a real stick, so games start with the right run levels.
- **PSP Go homebrew on the internal storage** (like the GTA III and Super Smash Bros ports) starts with the run level ARK's own
  menu uses, so ports that read their files from `ms0:` find them.
- **"Boot psp5"** showed as off until the first game was started.
- **A crackle when Settings opened** (the sound played while Settings was still loading).
- **PSP minis icons** are no longer stretched: a square icon keeps its shape on a darker, blurred copy of itself.
- **Wide pictures are drawn in narrow strips**, which the PSP's graphics chip handles much faster (the artwork behind the cards).

*Other fixes:*

- Behind Settings, Information and the delete screens the menu got a stick pushed all the way left and up, so the selection slid
  to the first card with move sounds. It now gets a centred stick.
- A picture that claims a size like 65537 × 65537 could pass the size check (the product wrapped around in 32 bits). The check
  uses 64 bits now.
- Cancelling a folder copy left the half-made folder behind, and the 1000th copy of a file could overwrite the existing one.
- `SETTINGS.TXT` and `PLUGINS.TXT` could lose lines when saving: game lines past the 64th were deleted and a file that didn't fit
  was cut. Long files are kept in full now, and a file that couldn't be read completely is never written over.
- The loader no longer writes through a null pointer when memory runs out; the cards just show no pictures.
- Games and apps in category folders (`PSP/GAME/PS1/<game>` and the like) show up. (This is psp5 v1.5.3's own change.)

### Faster

- Opaque fills, gradients and pictures are drawn without blending. The battery and clock are read once a second, text widths
  are remembered, and blur and JPEG decoding are cheaper. Music tags are cached on the card, so later starts only read new or
  changed files.

### Removed

- The Apps tab and *Move to Apps / Games*, the on-screen hint pills (L R Tab, Settings), the △ shortcut to Settings, the tint a
  game's icon gave the background, and the code and strings that went with them.

### Downloads and installing

`psp5-authentic-v1.0.0-ARK5.zip`, `-ARK4.zip`, `-PSPGo-ARK5.zip` and `-PSPGo-ARK4.zip`: see [INSTALL.md](INSTALL.md). They use
the same folders as psp5 (`PSP/GAME/PSP5`, `PSP5`), so they replace an installed psp5 and keep its settings and covers. The image
cache is rebuilt once.

### Credits and licence

Based on [psp5](https://github.com/08Weber/psp5) v1.5.3 by Mille Weber (08Weber). Written with Claude (Anthropic). GPL-3.0, like
the original; the original copyright notices are kept in the source files. Not affiliated with or endorsed by Sony Interactive
Entertainment.
