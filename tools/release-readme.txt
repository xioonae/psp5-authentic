=====================================================================
 psp5 authentic @VERSION@  -  the zip for @ARK@
=====================================================================

psp5 authentic is a launcher for the PSP. It shows your games, music
and settings in a style close to a modern home console.

A fork of psp5 v1.5.3, originally made by Mille Weber
(https://github.com/08Weber/psp5), forked by Xioonae.
Made with Claude, an AI assistant.


---------------------------------------------------------------------
 1. IS THIS THE RIGHT ZIP FOR YOU?
---------------------------------------------------------------------

This zip is for devices that run @ARK@.

  * You have @OTHER_ARK@ instead? Use @OTHER_ZIP@.
  * Not sure which ARK you have? On the PSP, go to the XMB (the main
    menu) and press the SELECT button. ARK's menu opens and says
    "ARK 4.x" or "ARK 5.x".
  * No ARK at all? psp5 authentic needs ARK. You can't use it on a
    PSP that has no custom firmware.

Works on:
  * PSP-1000, PSP-2000 and PSP-3000 (ARK-4 or ARK-5)
  * PSP Go (ARK-4 or ARK-5)
  * PS Vita 1000 and 2000 (ARK-5, the standalone install)

It is the same program on all of them. It finds out by itself which
device it is on, and whether there is an internal storage (PSP Go).

What is in this zip:

  PSP\GAME\PSP5\EBOOT.PBP   the program
  PSP\GAME\PSP5\*.wav       its interface sounds (they must stay
                            next to EBOOT.PBP)
  ARK_LAUNCHER\VBOOT.PBP    the same program, for step 4 only
  READ ME.txt               this file


---------------------------------------------------------------------
 2. UNZIP
---------------------------------------------------------------------

Windows: right-click the zip file and choose "Extract All".
Mac: double-click the zip file.

You now have a folder with two folders in it (PSP and ARK_LAUNCHER)
and this file. Keep it open on your computer. You need it in a moment.


---------------------------------------------------------------------
 3. COPY IT TO YOUR DEVICE   (read only the part for YOUR device)
---------------------------------------------------------------------

-- PSP-1000, PSP-2000, PSP-3000 -------------------------------------

 1. On the PSP, open the XMB. Go to Settings, then "USB Connection"
    and press the X button.
 2. Plug the USB cable into the PSP and into your computer.
    The PSP shows up on the computer like a USB stick, with a drive
    letter (for example E:).
 3. Open that drive. Drag the folder "PSP" from the unzipped folder
    onto it.
    If the computer asks to merge folders or replace files, say yes.
    Your games and saved games are not touched.
 4. On the computer, eject the drive (right-click it, "Eject").
    Then press the O button on the PSP and unplug the cable.
 5. To start it: XMB, Game, Memory Stick, psp5 authentic. Press X.

-- PSP Go -----------------------------------------------------------

 1. On the PSP Go, open the XMB. Go to Settings, then "USB Connection"
    and press the X button.
 2. Plug the USB cable into the PSP Go and into your computer.
    The PSP Go shows up on the computer like a USB stick.
    Its internal storage is the drive to use.
 3. Open that drive. Drag the folder "PSP" from the unzipped folder
    onto it.
    If the computer asks to merge folders or replace files, say yes.
    Your games and saved games are not touched.
 4. On the computer, eject the drive (right-click it, "Eject").
    Then press the O button on the PSP Go and unplug the cable.
 5. To start it: XMB, Game, System Storage, psp5 authentic.
    Press X.

 Your games can be on the internal storage, on a Memory Stick Micro,
 or on both. psp5 authentic finds them all.

-- PS Vita 1000 and 2000 --------------------------------------------

 You need ARK on the Vita first. For ARK-5 that is the standalone
 install: FasterARK_psvita.vpk from
 https://github.com/PSP-Arkfive/FasterARK
 Install it as that page tells you, and check that ARK starts.
 @VITA_NOTE@

 1. Connect to the Vita with VitaShell, the way you already move
    files to it (USB cable or FTP).
 2. Open the folder "ux0:pspemu" on the Vita. Copy the folder "PSP"
    from the unzipped folder into it.
    If asked to merge folders or replace files, say yes.
    You should now have this file on the Vita:
    ux0:pspemu/PSP/GAME/PSP5/EBOOT.PBP
 3. Do step 4 below with the Vita's folder
    ux0:pspemu/PSP/SAVEDATA/ARK_01234
    On the Vita, psp5 authentic starts as ARK's launcher.
 4. Start ARK on the Vita as usual. psp5 authentic opens.


---------------------------------------------------------------------
 4. OPTIONAL: START STRAIGHT INTO psp5 authentic (needed on the Vita)
---------------------------------------------------------------------

ARK has a launcher, a menu that opens when you start ARK. You can
replace it with psp5 authentic. Then your device starts right into
psp5 authentic. The old launcher stays as a backup.

 1. On your device's storage, open the folder PSP/SAVEDATA/ARK_01234.
    (PSP and PSP Go: through the USB connection from step 3.
    PS Vita: ux0:pspemu/PSP/SAVEDATA/ARK_01234.)
 2. There is a file called VBOOT.PBP. Rename it to
    VBOOT_ARKMENU.PBP. That is your backup of ARK's own launcher.
 3. Copy VBOOT.PBP from this zip's folder ARK_LAUNCHER into
    ARK_01234.
 4. Open psp5 authentic. On a PSP or PSP Go, "Custom Launcher" in the
    XMB now opens it. On the Vita, start ARK. Then open the
    settings: press up to reach the top bar, right to the cog wheel,
    then X. Go to "psp5 authentic" and turn on
    "Boot into psp5 authentic". Restart the device.

To go back to ARK's own launcher: delete VBOOT.PBP in ARK_01234 and
rename VBOOT_ARKMENU.PBP back to VBOOT.PBP.

To get to the XMB from psp5 authentic: press SELECT and choose
"Exit to XMB".


---------------------------------------------------------------------
 5. THE FIRST START, AND YOUR GAMES AND MUSIC
---------------------------------------------------------------------

The first start takes a moment, because psp5 authentic looks for your
games. After that it starts fast.

Where your files go (on the same storage as the PSP folder):

  PSP games (.iso .cso .zso .dax .jso)   folder  ISO
  Homebrew, PSN games and PS1 games      folder  PSP/GAME
  Music (.mp3)                           folder  MUSIC

On the Vita these folders are inside ux0:pspemu (for example
ux0:pspemu/ISO).

The buttons: the stick or the arrows move, X confirms, O goes back,
START opens the options of the game you selected, SELECT opens the
power menu. (X and O swap if your PSP is set to confirm with O.)

psp5 authentic keeps its settings in a folder named PSP5 at the top
of the storage. It makes this folder itself. Leave it there.


---------------------------------------------------------------------
 6. UPDATING FROM AN OLDER VERSION
---------------------------------------------------------------------

Do step 3 again. The new files replace the old ones, and your
settings and covers stay.

Older versions kept their sounds in PSP5/sounds. The sounds now lie
next to EBOOT.PBP instead. You can delete the old folder PSP5/sounds
if you like. Do not delete the PSP5 folder itself.


---------------------------------------------------------------------
 7. SOMETHING WENT WRONG?
---------------------------------------------------------------------

  * Stuck, or it won't start? Turn the PSP off. Hold the SELECT
    button while you turn it on. ARK then skips its settings and you
    land in the normal XMB.
  * If a start doesn't finish (the PSP froze, for example), the next
    start is a "safe start": no start-up screen and no music.
  * If that fails too, psp5 authentic sends the PSP to the XMB, so it
    can't get stuck.
  * No sound? Check that the .wav files are in the same folder as
    EBOOT.PBP (PSP/GAME/PSP5). Without them psp5 authentic plays its
    built-in sounds.

Still not working? Please open an issue in the project's repository
and tell us:
  1. what you saw,
  2. which device you have (PSP-1000, 2000, 3000, PSP Go or PS Vita),
  3. which ARK version you use (4 or 5).

Full guide: INSTALL.md in the project's repository.
