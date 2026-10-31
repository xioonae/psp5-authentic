# psp5 – made by Mille Weber
"""Packs the release zips (one per ARK and PSP kind) from dist/ and dist-ark4/.

    py tools/make_release.py 1.2.0

Makes release/psp5-authentic-v<version>-ARK5.zip, -ARK4.zip, -PSPGo-ARK5.zip and -PSPGo-ARK4.zip with
    PSP/GAME/PSP5/EBOOT.PBP     - psp5 authentic as a normal app
    ARK_LAUNCHER/VBOOT.PBP      - the same file, to replace ARK's launcher (see INSTALL.md)
    PSP5/sounds/*.wav           - recorded interface sounds from res/sounds (if they are missing psp5 authentic plays its built-in ones)
    READ ME.txt                 - a short note on which PSP and ARK the zip is for
The PSP Go zips have the same EBOOT - psp5 authentic detects the internal storage (ef0:) by itself.
Build first with tools/build.sh (ARK-5) and tools/build.sh ark4 (ARK-4).
"""
import os
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

README = """psp5 authentic v{version} for {ark}
A fork of psp5 v1.5.3, originally made by Mille Weber (https://github.com/08Weber/psp5), forked by Xioonae

This zip is for PSPs running {ark} (PSP-1000, PSP-2000 and PSP-3000).
{other}

Install:
  1. Copy the PSP and PSP5 folders onto your PSP's memory stick (merge folders).
     (PSP5 holds the sounds. Without it psp5 authentic plays its built-in sounds instead.)
  2. Start psp5 authentic from Game -> Memory Stick.

Optional - use psp5 authentic as ARK's launcher:
  rename PSP/SAVEDATA/ARK_01234/VBOOT.PBP to VBOOT_ARKMENU.PBP, then copy
  ARK_LAUNCHER/VBOOT.PBP from this zip into PSP/SAVEDATA/ARK_01234/.

Music: put .mp3 files in the MUSIC folder.
Made with Claude, an AI assistant. Something not working? Please open an issue in the project's repository and say which PSP and ARK you have.
Stuck? Hold SELECT while turning on the PSP - ARK then starts the normal XMB.
Full guide: INSTALL.md in the project's repository.
"""

README_GO = """psp5 authentic v{version} for the PSP Go with {ark}
A fork of psp5 v1.5.3, originally made by Mille Weber (https://github.com/08Weber/psp5), forked by Xioonae

This zip is for the PSP Go (PSP-N1000) running {ark}.
{other}

Install:
  1. Connect the PSP Go with USB (Settings -> USB Connection).
  2. Copy the PSP and PSP5 folders onto the PSP Go's internal storage (merge folders).
     (PSP5 holds the sounds. Without it psp5 authentic plays its built-in sounds instead.)
  3. Start psp5 authentic from Game -> System Storage.

Games: ISO/CSO/ZSO/DAX in ISO, homebrew, PSN and PS1 games in PSP/GAME, music in MUSIC -
on the internal storage or on a Memory Stick Micro (M2).

Optional - use psp5 authentic as ARK's launcher:
  rename PSP/SAVEDATA/ARK_01234/VBOOT.PBP to VBOOT_ARKMENU.PBP, then copy
  ARK_LAUNCHER/VBOOT.PBP from this zip into PSP/SAVEDATA/ARK_01234/
  (on the internal storage).

Made with Claude, an AI assistant. Something not working? Please open an issue in the project's repository and say which PSP and ARK you have.
Stuck? Hold SELECT while turning on the PSP - ARK then starts the normal XMB.
Full guide: INSTALL.md in the project's repository.
"""


def make(version, ark, dist, go=False):
    eboot = os.path.join(ROOT, dist, 'PSP', 'GAME', 'PSP5', 'EBOOT.PBP')
    if not os.path.isfile(eboot):
        sys.exit(f'missing: {eboot} - build first')
    kind = 'PSPGo-' if go else ''
    other_ark = 'ARK-4' if ark == 'ARK-5' else 'ARK-5'
    other = (f'Running {other_ark}? Use psp5-authentic-v{version}-{kind}{other_ark.replace("-", "")}.zip instead.\n'
             + (f'Not a PSP Go? Use psp5-authentic-v{version}-{ark.replace("-", "")}.zip instead.' if go
                else f'PSP Go? Use psp5-authentic-v{version}-PSPGo-{ark.replace("-", "")}.zip instead.'))
    name = f'psp5-authentic-v{version}-{kind}{ark.replace("-", "")}.zip'
    out = os.path.join(ROOT, 'release', name)
    os.makedirs(os.path.dirname(out), exist_ok=True)
    text = (README_GO if go else README).format(version=version, ark=ark, other=other).replace('\n', '\r\n')
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        z.write(eboot, 'PSP/GAME/PSP5/EBOOT.PBP')
        z.write(eboot, 'ARK_LAUNCHER/VBOOT.PBP')
        z.writestr('READ ME.txt', text)
        for wav in sounds():
            z.write(wav, 'PSP5/sounds/' + os.path.basename(wav))
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
    make(v, 'ARK-5', 'dist', go=True)
    make(v, 'ARK-4', 'dist-ark4', go=True)
