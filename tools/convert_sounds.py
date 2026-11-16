# psp5 - sound conversion tool
"""Turns recorded interface sounds (MP3, WAV, OGG, ...) into the files psp5 plays.

    python3 tools/convert_sounds.py <folder with the recordings> [output folder]

The folder must hold one recording for each of MOVE, SELECT, BACK, ERROR and BOOT: any file whose name
contains that word (any case, any extension). The output (default: res/sounds) gets move.wav, select.wav,
back.wav, error.wav and boot.wav - 16-bit PCM, 44.1 kHz, mono. tools/make_release.py puts them into the
zips in PSP/GAME/PSP5/, next to EBOOT.PBP, which is where psp5 looks for them first.

What it does to a recording:
  - ffmpeg decodes it and resamples it to 44.1 kHz (the rate the sound engine mixes at)
  - stereo is mixed down to mono (the engine is mono), keeping the average loudness of the two channels
  - silence below -60 dBFS is cut off at the end, and at the start down to 1 ms (decoder delay, reverb tail)
The level is left alone, and psp5 plays the files as they are (there is no gain): make the five recordings as loud as one
another, with peaks around -4 to -8 dBFS like the ones in res/sounds (louder than that would clip when two sounds overlap).
Needs ffmpeg on the PATH.
"""
import array
import math
import os
import subprocess
import sys
import tempfile
import wave

NAMES = ['move', 'select', 'back', 'error', 'boot']
EXTS = ('.mp3', '.wav', '.ogg', '.flac', '.m4a', '.aac')
RATE = 44100
FLOOR = int(32768 * 10 ** (-60 / 20))  # -60 dBFS
LEAD = RATE // 1000                    # keep 1 ms before the first sound


def decode(path):
    """-> (channels, samples) of the file as 16-bit PCM at 44.1 kHz (interleaved when stereo)."""
    with tempfile.TemporaryDirectory() as tmp:
        wav = os.path.join(tmp, 'decoded.wav')
        subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', path, '-ar', str(RATE), '-c:a', 'pcm_s16le', wav],
                       check=True)
        with wave.open(wav) as w:
            ch = w.getnchannels()
            data = array.array('h')
            data.frombytes(w.readframes(w.getnframes()))
    if sys.byteorder == 'big':
        data.byteswap()
    return ch, data


def to_mono(ch, data):
    if ch == 1:
        return data
    if ch != 2:
        sys.exit(f'{ch} channels - only mono and stereo are supported')
    n = len(data) // 2
    mid = [(data[2 * i] + data[2 * i + 1]) / 2 for i in range(n)]
    e_mid = sum(m * m for m in mid) / n
    e_chan = sum(x * x for x in data) / len(data)
    k = math.sqrt(e_chan / e_mid) if e_mid > 0 else 1.0  # same average loudness as the two channels had
    out = array.array('h', (max(-32768, min(32767, round(m * k))) for m in mid))
    return out


def trim(x):
    loud = [i for i, v in enumerate(x) if abs(v) > FLOOR]
    if not loud:
        sys.exit('the recording is silent')
    return x[max(0, loud[0] - LEAD):loud[-1] + 1]


def db(v):
    return 20 * math.log10(max(v, 1) / 32768)


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    src = sys.argv[1]
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = sys.argv[2] if len(sys.argv) == 3 else os.path.join(root, 'res', 'sounds')
    files = [f for f in sorted(os.listdir(src)) if f.lower().endswith(EXTS)]
    os.makedirs(out_dir, exist_ok=True)
    for name in NAMES:
        hits = [f for f in files if name in f.lower()]
        if len(hits) != 1:
            sys.exit(f'{name.upper()}: expected one file with "{name}" in its name, found {len(hits)}')
        ch, data = decode(os.path.join(src, hits[0]))
        mono = trim(to_mono(ch, data))
        with wave.open(os.path.join(out_dir, name + '.wav'), 'wb') as w:
            w.setnchannels(1)
            w.setsampwidth(2)
            w.setframerate(RATE)
            pcm = array.array('h', mono)
            if sys.byteorder == 'big':
                pcm.byteswap()
            w.writeframes(pcm.tobytes())
        peak = max(abs(v) for v in mono)
        rms = math.sqrt(sum(v * v for v in mono) / len(mono))
        print(f'{name:7s} {hits[0]}: {"stereo" if ch == 2 else "mono"} -> mono, {len(mono) / RATE:.3f} s, '
              f'peak {db(peak):.1f} dBFS, rms {db(rms):.1f} dBFS')


if __name__ == '__main__':
    main()
