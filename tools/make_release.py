# psp5 – made by Mille Weber
"""Packs the two release zips (one per ARK version) from dist/ and dist-ark4/.

    py tools/make_release.py 1.2.0

Makes release/psp5-authentic-v<version>-ARK5.zip and -ARK4.zip, each with
    PSP/GAME/PSP5/EBOOT.PBP     - psp5 authentic as a normal app
    PSP/GAME/PSP5/*.wav         - recorded interface sounds from res/sounds, next to EBOOT.PBP (if they are missing psp5 authentic plays its built-in ones)
    ARK_LAUNCHER/VBOOT.PBP      - the same file, to replace ARK's launcher (see INSTALL.md)
    READ ME.txt                 - the install guide for every device (tools/release-readme.txt with the version and the ARK filled in)
There is one zip per ARK version for every device: the EBOOT is the same on a PSP-1000/2000/3000, a PSP Go and a PS Vita - psp5 authentic finds the
internal storage (ef0:) by itself. Build first with tools/build.sh (ARK-5) and tools/build.sh ark4 (ARK-4).
"""
import os
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# What the PS Vita part of the READ ME says in each zip (@VITA_NOTE@ in tools/release-readme.txt)
VITA_NOTE = {
    'ARK-5': 'This is the right zip for the Vita.',
    'ARK-4': ('For the Vita, use the ARK-5 zip. ARK-4 on the Vita has not\n'
              ' been confirmed yet: the steps below should work with it,\n'
              ' but nobody has told us so far. If you try it, please tell\n'
              ' us how it went (see part 7).'),
}


def readme(version, ark):
    """The READ ME.txt of a zip: tools/release-readme.txt with its @...@ places filled in, with Windows line ends."""
    other_ark = 'ARK-4' if ark == 'ARK-5' else 'ARK-5'
    with open(os.path.join(ROOT, 'tools', 'release-readme.txt'), encoding='utf-8') as f:
        text = f.read()
    text = (text.replace('@VERSION@', 'v' + version).replace('@ARK@', ark).replace('@OTHER_ARK@', other_ark)
            .replace('@OTHER_ZIP@', zip_name(version, other_ark)).replace('@VITA_NOTE@', VITA_NOTE[ark]))
    if '@' in text:
        sys.exit('release-readme.txt has a place that is not filled in: ' + text[text.index('@'):][:30])
    return text.replace('\n', '\r\n')


def zip_name(version, ark):
    return f'psp5-authentic-v{version}-{ark.replace("-", "")}.zip'


def make(version, ark, dist):
    eboot = os.path.join(ROOT, dist, 'PSP', 'GAME', 'PSP5', 'EBOOT.PBP')
    if not os.path.isfile(eboot):
        sys.exit(f'missing: {eboot} - build first')
    out = os.path.join(ROOT, 'release', zip_name(version, ark))
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        z.write(eboot, 'PSP/GAME/PSP5/EBOOT.PBP')
        z.write(eboot, 'ARK_LAUNCHER/VBOOT.PBP')
        z.writestr('READ ME.txt', readme(version, ark))
        for wav in sounds():
            z.write(wav, 'PSP/GAME/PSP5/' + os.path.basename(wav))
    print(out, os.path.getsize(out))


def sounds():
    """The recorded interface sounds (res/sounds/*.wav, made by tools/convert_sounds.py)."""
    folder = os.path.join(ROOT, 'res', 'sounds')
    return sorted(os.path.join(folder, f) for f in os.listdir(folder) if f.endswith('.wav')) \
        if os.path.isdir(folder) else []


if __name__ == '__main__':
    v = sys.argv[1] if len(sys.argv) > 1 else '1.2.0'
    make(v, 'ARK-5', 'dist')
    make(v, 'ARK-4', 'dist-ark4')
