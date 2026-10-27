# psp5-authentic – own launcher for PSP with ARK-4/ARK-5 – developer notes

**psp5-authentic is a fork of [psp5](https://github.com/08Weber/psp5) by Mille Weber (08Weber), based on psp5 v1.5.3.** The
goal of the fork is to be as authentic as possible to its home console counterpart. These are the developer notes: what each
change touches in the code. The list for users is in [`CHANGELOG.md`](../CHANGELOG.md). The older sections at the bottom
(1.5.2 and earlier) are the original project's own history; they were written in Swedish and have been translated.

A dark launcher with a row of game cards, the game's picture as the background, and tabs for games (Home) and music; the library is a card of Home.
All graphics are its own. The launcher is a normal app in `ms0:/PSP/GAME/PSP5` and never touches flash0, so the XMB is
always still there (HOME in psp5 = back to the XMB).

## psp5 as ARK's launcher
psp5 replaces ARK-5's own launcher (arkMenu): the file `ms0:/PSP/SAVEDATA/ARK_01234/VBOOT.PBP` is psp5.
ARK's "Custom Launcher" in the XMB then starts psp5, and with **Settings → psp5 → Boot into psp5** the PSP starts straight
into psp5 instead of the XMB.

- **Emergency exit:** hold **SELECT** while the PSP starts – ARK then skips SETTINGS.TXT and you land in the XMB.
  Hold **START** to skip all plugins.
- **Exit to XMB** (Settings → Power and USB) turns the launcher mode off for that session.
- **Back to arkMenu:** copy `ARK_01234/VBOOT_ARKMENU.PBP` to `VBOOT.PBP` (a copy is also in `backup/`).
- An ARK update can overwrite `VBOOT.PBP` – then copy `dist/PSP/GAME/PSP5/EBOOT.PBP` in as `VBOOT.PBP` again.

## Version 1.2.0 (changes after 1.1.0)

The changes after 1.1.0 are noted here; the list for users is under *1.2.0* in `CHANGELOG.md`. Changes after 1.2.0 go in a new section above this one.

- **Brightness 1 to 4, the backlight time kept, three more names** (`settings.c`, `system.c` / `system.h`, `config.c` / `config.h`, `main.c`, `lang.c`; still 1.2.0).
  - *The brightness levels*: the maintainer found that the PSP's own brightness button gives 0 to 3 (the lowest showed as "0" next to the list 1 to 4) and that 4 blacks the screen out, so the first guess (levels 1 to 4 sent to the PSP) was one too high.
    `br_build(now)` lists the PSP's levels 0 to 3 (`BRIGHT_LEVELS` 4, `s_br_levels`) and shows each one plus one (`s_br_text`), like the XMB's 1 to 4; `set_brightness` sends the level itself. A level the PSP reports above 3 (and up to
    `BRIGHT_MAX`, 100) is added after the four, so the row never says anything else than what the PSP has. Nothing in `system.c` changed but its comment; the test build's fake level 3 is shown as 4.
  - *The backlight time is kept* (`g_backlight_off` in `config.c`, seconds, 0 = none chosen; `psp5.cfg` line `backlightoff=120`, written only when a time has been chosen, read only from 10 to 3600 s): the PSP forgets the time of
    `sceImposeSetBacklightOffTime` at a restart, so `set_backlight` keeps what the PSP took as asked (`now == want`, and only when it is a new time: `S.own_dirty`, written by `settings_commit`) and `main()` sets it again right after
    `sys_init` at every start (`if (g_backlight_off) sys_set_backlight_off(g_backlight_off)`). The PSP's own registry is not written for this (its key names are not known to psp5); a time chosen in the XMB later is replaced by psp5's at the next psp5 start.
  - *Names*: Czech *About* is *O Zařízení*, French *À Propos* and Russian *О Системе* (the house style; `S_CAT_ABOUT`). The private `units/caps_test.c` now checks all seven category names (`S_CAT_THEME` ... `S_CAT_ABOUT`) and the exact About names.
  - *Tests* (private, not in git): `settings/settings_test.c` (the brightness lists for readings 0, 1, 2, 3, 4, 7 and 100, the lowest shown as 1 and the highest as 4, choosing 4 sends level 3 and choosing 1 level 0, a PSP that refuses / keeps less / answers -1 or 250; the
    backlight time kept after a change, once, not when the PSP refuses or rounds, not when it is chosen again, and written by `settings_commit`), `units/config_test.c` (the `backlightoff` line: not written until chosen, 10 to 3600 s round trip,
    garbage and out-of-range values read as "none", too-long file), a source check that `main.c` calls `sys_set_backlight_off(g_backlight_off)` after `config_load` and `sys_init`.
  - *Docs*: the "not tried on a PSP" notes are gone from the README, INSTALL, CHANGELOG and these notes; they say instead that an issue is welcome. `INSTALL.md` has the *Covers* section the README pointed to (it was missing), and the messages of
    `tools/covers.py` are in English.
- **New screenshots for 1.2.0** (`docs/screenshots/`: 16 PNGs of 960 x 544, ~3 MB in all; `README.md`; `boot.c`). They replace the eight of 1.1.0 (`settings.png` is gone, `settings-controls`, `settings-display`, `settings-about` and
  `settings-theme` are new, as are `startup-logo`, `library-folders`, `information`, `file-manager` and `power-menu`). The pictures are the 480 x 272 frames of PPSSPP's software renderer (`PPSSPPHeadless`, v1.20.4, built from source), enlarged 2 x with
  Lanczos. The app is a `-DPSP5_TEST` build run over an invented memory stick (14 games and apps with drawn pictures, in folders too, ten saves, six MP3 files with album art, played times); the buttons come from `ms0:/PSP5/test.txt` as always
  (`battery 82`, `charging 1`, `wifi 1`, `shot NAME` ...), and `psp5.cfg` sets `batterypct=1`, `theme=` / `themeart=` and `libview=`. Things the test build fakes are on the pictures: the clock is the time of the capture, the battery, the volume (18) and the
  brightness (3) are test values, and the model in *About* is the one from `fake.txt`. The PPSSPP side (a few lines in its `Headless.cpp` that write the displayed frame to `NAME.ppm` when `NAME.req` appears, and one line in its flash0 reader, which
  refused `sceIoOpen` with PPSSPP's own "quiet" flag, so no font could be read in the headless build) and the generator of the memory stick are **not in git**, like the tests.
  - *The test build counts frames in the start-up screen* (`boot.c`, `#ifdef PSP5_TEST`): `t = frame / 60` instead of the clock, so that a script's `wait` and `shot` fall on the same moment of the intro every time (the real build uses the clock).
    Nothing else changed in `boot.c`; the normal build is the same as before.
- **Version 1.2.0**: `PSP5_VERSION` in `settings.c` (shown in *About*), the download names in `INSTALL.md`, the default of `tools/make_release.py`. *Capitals:* *USB Mode*, *Shut Down*, *Recovery Menu*, *Disable UMD Drive* and
  *Disable Analog Stick* are in title case now, in all languages (`lang.c`, checked by the private `units/caps_test.c`). *The test build* (`-DPSP5_TEST`, `tools/build.sh test`) fakes the Impose settings (`s_test_impose`: volume 18, brightness 3,
  equalizer off, AVLS off) and the backlight timer (120 s) in `system.c`, so that *Settings → Display and Sound* can be looked at and used in PPSSPP, which has no Impose service and no ARK. Nothing of this is in the normal build.
- **The battery temperature** (`system.c`: `battery_celsius`, used by `sys_about`): `scePowerGetBatteryTemp` gave 3687 on the maintainer's PSP, which is hundredths of a degree (36.87 °C), not the degrees that the SDK says. From 100 up the number is
  hundredths and is rounded to whole degrees (`raw / 100 + (raw % 100 >= 50)`); below 100 it is taken as it is; a negative one is -1 (the row is left out). Tested by cutting the function out of `system.c` (23 checks, from 3687 to
  `INT_MIN`; 9 mutation runs: all caught except one that changes nothing (the test for a negative number) and one hole that was closed: that `sys_about` really calls the conversion, which the test script checks in the source). The PSP that
  gave 3687 is not known by model.
- **The file manager's folders** (`files.c` `draw_icon`, `gfx.c` `gfx_folder_picture`, `icons.c` `icon_folder_pic_at`): the folder of a row is the picture of the Library's folder cards (`ICON_FOLDER`), 14 px high and 19 wide (`h * 1.38`), its
  top left corner on the whole pixel (x, 5 px below the top of the row), in the old folder's light grey; `gfx_folder_icon` and `gfx_folder_picture` share `folder_draw` (one image from the cache, two rounded boxes if there is no room), and
  `icon_folder_pic` is built on `icon_folder_pic_at` (the same numbers as before). The drive icon of the file manager is not a folder and is as it was. Tests (private): the same pixels as the folder of a card of that height (heights 10 to 40),
  the colour and alpha, whole pixels, the fallback, the rows' positions with and without marks and while the list fades in; 17 mutation runs, all caught after three holes were closed (the odd pixel when a card's folder is centred, the
  tab in the fallback, the fade of the row).
- **The start-up logo** (`icons.c` / `icons.h`: `ICON_LOGO`, `icon_logo_width`, `icon_logo_outline`; `gfx.c` / `gfx.h`: `gfx_logo`; `boot.c`; `tools/trace_logo.py`). The grey "psp5" made of strokes is replaced by the PlayStation logo, drawn from code.
  - *The outline*: three solid shapes (the P with its stem, the left swoosh, the right swoosh), 49 + 45 + 48 corners in half pixels of a 2000 x 1544 picture (`LOGO_UNITS_W` 4000 x `LOGO_UNITS_H` 3088; `logo_len`, `logo_pts`,
    570 bytes). `tools/trace_logo.py picture.png [eps]` made them: marching squares on the alpha channel (threshold 0.5, a 2 px empty margin so that a logo that touches the edge of the picture still gives closed outlines),
    Douglas-Peucker with 1.0 px of the picture (142 points), coordinates rounded to half pixels, every shape the same way round (the script refuses a picture with a hole in a shape). It reproduces the table in `icons.c` exactly.
    The picture is not in the repository and nothing reads it at run time.
  - *The image* (`logo_render`, `ICON_LOGO` in `icon_render`): the exact area of every pixel inside the outline, by the method of font-rs: each edge that crosses the pixel row adds the share of a pixel that is covered right of it to
    `acc`, and the running sum along the row is the coverage (`logo_edge`; `acc` has `tw + 2` floats on the stack, so the image can be up to `ICON_LOGO_MAX_W` = 128 wide; a wider one comes out empty). Edges are
    anti-aliased exactly (no sampling), so a straight edge moves evenly when the logo is put at a fraction of a pixel. It costs one pass over 142 edges and 90 x 70 pixels, once.
  - *`gfx_logo(x, y, h, color)`* draws it: `icon_logo_width(h)` wide (88 at 68), one transparent pixel round it (image 90 x 70, the top left corner on the whole pixel x - 1, y - 1), cached like the other icons (`cache_get(ICON_LOGO, ...)`
    and `icon_blit`: one sprite), so it is one draw call a frame. With no room in the cache, or an image over 96 px (a logo over 74 high), the outline of the three shapes is stroked (2 px, `gfx_stroke`).
  - *`boot.c`*: `LOGO_H` is `SCREEN_H / 4` = 68, the logo is centred (196, 102), colour RGBA(178, 178, 180, alpha) with the same fade as before (`T_LOGO_IN` .. `T_LOGO_OUT`). `build_logo`, the stroke paths and
    `Path` are gone.
  - *Tests* (private, not in git): `icons/logotest.py` (252 checks: the box and the width at every height, the outline (counts, the corners fill the box, the same way round, the shapes apart), the image against the exact
    area of every pixel (Sutherland-Hodgman, worst pixel 0.006) at four sizes and offsets and against the reference picture shrunk to the same size (worst 0.09), three shapes, a ramp of coverage values, a straight
    edge at nine offsets, the same area at every offset and size, the 128-wide limit, nothing written after the image); `gfx/logodraw_test.c` (the outline fallback with a full cache and at 100 high, one sprite, the colour and the
    alpha, pixel for pixel the image of `icon_render`, whole pixels at 40 places, 600 frames of every size and place); `gfx/boot_test.c` (the logo is 88 x 68 at 196, 102, grey, one draw call, fades in and out steadily).
    31 mutation runs on the logo code: all caught, except one that changes nothing (an edge that ends exactly on a pixel row adds an area of 0) and one hole that was found and closed (the outline in the
    fallback could be drawn with half the alpha).
  - *The logo is Sony's trademark*: the README no longer says that nothing from Sony is used.
- **Settings and Information: the same sides as the file manager** (`settings.c`, `info.c`). `LEFT_X` 24 (was 32), `LEFT_W` 136 (124), `RIGHT_X` 172 (176), `RIGHT_W` 284 (280): 24 px from the left edge, 12 px between the columns
  (20 before), the settings to 456 as before (the list of `files.c` and `saves.c` is 24 .. 456 too). The title is at `LEFT_X + 4` = 28, where the title of the file manager is; the category names start 8 px inside the frame. In
  `info.c` the game's picture is `LEFT_W` wide and `floorf(w * 80 / 144 + 0.5f)` high (76: the shape of an ICON0, 144 x 80). Tests (private): the constants and where the frames, the title and the first category name
  are drawn (`settings_test.c`), the picture and the title (`info_test.c`).
- **○ cancels before it leaves** (`files.c`, `saves.c`). `files_update`: ○ clears the marks if there are any (`marked_count`), else, when `can_go_up()` is false (the top: a single drive's root, or the list of drives) and
  a copy / move is waiting (`s_clip_n`), clears that (`clear_clip`), else goes up or, at the top, leaves (`go_up`, which asks `can_go_up` now). `saves_update`: ○ on the Sort button goes to the list, then clears the marks
  (`marked_count`), then leaves. The back sound plays for the first two. (Tests, private: `files/popup_test.c` and `saves/saves_test.c`: marks alone, in a folder, a waiting copy at the top and in a folder, both, the options popup open,
  two drives, the Sort button, the delete question.) 12 mutation runs on these, all caught.
- **Capitals** (`lang.c`). `S_FILES` is "File Manager". The names that are in title case in English are in title case in every language, each with its own small words (the test lists them): `S_CAT_DISPLAY_SOUND` ("Skärm och
  Ljud", "Écran et Son", "Pantalla y Sonido", ...), `S_FILES` ("Gestionnaire de Fichiers", "Gestor de Archivos", "Gestione File", "Gestor de Ficheiros", "Файловый Менеджер", "Správce Souborů"), `S_HOME_REMOVE` in Swedish ("Ta Bort från
  Hem"). `units/caps_test.c` (private) checks a list of names in all languages (title case with the language's small words; German's participles are small) and the exact words; it was found by comparing every short name
  with its translations (8 mutation runs, all caught). Language names of regions that keep their own rule (*Amérique latine*, *Spojené království*) are left as the language writes them.
- **Settings → Network** (new `wifi.c` / `wifi.h`, `netstub.S`; `settings.c`, `settings.h`, `main.c`, `Makefile`, `lang.c`).
  - *What it is*: `CAT_NETWORK` has one action row, *Wi-Fi connection* (`SA_WIFI`). `main.c`'s `run_action` stops everything (`app_stop`), runs `wifi_mode()` (`gfx_init`, `wifi_dialog(&s_running)`,
    `gfx_term`) and rebuilds the app, like the USB and file-manager modes. Unlike those it **returns to Settings**: `in_settings` stays 1 and `settings_open` is not called, so the category and the row are
    where they were (`S` is not reset; the USB, file-manager and clear-cache actions still end on the main screen). A result of -1 shows `S_WIFI_FAILED` through the new `settings_notice(msg)`: `S.toast` /
    `S.toast_time` (2.8 s), drawn by `settings_draw` with `popup_toast` and the same fade as the toast of `ui.c`.
  - *`wifi_dialog`*: `sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON)` and `_INET` (an "already loaded" answer, 0x80111102, counts as loaded and is not unloaded), `sceNetInit(0x20000, 0x20, 0x1000, 0x20, 0x1000)`,
    `sceNetInetInit()`, `sceNetApctlInit(0x1600, 42)` (the numbers games use), then `sceUtilityNetconfInitStart` with `PSP_NETCONF_ACTION_CONNECTAP`, `hotspot` and `wifisp` = 1 (all saved kinds can be chosen),
    `buttonSwap` and `language` from the system (the dialog is the system's: it does not follow psp5's own Controls), and the thread priorities 17 / 19 / 18 / 16 that the on-screen keyboard
    uses. It is driven like `osk_input` in `files.c`: `draw_frame` (the theme's backdrop) then `gfx_end_with(step)`, where `step` calls `sceUtilityNetconfUpdate(1)` while the dialog is VISIBLE, `ShutdownStart` at
    QUIT and ends at NONE. When `*running` goes to 0 (HOME) up to 120 more frames are given for the dialog to end. Result: `data.base.result == 0` is 1 (a connection was made), else 0; -1 when a module,
    the network libraries or the dialog would not start; then whatever was started is stopped, in the other order (`sceNetApctlTerm`, `sceNetInetTerm`, `sceNetTerm`, the modules this call loaded).
  - *What the SDK offers* (`psputility_netconf.h`): three actions only (`CONNECTAP`, `DISPLAYSTATUS`, `CONNECT_ADHOC`). **Nothing for adding, editing or removing a connection.** The maintainer asked for "the screen that lets you
    add, remove or edit Wi-Fi, like in apps": this is the dialog apps have, which (as far as the SDK says) chooses among saved connections.
  - *Why `netstub.S`*: linking the SDK's `libpspnet_inet` for `sceNetInetInit` / `sceNetInetTerm` makes `psp-fixup-imports` print "could not fixup imports, stubs out of order" (it does so in a three-line program too,
    in every link order tried; it is the library's `sceNetInet.o`), and the build must have no warnings. The two calls are imported by hand (`STUB_START "sceNetInet", 0x00090000, ...` with the numbers 0x17943399 and
    0xA9ED66B9 read from `libpspnet_inet.a` with `objdump -s -j .rodata.sceNid`): the same module, attribute (0x0009, as the library's own header has it) and numbers, so it is the same import. `libpspnet` and
    `libpspnet_apctl` are linked by the SDK's default libraries already.
  - *Tests* (`host-tests/wifi/wifi_test.c`, 27 checks; stand-in headers in `wifi/inc`): the real `wifi.c` against stand-ins that log every call: the order of the calls for a connection made and for a dialog closed without one,
    what the dialog is asked for (action, hotspot, WISP, the system's button setup and language, the thread priorities), each of the six steps failing (what was started is stopped, nothing else, -1), "already loaded"
    modules left loaded, HOME during the dialog, and a dialog that never ends (the wait is bounded; the stand-in's frame limit makes a loop that does not end *fail* the test instead of hanging it).
    `settings/settings_test.c`: the category, the action it returns, the position after Display and Sound.
- **Settings → About** (`settings.c`, `system.c` / `system.h`, `lang.c`).
  - *System*: `sys_about(SysAbout *)` asks the PSP for everything at once: `sceUtilityGetSystemParamString(PSP_SYSTEMPARAM_ID_STRING_NICKNAME)`, `sceKernelDevkitVersion()`, `sceWlanGetEtherAddr`, the battery
    (`scePowerIsBatteryExist`, `GetBatteryLifePercent`, `IsBatteryCharging`, `GetBatteryVolt` (mV), `GetBatteryTemp` (°C), `GetBatteryRemainCapacity` / `FullCapacity` (mAh); -1 where it does not say), and the
    free / total space of `ms0:` (and `ef0:` on a PSP Go) by `sceIoDevctl(dev, 0x02425818, ...)`, the call the file manager makes (a copy of the few lines: `files.c` is built without `system.c` in the tests).
  - *The rows* (`fill_about`, `CAT_ABOUT`, the last category): Nickname (only when there is one), Model, System software (`%u.%u%u` from the devkit: 0x06060110 is 6.61), CFW (`ARK-4` / `ARK-5`, only with ARK),
    psp5 (`PSP5_VERSION`, now one define), MAC address, Memory Stick (`S_ABOUT_FREE_OF`: "12.3 GB free of 29.8 GB"; MB under a gigabyte), System Storage (PSP Go), Battery ("87 %" and ", charging"),
    voltage ("4.12 V"), temperature ("31 °C") and capacity ("1860 / 2200 mAh"). Every row is an `IT_INFO` (selectable, so the list scrolls like the others; nothing can be changed). `fill_about` is called when the
    menu is built and every time the category *comes into view* in the left column (`settings_update`, with the chosen row kept inside the list if it got shorter).
  - *Tests* (`settings/settings_test.c`; `sys_about` is a stand-in): every row and value in order, four devkit numbers, what is left out (no nickname, MAC, ARK, battery, charging; a battery that only says its level), a PSP
    Go's internal storage, MB / GB, the list scrolling to the last row, being asked again when it comes into view, the chosen row kept in range, and Network / About in all 11 languages on whole pixels.
- **Settings → Display and Sound: Brightness and Volume** (`settings.c`, `system.c` / `system.h`, `lang.c` / `lang.h`: `S_VOLUME`, `S_BRIGHTNESS`; `S_VERSION` is gone).
  - *System*: `sys_volume` / `sys_set_volume` (`PSP_IMPOSE_MAIN_VOLUME`, 1; 0 to 30 as `osd.c` reads it) and `sys_brightness` / `sys_set_brightness` (`PSP_IMPOSE_BACKLIGHT_BRIGHTNESS`, 2) are two more
    `impose_get` / `impose_set` pairs, answering with what the PSP says afterwards. The PSP counts the brightness levels 0 to 3 (see *The brightness levels* above: this was first a guess of 1 to 4).
  - *Rows* (`CAT_DISPLAY`, in this order: Brightness, Backlight auto-off, Volume, Equalizer, AVLS): Volume is `IT_CHOICE` on `s_volume` with the 31 numbers 0 to 30 (`s_vol_text`), shown when the PSP says 0 to 30;
    its popup (more than `POP_ROWS`) scrolls with the bar. Brightness (`br_build`): the list is the PSP's levels 0 to 3 shown as 1 to 4, plus the PSP's own level after them when it is above 3 (`s_br_levels`, `s_br_item->nopts`), shown
    when the PSP says 0 to `BRIGHT_MAX` (100); after a change the list is rebuilt from what the PSP says (a PSP that keeps less than asked, or answers -1 or a number out of range, leaves the row as it was).
  - *The psp5 category lost its Version and CFW rows* (about the version in two places): the version and the CFW are in *About* only; `system_name()` and `S_VERSION` are removed.
  - *Tests* (private, not in git): the rows and their order, eleven brightness readings and the lists they give, choosing, a PSP that refuses / keeps less / answers -1 or 250, the volume's 31 values, 0 and 30, volume 31
    refused. 24 mutation runs on the new code: two holes found and closed (a brightness list edge, and a volume row that stayed in the list by coincidence because the next row also had the value 0).
- **Settings → Display and Sound** (`settings.c`, `system.c` / `system.h`, `lang.c` / `lang.h`).
  - *System* (`system.c`): `sys_backlight_off()` / `sys_set_backlight_off(secs)` are `sceImposeGetBacklightOffTime` / `sceImposeSetBacklightOffTime` (user calls in `libpspuser`: any program may
    make them; "30 to a lot of seconds" in the SDK); `sys_equalizer` / `sys_avls` and their setters go through `impose_get` / `impose_set`: the kernel's `sceImposeGetParam` (NID 0x531C9778; 0xDC3BECFF in
    6.60 / 6.61 as `osd.c` has it) and `sceImposeSetParam` (**0x810FB7FB**, read from the SDK's stub library `libpspkernel.a` with `objdump -s -j .rodata.sceNid`), found with `sctrlHENFindFunction("sceImpose_Driver",
    "sceImpose_driver", nid)` and called with `kuKernelCall`, as the volume bar does; the parameters are `PSP_IMPOSE_EQUALIZER_MODE` (4) and `PSP_IMPOSE_AVLS` (0x10) of `pspimpose_driver.h`. Every setter
    answers with what the PSP says afterwards, and -1 when there is no such call. **Nothing here has run on a PSP**; the values of the equalizer (0 = off, 1 .. 4 = Heavy, Pops, Jazz, Unique) are a guess.
  - *The category* (`CAT_DISPLAY`, after Controls): rows only for what the PSP answers (`sys_backlight_off() >= 0`; equalizer 0 .. 4; AVLS 0 / 1; otherwise `S_UNAVAILABLE` as an info row). All three are `immediate`
    (no file) and have a `set` hook: `set_backlight`, `set_eq`, `set_avls` (a toggle's ✕ calls `it->set(!value)` now). The time list (`bl_build`): 30, 60, 120, 300, 600, 900 s plus the PSP's own value in its
    place when it is none of those (so a PSP that rounded or that was set in the XMB shows what it has); labels "30 s" / "1 min" (`S_UNIT_SEC`, `S_UNIT_MIN`), rebuilt after every change because the PSP may have kept
    another value than asked for (`s_bl_item->nopts`).
  - *Tests* (`settings/settings_test.c`; stubs `sys_backlight_off` ... with `s_sys_refuse` and `s_bl_round`): the rows and their order; eight PSP times and the list each gives; choosing through the popup; a PSP that refuses
    and one that rounds; the equalizer's five values and modes; AVLS on and off and refused; rows left out (no timer, a mode that is not one of five, an AVLS value that is not 0 / 1, nothing at all); all three
    rows and the popups in 11 languages on whole pixels.
- **Controls: one name per button, chosen in Settings** (new `controls.c` / `controls.h`; `main.c`, `ui.c`, `saves.c`, `info.c`, `files.c`, `gameops.c`,
  `settings.c`, `config.c`, `gfx.c`, `system.c`, `lang.c`, `Makefile`).
  - *The idea*: the screens only know **names**: `PSP_CTRL_CROSS` = confirm, `PSP_CTRL_CIRCLE` = back, `PSP_CTRL_START` = options, `PSP_CTRL_SELECT` = the power menu,
    `PSP_CTRL_SQUARE` / `PSP_CTRL_TRIANGLE` = what a screen has besides (mark, select all, pause the music), L / R = tabs, the D-pad and the stick = navigate. The pad
    is turned into them once, in `main.c`'s `read_pad`: `in->pressed = controls_map(raw & ~old)` and `in->held = controls_map(raw)`; `old` / `s_raw` stay raw
    (so a button held while a setting changes does not count as a new press; `old = s_raw` replaces the old `old = in.held`). `ui.c`, `saves.c`, `info.c`, `settings.c`
    and `files.c` therefore no longer know `confirm_circle` (the field, the `ui_init` / `info_open` / `saves_open` argument and `S.ok` / `S.back` are gone;
    `gameops_delete_start(e)` lost its two button arguments): they test `PSP_CTRL_CROSS` and `PSP_CTRL_CIRCLE`. Only the symbols on the screen need the real buttons:
    `controls_glyph(name)` gives the `BTN_*` of the physical button behind a name (hints in `saves.c` and `files.c`, the cancel hint of `popup_progress` in `saves.c`,
    `files.c` and `gameops.c`, the USB screen in `main.c`). `ui.c` itself does not include `controls.h` (it needs nothing from it).
  - *`controls.c`*: `g_ctl_confirm` / `g_ctl_back` (`CTL_PSP` = the PSP's own setting, or `CTL_CROSS .. CTL_TRIANGLE`), `g_ctl_options` (0 = START opens the options,
    1 = SELECT does), `controls_psp_assignment(circle)` (the PSP's own ✕ / ○ setting, told at start by `app_start` and by Settings). `resolve()`: confirm and back
    are the chosen button or the PSP's; if they are the same button (a damaged `psp5.cfg`) the one that follows the PSP gives way, else back does (`other_than`:
    the first of ○ ✕ □ △ that is not the other); the two buttons that are free take SQUARE and TRIANGLE in the order □ △ ✕ ○ (so with the usual setup nothing moves). The
    result is always one-to-one (tested for all 2 x 2 x 5 x 5 setups). `controls_set_confirm` / `controls_set_back` swap when one button would get two jobs (also when
    confirm goes back to the PSP's setting while back sits on that button). `controls_code` / `controls_from_code`: the words in `psp5.cfg`.
  - *The extra bindings that went* (so a meaning has one button): △ / □ as a second back in `saves.c` (the Sort popup and the question), `info.c` and the options
    popup of the cards (`ui.c`: `back = CIRCLE | START` now; the options button closes what it opened, so does SELECT on the power popup), △ as a second close in `settings.c`
    (from the list of options it closed all of Settings), ✕ as a second way out of the USB screen (`main.c`), the file manager's △ for the options (now START; its
    menu closes with back or START), `files.c`'s direct ✕ / ○ (it ignored the PSP's own setting). Kept on purpose: □ pauses the music (it pauses without choosing
    the track under the cursor, which ✕ does not), □ marks and △ selects all in the lists, L / R page in the file manager.
  - *Settings* (`settings.c`): the category **Controls** (`CAT_CONTROLS`, after Theme): *PSP confirm button* (`IT_CHOICE` on `s_psp_circle`, `immediate`: no file; `set_psp` calls
    `sys_set_confirm_is_circle`, takes the answer and tells `controls_psp_assignment`; it is read again at every `build`, so the XMB or a game having changed it is seen), *Confirm*
    and *Back* (`set_confirm` / `set_back` = the swapping setters, `glyph_confirm` / `glyph_back`), *Options* and *Power menu* (both on `g_ctl_options`, the values START / SELECT either
    way round). New `Item` fields: `glyph(opt)` (a `BTN_*` drawn with the value: at the end of the row, and before the word in the popup; the word is the PSP's name for it:
    Cross / Circle / Square / Triangle, `S_BTN_*`), `set(opt)` (how a value is set when it is more than storing it) and `immediate`. The left column
    now scrolls (`VIS_CATS` = 6 shown, `S.cat_top`, `S.cat_scroll`, rows fade at the ends, a chevron says there are more): the 7 categories do not fit above 236 px and
    three more are planned.
  - *System* (`system.c`): `sys_confirm_is_circle()` (moved from `main.c`: `sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP)`, 0 = ○ confirms) and
    `sys_set_confirm_is_circle(circle)`: the SDK says of `sceImposeSetLanguageMode(lang, button)` that the values are not known, so it reads `sceImposeGetLanguageMode` and the system
    parameter first and takes the two to mean the same when they are equal (else it writes the opposite number), writes, reads the same service again and answers with that.
    **Never run on a PSP**; a PSP that ignores it just shows the old value. `sceImposeSetLanguageMode` is in `libpspuser` (no new library). *Changed later:* the Impose service alone did not
    last (it is what running programs ask; the XMB's setting is in the registry), so `sys_set_confirm_is_circle` now also writes the registry (`reg_set_int("/CONFIG/SYSTEM/XMB", "button_assign", 0 = ○ / 1 = ✕)`:
    `sceRegOpenRegistry` with a `RegParam` for "/system" (`regtype` 1, `unk2` / `unk3` 1, mode 2), `sceRegOpenCategory`, `sceRegSetKeyValue` (4 bytes), `sceRegFlushCategory`, `sceRegCloseCategory`,
    `sceRegFlushRegistry`, `sceRegCloseRegistry`: the calls the XMB and the homebrew button swappers make; `-lpspreg` is new in the `Makefile`), then tells Impose as before (calibrated), and answers with the system
    parameter if it now says what was wanted, else with Impose's answer, else with the old value. **Never run on a PSP.** Tested against a stand-in system (the function is cut out of `system.c` by the test script,
    42 checks: the order and the arguments of the registry calls, both buttons, an inverted Impose service, a parameter that only follows after a restart, each registry step refusing, no Impose, nothing working; 25 mutation runs, all caught).
  - *Hints with START / SELECT* (`gfx.c`): `BTN_START` / `BTN_SELECT` are pills (28 / 34 px wide, 14 high so the edges are whole pixels; the word in bold size 0.34, fitted) and
    `gfx_button_width(kind)` is how much room a hint gives its symbol (13 for the round ones); `files.c`'s hint row uses it (width and the step to the word).
  - *`psp5.cfg`* (`config.c`): `confirm=psp|cross|circle|square|triangle`, `back=` (the same), `options=start|select`; a file with the same button for both reads back
    as back = the PSP's. The buffers are 384 bytes now (they were 256 / 192).
  - *Tests* (`host-tests`, not in git): `units/controls_test.c` (new, 111 checks: the usual and the swapped PSP setup, all 12 pairs of buttons with what is left over, 100 setups one-to-one,
    START / SELECT, the swaps in both directions and back to the PSP's setting, damaged pairs, the words); `units/config_test.c` (the three keys, 25 pairs, a damaged file);
    `settings/settings_test.c` (the Controls category row by row, the swaps through the popups, what is written, the system refusing, the symbols drawn - `rec_button` records
    `gfx_button` as `rec_round_rect` records the scroll bar -, the scrolling column, all 11 languages on whole pixels); `ui/popup_sig.h` records `gfx_button` too (`sig_btn`,
    wrapped through `$WRAP`) so that `saves_test`, `files/popup_test.c` and `gameops/gameops_test.c` check which symbol each hint and each cancel hint shows, in four or five
    button setups; `gfx/gfxtest.c` (the pills: width, height, whole pixels, the word in the middle); `ui/menu_test.c`, `saves/saves_test.c` send the physical buttons through
    `controls_map` as `main.c` does (`press()`), so the swapped PSP setup is still tested end to end; `gameinfo/info_test.c` includes `info.c` without a path
    (`../../psp5/src/info.c` only worked with a repository folder called `psp5`). `run_all.sh`: `controls.c` is linked wherever `gfx.c` is.
- **Settings: a choice is picked from a popup** (`settings.c`). `IT_CHOICE` items used to change with ← → (and ✕ stepped to the next value); now ✕ calls
  `pop_open(cat, item)`, which sets `S.pop`, `S.pop_cat/item`, `S.pop_sel` (the current value) and `S.pop_top` (the first value shown: the current one in
  the middle, `clampf(sel - vis / 2, 0, n - vis)`, and `S.pop_scroll` = `S.pop_top` at once so nothing slides when it opens). While `S.pop` is set, `pop_update` gets the
  buttons (↑ ↓ with the same repeat as the lists, ✕ takes the value through `choice_set`, ○ closes it; nothing else does anything, not even △). `choice_set` is the code that
  used to be in the ← → branch (`mark_dirty`, and a new language: `lang_apply`, the CJK font, `build(1)`); choosing the value the setting has is no change and marks nothing.
  ← is "back to the categories" on every kind of row now (it changed the value of a choice before). `S.pop_anim` fades the popup in and out (the list behind it keeps
  scrolling into place: the "keep the chosen row in view" code at the end of `settings_update` runs while the popup is open too; an early `return` there left the rows
  half way between two pixels).
  *Drawing* (`draw_popup`): `popup_dim`, `popup_panel` (`POPUP_DARK`) and `popup_choice` like `draw_menu` in `ui.c`; in the middle of the screen, rising into place like the power popup;
  `w` = the widest value + 32 (at least 200, at most the screen less 40, rounded up to an even number so the panel is on whole pixels), `POPUP_ITEM` per row; at most `POP_ROWS` = 8
  rows (the clock list has 8, the regions 14, the languages 12); a longer list is clipped (`gfx_clip`) to the rows area and has a 3 px bar 6 px from the right edge
  (track and thumb rounded to whole pixels; the thumb is at least 12 px).
  *Tests* (`host-tests/settings/settings_test.c`, not in git): opening with the current value chosen and in the middle (four choices, three start values each), ← → and the stick sideways doing
  nothing, ↑ ↓ to both ends (the list follows, one step per press, no wrapping), the stick, ○ keeping the value, ✕ taking it (ARK vs psp5's own flag, the saved file), the same value marking
  nothing, △ / START doing nothing, the fade, a held direction repeating, the list scrolling on while the popup is open, the language (texts change at once), and the whole-pixel audit of the popup
  of every choice at every value (`rec_round_rect` reads the bar, which the audit does not see).

- **Top bar: new battery, Wi-Fi sign, the cog wheel at the end** (`ui.c`, `icons.c/.h`, `gfx.c/.h`, `testrun.c/.h`, `Makefile`).
  - *Layout* (`draw_top_bar`): from the right edge to the left, `TOP_GAP` = 12 px between the things. The cog wheel (`COG_R` 7:
    `gx = SCREEN_W - MARGIN - 7`, so its right edge is on the margin, where the clock's used to be; its frame is `popup_choice(gx - 15, ..., 30, ...)`
    as before). The clock, right-aligned at `gx - 7 - 12` (= 430). The number, when "Show battery percentage" is on and the level is known:
    its right end 12 px before the clock text's left end, its left end `BAT_GAP` = 5 px after the battery. The battery (`BAT_W` 23: terminal 2 +
    body 21; `bx = floorf(next - BAT_W + 0.5)`, a whole pixel). While it charges, the bolt: `gfx_bolt(bx - BOLT_GAP - BOLT_W / 2, TOP_MID,
    BOLT_R, LIGHT)` with `BOLT_R` = `BOLT_W` = 6 (12 px high, 6 wide, as high as the battery) and `BOLT_GAP` = 3 (the bolt is slanted, so it
    looks a little further away; the number's own left edge likewise adds about a pixel to `BAT_GAP`). The battery stays put: `bar_left`
    becomes the bolt's left end, so only what is left of it moves. The Wi-Fi sign (14 px wide, `WIFI_R` 7, `floorf(bar_left - 12 - 14 + 0.5)`)
    while the switch is on. `bar_left` is the left end of what has been placed so far; what is not there (no battery, switch off) takes no room,
    so what is left of it moves right. The number is not given room for "100%" ahead of time any more (the cog wheel no longer
    depends on it): the group is laid out from the real width, so with a 2-digit level the battery is about 8 px further right than at
    100 %; the clock and the cog wheel never move. Tabs: `room = bar_left - TOP_GAP - MARGIN` (the last tab's frame, 9 px wider than
    its text on each side, ends 3 px before the first thing on the right); the music-note line ends 40 px before `bar_left`.
    `COG_GAP` and the old `shift` are gone.
  - *Battery* (`draw_battery`): terminal 2 x 4 on the left (`x`, `y + 4`), body 21 x 12 (a `gfx_round_rect` outline of radius 2.5 with a 1.5
    dark one inside, as before), four bars 3 x 6 at `body + 3 + (3 - i) * 4`, `y + 3`; bar 0 is the right one. `battery_bars(percent)`:
    0 for 0, otherwise `(percent + 24) / 25` limited to 1..4 (1-25 % one bar, 26-50 two, 51-75 three, 76-100 four). Red (255, 95, 85)
    at 15 % or less when not charging. Charging: the bars `lit .. lit + step - 1` are drawn at half strength, with
    `step = floor(time / 0.5) mod (5 - lit)`: the bars above the level come on one after the other every half second, then it
    starts again; with four bars nothing animates. The bolt (`gfx_bolt`, `ICON_BOLT`, the old polygon) is drawn by `draw_top_bar`, not
    by `draw_battery`, because it is outside the battery and moves the things left of it (see *Layout*).
  - *Wi-Fi*: `sceWlanGetSwitchState() > 0` in the once-a-second poll (`#include <pspwlan.h>`, `-lpspwlan` in the `Makefile`; PPSSPP
    implements it too). It is the state of the Wi-Fi switch, not "connected to a network". The maintainer reported on 27 Oct 2026 that the sign works on their PSP (which model was not said: the PSP Go is still unknown).
    **There is no Bluetooth sign for the PSP Go**: no call that reads the Bluetooth state was found in the SDK headers (`pspwlan.h` and
    `pspnet_apctl.h` are Wi-Fi only) or in the ARK-4 sources (only the file names `bluetooth_plugin.prx`, `btdun.prx`, `hidsvc.prx` and an LED id);
    a sign that is never drawn would only be dead code. If someone finds the call, the sign goes at `bar_left - TOP_GAP`, left of the Wi-Fi sign.
  - *`ICON_WIFI`* (`icons.c`: `sd_wifi`; `gfx_wifi(cx, cy, r, color)` in `gfx.c`): a box of 2 r = 14 px (r = 7, `u = r / 7`): a 3 x 3 square in
    the bottom left corner and, round its middle, the upper right quarters of three rings 2 thick (bands 3.1-5.1, 6.8-8.8 and 10.5-12.5 from
    the middle; the outer one touches the box's right and top). The arcs begin 0.5 right of and above the square's middle, which is on pixel
    lines at the usual place, so their ends are straight. The edges are soft like the other icons (`ICON_WIFI` is in the `fine` kinds of
    `icon_render`: 4 x 4 samples per pixel; the distance is a true distance). The first version was 12 px with a steep distance so that every
    pixel was lit or not at all; the maintainer found it too sharp next to the other icons, and at 12 px a soft edge makes the three arcs
    (3 px apart) blur into grey stripes, which is why the sign is 14 px with 1.7 px between the arcs. At the usual place (`cx`, `cy` whole) it
    is the same picture everywhere. When the icon cache is full `gfx_wifi` draws the square and 24 flat pieces instead (like `gfx_gear` and
    `gfx_bolt` have a fallback).
  - *Test mode*: `wifi 0/1` in the test script (`testrun_wifi()`).
  - *Tests* (`host-tests`, not in git): `ui/stubs.c` (`g_stub_wifi`, `sceWlanGetSwitchState`) and `inc/pspwlan.h`; `ui/menu_test.c`:
    `top_right_at()` measures from the draw calls and the pixels (the Wi-Fi sign is what disappears when the switch goes off; the cog
    wheel is everything right of the clock). It checks the 12 px gaps, the cog wheel on the margin, the bars of 12 levels (place, full
    strength, red only up to 15 %), the charging steps (4 levels x 12 half seconds: count, place, half strength, never red, the bolt 6 x 12 px
    on the battery's rows, 3 px from it, 12 px from the sign, bright, the same in every step), the bolt not moving the battery or the number
    (number on and off, no Wi-Fi, no battery), the number at 7 levels (5 px after the battery, 12 px before the clock, the battery moves by the width difference), no battery, no Wi-Fi,
    level unknown, the highlight of the cog wheel changing nothing left of it, the last tab ending before the sign in all 11
    languages with the number off and on, the tab row shrinking to the biggest size that leaves 12 px (six layouts; `text_width` is wrapped
    with `-Wl,--wrap=text_width` in the menu, library and pixel builds so that a test can make the two tab names too wide: the plain
    mock's widths never make the row shrink), and the music line (`music_current` / `music_state` / `music_track` in `ui/stubs.c`
    now say what a test wants) ending 40 px before the first thing on the right. `ui/pixel_test.c`: every level x number x charging
    step and the battery / Wi-Fi combinations draw nothing between two pixels. `icons/icontest.py`: the 14 x 14 picture against an exact one worked out by sampling (largest difference
    under 0.12, soft edges, the square sharp, the arcs ending on pixel lines, four separate stretches along the diagonal). `gfx/gfxtest.c`: the
    same comparison in C, half transparency, position, random signs in the stress test. `run_all.sh`: the menu test and the pixel
    test run a second time with the letter widths of the real font (`MOCK_ATLAS`, from the atlas the foldertext test makes): the plain mock's
    widths are whole pixels, so a position that is not rounded to a whole pixel (the battery's, the sign's) looked fine in it; `KEEP=1`
    leaves the work folder (`menu.out` ...) in place for looking at a failure.

- **Library: the view button and folders** (`ui.c`, `ui.h`, new `folders.c` / `folders.h`, `icons.c`, `gfx.c`, `lang.c`, `config.c`).
  - *Where a game is* (`folders.c`, `folder_raw`): nothing is stored (no change in the list cache): the folder comes from
    `Entry.path`. After the first `/PSP/GAME/` or `/ISO/` of the path (also `ef0:`), the parts are counted: for ISO the folder is the
    part before the file (needs 2 parts), for PSP/GAME the part before the game's own folder (needs 3). `PSP/GAME/Hello/EBOOT.PBP` and
    `ISO/x.iso` are in no folder; `PSP/GAME/PS1/Crash/EBOOT.PBP` is in "PS1". `folder_clean` drops `CAT_` (any case) and two digits that
    are followed by a non-digit (`01Racing`, `CAT_01Racing` -> "Racing"; "2048", "007 Games" and a name that is only the prefix stay).
    `folder_initial`: the first letter as a capital (ASCII, Latin-1, Latin Extended-A, Greek, Cyrillic; other characters as they are),
    two letters for Ch, Dz, IJ, Lj, Ll, Nj (written Ch, Dz, IJ, Lj, Ll, Nj), skipping a leading space `_ - . [ (`.
    `folders_build` makes the `FolderSet` (names sorted without regard to case, equal names are one `Folder` with `count` and
    `newest` file time, `of[i]` = folder of list entry i or -1). `Ui.folders` is built again in `build_views`.
  - *The cards*: a folder card in the Library's `LoaderView` is `LIB_FOLDER_ID(n)` = -10 - n (`ui.h`; the loader leaves every negative
    index alone, like `HOME_LIBRARY` = -3). `ui_selected()` is -1 on a folder (no background, no options). `rebuild_library()`
    makes the list for `lib_view` (`LIB_VIEW_ALL / FOLDERS / LOOSE / BOTH`, in the order of the popup and of `libview=` in `psp5.cfg`) and
    `lib_open` (the open folder, -1 = none): folders first (by name = their number, or the newest `Folder.newest` first), then the games
    of the view (`cmp_name` / `cmp_recent`, the UMD first); in BOTH the disc is moved in front of the folders. It builds in a
    temporary array and copies it into the same buffer (the loader thread reads it) and sets `count` last. `sort_library()` is gone.
    `lib_open` is kept as a number and as a name (`lib_open_name`): `build_views` finds the folder again by its name after the
    list changed and closes it (`lib_open = -1`) when its last game is gone. `relist()` also remembers the name of a selected folder
    card and selects it again. Leaving the Library (`set_tab`) closes an open folder (`leave_folder`).
  - *Buttons*: `lib_focus` is 0 = a card, 1 = Sort (`LIB_BTN_SORT`), 2 = the view button (`LIB_BTN_VIEW`). `lib_buttons()` says which
    exist: Sort with 2 or more cards, the view button unless a folder is open (and when the list has any game). ↑ from the first row
    (`lib_button_above`): the nearest button by the distance from the middle of the card to the button's frame (equal: the view
    button), none: straight to the cog wheel. ← → move between the two, ↓ / ○ to the cards (with no cards ○ closes the Library), ↑
    to the cog wheel; ↓ from the cog wheel returns to the button ↑ came from, as before. With no card in the view (`FOLDERS` with
    no folders, `LOOSE` with everything in folders) `lib_focus` is forced to the view button. ○ on a card: an open folder closes
    (`close_folder`: the folders again, on its card), else the Library closes. ✕ on a folder opens it (`activate_card`). START
    does nothing on a folder. New `view_anim` / `view_frame` like `sort_anim` / `sort_frame` (`sort_anim` now only follows
    `lib_focus == 1`).
  - *The row at the top* (`lib_header`, used by the drawing and by the ↑ choice): the view button's text is the name of the view
    (`S_LIB_ALL`, `S_LIB_IN`, `S_LIB_OUT`, `S_LIB_BOTH`) **with a ":" after it** ("All: 15"), or inside a folder the folder's name with a ":"
    ("Racing: 3"), without a frame; the number of cards is *inside* the frame now (`view_r` = the end of the number + `SORT_PAD`), a
    space (`gap`, whole pixels) after the colon; size 0.66, smaller
    (`text_fit`, down to 0.45) when it does not fit in the room left of the Sort button (12 px gap, the number of cards 8 px after the
    text); when even 0.45 is too wide next to the number, the number is dropped, then the end of the text is cut ("..."). The frame is
    `SORT_PAD` (9 px) wider than the text on each side and 22 px high, on the same line as the Sort frame
    (`popup_label_mid(53, SORT_SIZE)`); the baseline of the text is worked out from its size (`popup_label_mid`) so that the middle of the text stays
    on that line, whatever the size (`text_fit` may have made it smaller). The number of cards is drawn on that same baseline (`head_y`, size 0.6): it was at 53, the Sort button's baseline, and so one pixel above the text (`ui/library_test.c` compares the two in all 11 languages and four views). The view popup (`MENU_VIEW`, four `MI_VIEW_*`) opens at x = 15 (the frame's left edge), y = 66, as wide as the widest
    label + 32 (at least 200, at most 450).
  - *Drawing*: the folder card is a card in the theme's colour (`theme_cur()->card`, like the Library card) with `gfx_folder_icon` in a
    light grey (200 .. 255 with the focus) and the letter in the card's colour, size 0.037 x the height of the body (a capital is about half of it), on
    whole pixels, centred in the body. In the bottom row the tag is `L(S_I_FOLDER)` ("Folder", an existing string, used as it is by the Information screen) **in capitals**:
    `folder_upper()` (`folders.c`, UTF-8: ASCII, Latin-1, Latin Extended-A, Greek, Cyrillic; Japanese and anything else as it is, a
    damaged or 4-byte sequence is copied unchanged) turns it into "FOLDER" like the tags PSP / APP, in every language; the title is the
    folder's name; an empty view shows `S_LIB_NO_IN` / `S_LIB_NO_OUT` at (24, 120), size 0.9.
  - *The folder picture* (`icons.c`, `ICON_FOLDER`, `icon_folder_pic`, `gfx_folder_icon`): one image per size like `ICON_LIBRARY`: a body
    and a tab joined by the smaller of two signed distances, so there is one anti-aliased image (no double blending while it fades);
    the height is about 2/3 of the card's. The shape follows a folder icon the maintainer sent (47 x 34 px): the numbers were found by
    fitting this shape to that picture (a blur of 0.48 px on the shape, least squares over the pixels; the fit is within a quarter of a
    pixel at 23 px high): width 1.38 x the height; the tab sticks up 16 % of the height above the body and is 45 % of the width wide;
    the body's corners (the two at the bottom and the top right one) have 17 % of the height as radius, the tab's top left corner (which is
    also the folder's top left corner, so the body's own corner there is hidden) 31 % (at most half the tab) and its top right corner 23 %:
    a soft shoulder that runs down into the top edge of the body. `FolderPic` has `r`, `tab_rl`, `tab_rr` for the three radii; the tab is a
    box with two rounded top corners (`sd_rbox_top`, exact when the box is at least twice as high as its radii: it reaches down into the body
    that far). All straight edges are on whole pixels (fine sampling under 24 px). It is cheap, so it goes through the normal icon cache
    (`cache_get`), not the baked pool of the Library picture.
  - *The letter on the folder* (`gfx_folder_letter_at` in `gfx.c`, drawn by `draw_card`): in the middle of the body (the part under the
    tab): sideways text_draw's ALIGN_CENTER (the advance of the letter centred), up and down half a capital (13.7 x the size / 2) above the
    baseline, both on whole pixels, size 0.037 x the height of the body. The height of a capital, 13.7 px at size 1.0, was measured on the
    README screenshots (PPSSPP, the real PSP font): the A of the Library title at size 0.66 is 9 px high, the clock's digits at 0.8 are
    11 px. (Before, the baseline was 7 x the size below the middle, and the mock drew the single letter wrongly, to the right of its place:
    the mock spreads the letters over 6 px a byte, so the letter looked off centre in the readable test frames.)
  - *Strings* (`lang.c`): `S_LIB_ALL` says "All" (it was "All games and apps"), `S_LIB_IN` "In folders", `S_LIB_OUT` "Outside of folders",
    `S_LIB_BOTH` "Both in and out of folders" (no colon in the string: `lib_header` adds it), new `S_LIB_NO_IN`, `S_LIB_NO_OUT`; `S_SORT` says
    "Sort by" ("Sorteren op", "Trier par" ... in Czech and Japanese unchanged: "Seřadit", 並べ替え); it is also the word of the Sort button in
    Saved Data (`saves.c`). New `S_UNIT_H` / `S_UNIT_MIN` for the time played. `S_LAUNCH` (the button under the selected card in Games, the only place that uses it) says "Play" (was "Start"; the id keeps its name); the Library card's button is still `S_F_OPEN`. The translations (11 languages) are mine and have not
    been checked by native speakers. 169 strings now. `LibHeader.sort` is 96 bytes: Russian "Сортировать по: Недавно добавленные" is 66
    bytes, and with 64 it was cut in the middle of a letter (found by `library_test`: the Sort frame has to be as wide as the whole text).
  - *Kept between starts* (`config.c`): `libsort=0|1` and `libview=0..3` in `psp5.cfg` (`g_lib_sort`, `g_lib_view`); `ui_init` reads
    them, `set_sort` / `set_view` write them with `config_save()` (the buffer is 192 bytes now). A value that does not exist is the
    first one.
  - *Tests* (`host-tests`, not under git): `units/folders_test.c` (paths, names, letters, merging), `ui/library_test.c` (new: all views and
    sorts, the button and popup, opening and closing, the empty views, a deleted game, `psp5.cfg`, drawn letters and the row in
    11 languages), `ui/folder_fixture.h`, `icons/foldertest.py` (the shape and the fractions of the fit), `gfx/foldertext_test.c` (the ink of the capital letters
    A-Z and 0-9 in the folder body at the seven Library card sizes: within a pixel of the middle sideways and up and down, on average within a
    third of a pixel; with `MOCK_REAL=1` the mock measures text by the advances of its letters and draws it at the real size, so the test needs
    the atlas from `mkatlas.py`; the PC font's letters are shaped like the PSP's but are not the same); `ui/menu_test.c` and `ui/pixel_test.c` changed for the view button
    (↑ from the first card is the view button; one card has the view button but no Sort button) and draw every view, every folder
    card, the popup and an open folder through the whole-pixel check (`menu_test.c`'s check "↑ from the first row selects the nearest
    button" now takes the two frames from what is drawn, because the mock makes the Russian Sort button as wide as the row). Measured on a PC only. The mock measures Cyrillic and Japanese
    as 2 and 3 bytes of 6 px, wider than the real font, so how much of the label is cut in Russian and Japanese on a real PSP is not known.
- **Time played** (`playtime.c` / `playtime.h` new, `main.c`, `ui.c`, `ui.h`, `lang.c`, `Makefile`).
  - *What is known*: psp5 is not running while a game is, and the PSP's clocks cannot tell sleep and power-off from play time, so psp5 itself does not measure anything:
    the time comes only from the helper plugin below. (Test builds 1-6 measured start to next start with a marker file holding two clocks, and had a switch
    "Exact time played" for the plugin; all of that is removed. Why the clocks were no use: starting a game (`sctrlKernelLoadExecVSHWithApitype`) restarts the PSP's software and the system clock
    (`sceKernelGetSystemTimeWide`) with it - measured once on a real PSP: 7 s at the start of the game, 3 s when psp5 came back - and the real-time clock goes on through sleep and power-off.)
  - *The marker that is left*: `<DATA_DIR>/ingame`, an empty file. `set_ingame_mark()` (main.c) writes it before a game is started when psp5 is ARK's launcher (`sys_is_launcher()`), `take_ingame_mark()` removes it
    at the start and tells `app_start` to skip the start-up screen. It holds nothing and counts nothing. `sys_is_launcher()` is true when psp5 was started as ARK's `VBOOT.PBP`, also from the XMB with the Custom Launcher entry.
  - *The helper plugin* (installs itself, no switch): a kernel module that ARK loads into every game; it counts only awake time, and it finds out which game it is in by itself, so a game started from the XMB counts too.
    Files: `plugin/psp5time.c` (the module: `module_start` makes one thread, priority 0x10, stack 0x4000; the thread ticks once a second),
    `src/awake.c/.h` (shared with psp5, no system calls, **no 64-bit division** - the plugin is linked without libgcc), `plugin/Makefile` (a kernel PRX: `USE_KERNEL_LIBS`, `-lpspsystemctrl_kernel`,
    `PSP_MODULE_INFO("psp5time", 0x1007, 1, 0)`, exports only `module_start` and `module_info`), `src/helper.c/.h` (`helper_setup`), `plugins.c` (`plugins_helper_any`,
    `plugins_helper_add`), `playtime.c` (`merge_played`). There is **no fallback**: a game the plugin was not loaded for (no ARK, lines off) or cannot name the way psp5 does gets no time.
    - *Counting* (`AwakeClock`, `awake_tick`): the thread takes `sceKernelGetSystemTimeWide()` every second and adds the step since the last tick only when it is at most
      `AWAKE_MAX_STEP_US` (5 s). While the PSP sleeps the thread does not run; when it wakes the step is long (the clock went on) and is dropped, or the clock stood still
      and the step is normal: either way sleep adds nothing. A clock that went back adds nothing. A step of 5 s or more for another reason (the thread starved) is lost too.
      The sleeping second itself is lost as well (at most a second per sleep).
    - *Which game* (`awake_key_from_path`, `awake_key_from_sfo`): the names are the keys psp5 uses in playtime.txt (`entry_cover_key()` in scan.c; the tests compare the two on many names). From
      `sceKernelInitFileName()`: a file called EBOOT.PBP = a homebrew, PSN or PS1 game, named by the folder between the last two slashes (cut at 79 bytes, as psp5's 80-byte key); another `.PBP`
      (ARK's menu, a launcher) is not a game; everything else (`disc0:/PSP_GAME/SYSDIR/EBOOT.BIN`, an ISO path, a `PBOOT.PBP`) is a PSP game, named by the `DISC_ID` of `disc0:/PSP_GAME/PARAM.SFO`
      (at most 15 bytes, as `Entry.game_id`; an id with a path character is refused). The disc may not be readable the moment the plugins are loaded: the plugin tries once a second for 10 s (counting
      goes on meanwhile) and then gives up. **A PSP game with no disc id** (the SFO has no `DISC_ID`, or there is no PARAM.SFO; psp5 then lists the ISO with an empty `game_id` and `entry_cover_key()` names it by its file) is named the same way after the ten tries:
      `awake_key_from_files(init, umd, key, size)` takes the name of an ISO-type file (`.iso .cso .zso .dax .jso`, any case) between the last slash and the last dot, cut at 79 bytes, from the file the PSP started (`init`, when ARK starts the ISO
      by its path) or else from `umd`, the ISO that ARK has mounted. A `.PBP` as `init` (PBOOT.PBP, an installed game) never takes the name from ARK (ARK may remember an old ISO). `umd` comes from `umd_file()` in the plugin:
      `sctrlHENFindFunction("SystemControl", "SystemCtrlForKernel", nid)` for the two numbers that ARK exports `sctrlSEGetUmdFile` under (0xBA21998E, and 0xAC56B90B from PRO), called only when `init` is not itself the ISO.
      It is a run-time lookup and not an import on purpose: a stub that cannot be resolved could keep the whole module from loading, a lookup that finds nothing only means "no name". In ARK-4's source `sctrlSEGetUmdFile()` returns
      `rebootex_config.iso_path`, the path that psp5 (`sctrlSESetUmdFile` in `launch.c`) or ARK's XMB set before the game was started; ARK's own `startPlugins` compares plugin runlevels with it, so it is valid at the time plugins are loaded.
      No ISO file, no ARK function, an empty path, a path with no slash or an extension that is not an ISO type: no name, the plugin ends and writes nothing.
      **Not verified on a PSP**: what `sceKernelInitFileName()` is for ISO, PBOOT, PSN and PS1 games under ARK (the code takes anything that is not `EBOOT.PBP` as a disc game, so the disc path works
      whether it is `disc0:`, the ISO path or `PBOOT.PBP`).
    - *Where the files go*: ARK starts a plugin with `sceKernelStartModule(uid, strlen(path)+1, path, ...)` (`startPlugins` in plugin.c), so `module_start`'s `argp` is the path the module was loaded from, e.g. `ms0:/PSP5/psp5time.prx`, and
      its folder (`awake_dir_of`) is psp5's data folder (psp5 wrote the module there): no guessing between `ms0:` and `ef0:`. The seconds of a game are in `<data dir>/played/<key>.txt` (`awake.h`: `psp5-played 1`, `secs` + 10 digits, `end`).
      The first write is a new file (10 s after what the file had), later ones are in place, without truncation, always the same length, in one `sceIoWrite`, so a power loss leaves the last whole write or nothing.
      An existing whole file (seconds psp5 has not taken yet: a game played twice before psp5 started, a reset) is continued; one that is not whole (cut off, another format) is replaced.
    - *psp5 itself*: ARK's runlevel `game` also matches homebrew (`matchingRunlevel` in `core/systemctrl/src/plugin.c` looks for the word `game`, `homebrew` or `app` in the homebrew
      case, `umd`/`psp`/`game` for ISO and UMD games, `pops`/`ps1`/`psx` for PS1), so the plugin is loaded into psp5 too. `awake_is_psp5(sceKernelInitFileName())` makes it end at once there
      (`VBOOT.PBP`, or `EBOOT.PBP` in a folder called `PSP5`): psp5 is not a game, and as the launcher it is started again right after every game.
      The plugin cannot know a psp5 that was put in a folder with another name (it would count it as a game), so psp5 also knows itself: `main()` calls `playtime_set_self(argv[0])`, and `merge_played` removes (and does not add) a file whose name is the folder
      of psp5's own EBOOT.PBP (compared without regard to capitals, FAT does not tell them apart). The plugin does write that file while psp5 runs; it is removed at the next start.
    - *psp5's side* (`merge_played` in `playtime.c`, called by `playtime_load`, which `main()` calls at the start): every `*.txt` in `<data dir>/played` that is whole (`awake_parse`) is added to its game
      (`add`: no overflow), `playtime.txt` is saved once, and only when that worked the files are removed (none are lost when the card is full or read-only; a `playtime.txt` that cannot be read, because it is too big, is not written over
      and the files stay). Files that are not whole are left alone. At most 256 files per start. A game under 10 s awake writes no file (the first write is at 10 s), so it gets nothing.
      A switched-off PSP leaves the last write (at most 10 s behind) and the next start of psp5 uses it.
      Every homebrew you run gets a file and a line in playtime.txt (under its folder name), also ones psp5 never shows.
    - *Installing* (`helper_setup`, first thing in `main()` after `sys_init`; only with ARK, `sys_has_ark()`): `install()` writes `<data dir>/psp5time.prx` from the C array `psp5time_prx[]` that the Makefile makes with `bin2c`, unless the file is already
      byte for byte the same (an older, longer, shorter or damaged one is replaced); if that failed nothing else is done (no lines for a file that is not there; it is tried again at the next start). Then `plugins_load(sys_ark_dir())` and, when
      `plugins_helper_any("psp5time.prx")` says there is no line for it at all (neither `game` nor `pops`, any folder, any case), `plugins_helper_add` appends `game, <path>, on` and `pops, <path>, on` to the first PLUGINS.TXT that exists
      (the ARK folder's if both do) or creates the ARK folder's; a file that did not fit (more than 96 lines, a line over 191 bytes) is never written, and then nothing is added (not half of the two either).
      **Lines that are there are never changed**: the opt-out is turning them off in Settings > Plugins (they are listed as `psp5time.prx`, Games and PS1), and they stay off. One line left of the two counts as "there" (nothing is added). The user who deletes both lines gets
      them back at the next start. ARK reads PLUGINS.TXT at every game start, so no restart is needed. A test build user who had the switch on keeps working (the lines are there); one who had it off has `off` lines and keeps them off.
    - *Build*: the main Makefile makes `plugin/psp5time.prx` (sub-make), then `plugin/psp5time_prx.h` (`bin2c`), which `src/helper.c` includes; `make clean` cleans the plugin too.
    - *Reported working* (the maintainer, 27 Oct 2026, one PSP, model and ARK version not said): the self-installing plugin and the naming of ISO games without a disc ID. *Not known* (so still open for the other
      models, ARK-4 and the Go): that ARK accepts and runs the module as written; the thread's behaviour across a real sleep and wake-up (the code works for a clock that jumps and for one that stands still);
      the file names `sceKernelInitFileName()` gives (above); that ARK-5 (only ARK-4's source was read) exports `sctrlSEGetUmdFile` under one of the two numbers (without it a game with no disc id gets no time); file calls from the thread on a PSP Go with a game on the internal storage; the PSP Go's pause mode and the HOME menu (user threads are paused, the kernel thread may go on counting).
      The old test build 4 kept `PSP5/ingame.awake` (a different design, matched by the marker's number); that file is no longer read, and is not removed.
  - *Under a minute*: `playtime_text` gives `<1min` for 1..59 s (it was empty), so a short test shows something; sessions under 10 s are still not
    counted.
  - *The file* `<DATA_DIR>/playtime.txt`: a line per game `"<seconds> <key>"`, up to 4096 games (the first wins when a name is there twice; a full
    table takes no new game but keeps the others; CRLF is fine; a bad line is skipped). It is written when a session is added
    (whole file, `sceIoOpen` + write). Sums stop at `0xFFFFFF00` (no overflow).
  - *The text* `playtime_text(secs, out, size)`: 0 s nothing, under 60 s "<1min"; under an hour "%umin"; else the hours, rounded down, at most 9999,
    "%uh", the units `S_UNIT_MIN` / `S_UNIT_H` (h, min; Dutch u; Russian ч, мин; Japanese 時間, 分). Never both, as the maintainer asked.
  - *The pill* (`ui.c`, `draw_play_pill`; `PILL_H` 26, `PILL_PAD` 12, `CLOCK_D` 12, `CLOCK_GAP` 6, text size 0.66, baseline `by + 17.5` like
    *Start*): drawn with `button_bg` (the dark pill of the bottom-row buttons, never selected), a white face (`gfx_circle`, 12 px) with two
    dark hands (1 x 5 up from the middle and 4 x 1 to the left, `gfx_rect`, whole pixels) and the text, its right edge at `SCREEN_W - MARGIN`
    (456); in the Games tab (`draw_details`) opposite *Play* / Options (the pill is clear of the Options circle by at least 10 px in all
    11 languages), in the Library (bottom row) opposite the tag and the title, whose room `max_w` is 12 px less than the pill (the cache of
    the shortened title, `lib_label`, also remembers its room: `lib_label_room`). Nothing is drawn for a text that is empty (never played), a folder card, or the Library card (`play_text()` returns "" for every negative card number, so no lookup is made for
    the launcher's own cards, pinned or not; `library_test` puts lines for "Library", "?library", "PSP5" in playtime.txt to prove it). `play_text()` looks the seconds up once per selected game (`pt_for`, `pt_secs`; reset
    next to every `lib_label_for = -2`) and makes the text at every draw, so a language changed in Settings is followed.
  - *Limits*: no ARK, no time (the plugin is an ARK plugin: the PRX is loaded through PLUGINS.TXT); the plugin's lines off, no time; a game under 10 s awake, no time; a PSP game with no disc id whose ISO file name is not known (ARK does not give it), no time (above).
    The plugin cannot know a psp5 that was put in a folder with another name, so psp5 drops that file itself (above). A PSP that is switched off in a game loses at most the last 10 s.
  - *Tests* (`host-tests`, not under git): `units/playtime_test.c` (96 checks: no file, the format of playtime.txt - CRLF, names with spaces, damaged lines, the same name twice, 4096 games, numbers that cannot overflow -, the games told apart by `entry_cover_key()`,
    the files of the plugin merged and removed - only whole files, kept when `playtime.txt` cannot be written or read, 256 at a time, psp5's own folder dropped -, the names of the plugin against `entry_cover_key` / `entry_read_sfo`, also for ISO files without a disc id,
    the text of a time in 11 languages), `units/awake_test.c` (230 checks: the counter with sleeps, a clock that stands still or goes back, the file's format cut at every length, the names of games from paths, from a PARAM.SFO with
    wrapping offsets and cut pieces and from ISO files, the folder and file names, `awake_is_psp5`),
    `plugin/psp5time_test.c` (90 checks; includes the real `psp5time.c` and runs its loop against a fake clock with sleeps, a throwaway memory stick with a made-up disc and a stand-in for ARK's function: a game from the XMB, a disc that is readable late or never,
    the writes in place, under 10 s, sleep for hours, switched off, a game played twice, `ef0:`, a folder that is not there, a 6-hour game, psp5 itself skipped, the no-fallback cases, and a game with no disc id named by the ISO file from the started file or from ARK - by the first
    number, by the second, not at all -, no use of ARK's old ISO for PBOOT.PBP, the disc id winning, over-long paths),
    `helper/helper_test.c` (33 checks: no ARK = nothing written; the first start writes the file and the two lines; later starts write nothing; off stays off; one line is enough; lines from another folder or in capitals count as there; deleted lines come back; PLUGINS.TXT made when missing;
    SEPLUGINS; an older, longer, shorter or damaged file replaced; a file that cannot be written, or only half of it, leaves PLUGINS.TXT alone; a PLUGINS.TXT that does not fit is left as it is), the `plugins_helper_*` part of `cfg/cfg_test.c` (79 checks in all) and the plugin part of
    `settings/settings_test.c` (no "Exact time played" anywhere, the two lines show as Games and PS1, turning one off changes only that line). `ui/library_test.c` (the clock in the Library and the Games tab: exact place of
    the text and of the two hands, nothing for unplayed games, folders or the Library card, the title cut before the clock, the room in 11
    languages) and `ui/pixel_test.c` (every game has a time: the clock is drawn through the whole-pixel check in every state). `host-tests/plugin/inc/` holds two stand-in headers; the plugin and psp5 code is used as it is
    (the plugin has one hook for the tests, `FIND_UMD_GETTER`, because a function address does not fit its 32-bit lookup result on a 64-bit PC).
- **.jso files are read** (`lzo.c`, `lzo.h` new; `scan.c`, `info.c`, `gameops.c`, `files.c`, `iso.c`): a JSO is a CISO-like container
  ("JISO", block size at 6, method at 0x0A: 0 = LZO, 1 = zlib, unpacked size at 0x0C); libcisoread already reads the container and
  calls `lzo1x_decompress(src, src_len, dst, &dst_len, wrk)`, which `iso.c` only had as an empty stub returning -1. `lzo.c` is a decoder
  of the LZO1X format that checks every read and write against the end of the buffers (a damaged block returns an error, never
  writes outside `dst`; `dst_len` is the number of bytes written). `scan.c` lists `.jso`, `info.c` recognises `JISO` (label "JSO", size
  = 32 bits at 12), `gameops.c` may delete it, `files.c` colours it like the other images. Tests: `units/lzo_test.c` with
  `units/lzo_vectors.py` (196 pairs packed by liblzo2: both packers, runs, long matches, random data; each cut off at many lengths
  and damaged at random, in exactly sized buffers under AddressSanitizer; 100 % of the lines of `lzo.c`), and the scanner, the
  Information and delete tests with a made-up `.jso`. No real `.jso` file and no PSP was used.

- **PS1 theme: the symbols moved down** (`theme.c`, `theme_backdrop`): `deco_ps1(time, a, 418, 30, 0.6f)` became
  `deco_ps1(time, a, 418, 130, 0.6f)`; only the centre's y changed (x 418 and the size 0.6 are as before). At y = 30 the group
  (a diamond of four symbols, `d` = 14.4 px from the centre, `r` = 6.3 px, a glow of 8 px, a bob of +-2 px) covered about y 8..52
  and sat right behind the clock (`text_draw` at y 25, x 416..456) and the battery (`bx` about 384, y 15..), so the symbols were
  half hidden. At y = 130 it covers about y 101..160 and x 395..441: below Home's row of cards (`ROW_Y` 38 + `BIG_H` 62 = 100,
  titles end at about 99) and above the bottom row (`BOTTOM_Y` 243, Start and Options are at the left). Still one place on every
  screen, because `theme_backdrop` draws it for all of them. Looked at in PC test frames of Home and of the Library (a copy of
  `ui/menu_test.c` with `g_theme` = 12; not on a PSP): on Home it is clear of everything; in the Library it is fainter than on
  Home because of the Library's own darkening (`DIM_COLOR`, as before the move), and the grid cards of the second row (from the 10th
  game on, worked out from `GRID_Y` / `ROW_STEP` / `CELL_W`, not drawn) sit over most of it. The Settings, file manager,
  Information and Manage Saved Data lists were not looked at one by one: their rows are drawn over the symbols.
  `host-tests/gfx/theme_test.c` (not under git) now checks that the symbols change pixels at x >= 380, y 100..165 (more than 300),
  none at x < 300, none in the top 100 rows and none in the bottom 42.

## Version 1.1.0 (psp5-authentic)

Everything that changed after 1.0.0 – see `CHANGELOG.md` for the version written for users.

- **PSP Go with ARK-4: the games of the Memory Stick Micro show up when psp5 runs from the internal storage** (`Makefile`):
  `SFOFLAGS = -d EFAWARE=1` (after `PSP_EBOOT_TITLE`) adds the integer entry `EFAWARE` = 1 to the EBOOT's PARAM.SFO (the PSPSDK's
  `mksfoex -d`; checked by reading the SFO of the built EBOOT: `EFAWARE: 1`, the file is 28 bytes bigger). ARK-4 then starts psp5
  from the internal storage with the Memory Stick as `ms0:` (psp5 itself on `ef0:`), so `scan.c` finds both: `ef0:` always, `ms0:`
  when it is a real stick, as since 1.0.0. There is no change in the code. The setting is in every build (ARK-5, ARK-4, PSP Go and
  the other models). It was found with a test build
  (`psp5-authentic-TEST-PSPGo-ARK4-efaware-v2.zip`, ARK-4, otherwise the same as the build of that day): a PSP Go owner with
  ARK-4 reported that it works. A recent ARK-4 is needed. The CI build (`.github/workflows/build.yml`) gets the flag too, since it
  uses the same Makefile.
- **Version 1.1.0** (`settings.c`, `tools/make_release.py`, `INSTALL.md`, `README.md`): the Version row shows `1.1.0 (ARK-4)` /
  `1.1.0 (ARK-5)`, `make_release.py` defaults to 1.1.0 and the zips are called `psp5-authentic-v1.1.0-…`. The README says
  1.1.0 and keeps its "What has been tested" section general (no single fixes named); `INSTALL.md` says 1.1.0 and that a recent
  ARK-4 is needed on the PSP Go.

The entries below were written between 1.0.0 and 1.1.0.

- **Settings → psp5 → Show battery percentage** (`config.c`, `settings.c`, `ui.c`, `lang.c`): new own setting `g_battery_pct`
  (`batterypct=0|1` in `psp5.cfg`, default 0; `config_load` only changes it when the line is there, so a file from 1.0.0 keeps it
  off). `settings.c` adds the toggle after "Startup animation" (`own = 1`, so it is saved by `settings_commit`); label
  `S_BATTERY_PCT` in all 11 languages (the string table has 164 rows now). `draw_top_bar` (`ui.c`): with the setting on and a
  battery present, `shift = ceilf(text_width(0.8, "100%")) + BAT_GAP` (`BAT_GAP` = 5 px) is always reserved, whatever the level, so
  `bx = floorf(SCREEN_W - MARGIN - clock_w - TOP_GAP - BAT_W - shift + 0.5)` (the battery; `TOP_GAP` 10, `BAT_W` 22) and everything
  that follows it (the cog wheel and its frame, the tab `room`, the music-note line) stay put while the level changes. The level (`"%d%%"`, capped at 100, from
  `scePowerGetBatteryLifePercent()` through the once-a-second poll that already fed the icon, or the test-run override) is drawn
  at size 0.8, `ALIGN_LEFT` at `bx + BAT_W + BAT_GAP`, i.e. always 5 px after the end of the battery; with fewer digits its right
  end is further from the clock. It is only drawn when the level is known (`level >= 0`); the room stays reserved then.
  The tabs shrink with the same rule as before (`room = bx - COG_GAP - 21 - MARGIN`, smallest size 0.56).
  *(The layout in this entry - room for "100%", `COG_GAP`, the bolt - was replaced by the new top bar at the top of the section "Version 1.2.0".)*
- **Top bar without a battery** (`ui.c`): the poll keeps `sys_present` (`scePowerIsBatteryExist()`; the test-run battery counts as
  present). Without a battery `draw_battery` is not called at all (before it drew the empty outline) and no number is drawn; `bx`
  is the position without the number (`shift = 0`), so the cog wheel does not move.
  Tests (`host-tests`, not in git): `units/config_test.c` (new; file round trip, old file, defaults, runner entry `pcfg`),
  `ui/menu_test.c` (levels 100/80/55/10/5/0/150: battery and cog wheel at the same place, number 5 px after the battery, only the
  space to the clock grows; no battery with the switch on and off: no bright pixel in the battery's place, cog wheel where it was;
  level unknown; the tabs for three levels in all 11 languages), `settings/settings_test.c` (the toggle sits under Startup
  animation, toggles with ✕, is saved once), `ui/stubs.c`, `ui/popup_sig.h` (records the text of a drawing call). The test font is
  as wide at every size, so a language whose tabs are at the smallest size in the test (Japanese with "100%") is not measured there.

- **README rewritten, release notes shortened, screenshots added** (`README.md`, `CHANGELOG.md`, `docs/screenshots/`; docs only, no code):
  - README: ~190 -> ~140 lines. Order: intro (fork, goal, thanks), screenshot table, "What you get" (one short bullet per feature, no
    button lists inside), "Buttons" (short table; the full one stays in `INSTALL.md`), Install (+ safe start / hold SELECT), "What has
    been tested", "About this fork", Building, Thanks, License. The covers section is one sentence that points to `INSTALL.md`.
    Nothing about "PS5" (the theme list in the app shows that name, so no screenshot of the Theme page is used).
  - The screenshot table is HTML (`<table>`, two columns, a caption under each picture) because a Markdown table can't have
    captions under the images without a duplicated header row. Eight PNGs in `docs/screenshots/` at the time (`games`, `options`, `library`,
    `saved-data`, `music`, `settings`, `theme`, `startup`); 1.2.0 replaced them with sixteen, see *New screenshots for 1.2.0* at the top.
  - CHANGELOG: same facts, shorter bullets. *Unreleased* is grouped into Library / Top bar / Docs, 1.0.0 keeps its groups.
    Some 1.0.0 details that only matter to developers were dropped from there (they are in these notes): the popup look "lives
    in `popup.c`", the faint light fill of the popup panel, the 60 % beam is kept.
  - How the screenshots are made: the real app runs in **PPSSPPHeadless** (built from source, `--graphics=software`) with a
    scratch copy of the app built with `-DPSP5_TEST`. In that copy only, `testrun_frame` writes the finished frame (the 512 x 272
    x 4 byte buffer it is given, at `0x44000000 + s_draw`) to `ms0:/PSP5/shots/NAME.raw` for the `shot NAME` step and goes on at
    once (the repo's version waits for the Windows script to photograph the window), and the app calls `sceKernelExitGame()` when the
    script is over. `raw2png.py` crops the frames to 480 x 272; the README images are those enlarged 2 x with nearest-neighbour.
    PPSSPP's own flash0 fonts are used (copied to `PSP/NAND/flash0`), so the text is PPSSPP's replacement font, not the PSP's.
    `faketime` fixes the clock at 10:09; `battery 82` and `charging 1` in the script show the battery with its percentage and the
    bolt (`batterypct=1` in `psp5.cfg`). The demo data (16 invented games with drawn art, nine saves, five MP3 files with album
    art) comes from `make_shot_data.py`, which uses the helpers of `tools/make_testdata.py`.
  - The kit (generator, run script, the eight button scripts, the patch, a README with the PPSSPP build flags) is in
    `screenshot-kit/` next to `host-tests/` in the hand-off bundle, **not in git**. Run it again whenever the look changes.

- **Top bar: the number closer to the battery, the cog wheel further from it** (`ui.c`): *(replaced by the new top bar at the top of "Version 1.2.0")* `TOP_GAP` (clock to what is left of it, 10),
  `BAT_W` (battery body 20 + terminal 2), `BAT_GAP` (battery to its number, was 10, now 5) and `COG_GAP` (battery's left edge to the
  cog wheel's centre, was 22, now 27) replace the literals 32 / 22 / 10 in `draw_top_bar`. `gx = bx - COG_GAP`, tab
  `room = bx - COG_GAP - 21 - MARGIN`. The group (battery + number) sits 5 px closer to the clock, so with the percentage on the cog
  wheel ends where it was (`bx` +5, `COG_GAP` +5); with it off `bx` is unchanged and the cog wheel is 5 px further left. Reason for
  `COG_GAP`: `draw_battery` draws the charging bolt at `gfx_bolt(x - 6, y + 5, 5)` (about x - 8.5 .. x - 3.5), and the frame of the cog
  wheel (`popup_choice(gx - 15, ..., 30, ...)`, right edge at `bx - 7` before) overlapped it; now the frame ends at `bx - 12`.
  Tests (`host-tests/ui/menu_test.c`): the geometry checks use 27 / 5 / 48 (tab text ends 48 px before the battery); new
  charging check (`g_stub_battery_charging` in `ui/stubs.c`): the pixels of the bolt are the same with the cog wheel selected and
  not, number on and off (it fails with `COG_GAP` 22).

- **The Library card does not calculate its picture while it grows** (`icons.c/.h`, `libicon_data.inc`, `tools/gen_library_icons.py`,
  `gfx.c`): while the Library card went from its small size to the big card, every frame had a new card height (33 .. 62) and so a
  new picture height (22 .. 41), and `ICON_LIBRARY` was rendered by the CPU for each one (`icon_render`: 4 x 4 supersampled below 32 px,
  2 - 4 ms on a PC, 25 - 40 times more on a PSP) - the stutter just before the card enlarged. Now
  - `src/libicon_data.inc` (generated, do not edit): the picture for the heights 21 .. 42 (`LIB_BAKED_FIRST/LAST`), run-length coded as
    (coverage, run) pairs, 9972 bytes in `lib_baked` with `lib_baked_start[]`. `tools/gen_library_icons.py` makes it: it compiles
    `icons.c` with `-DLIB_NO_BAKED` (an empty table), renders the heights through `icon_render` with ctypes and writes the file; run it
    again whenever the picture or the card's sizes change (`librarytest.py` checks that the file is up to date).
  - `icon_library_image(h, out)` decodes the table (and calculates a height that is not in it), `icon_library_baked(h)` says if
    it is there.
  - `gfx.c`: the Library picture no longer goes through the evicting icon cache (which freed images that were not used in the last
    frame): `s_library[LIBRARY_MAX_H + 1]` is a pool with one texture per picture height, kept until `gfx_term` (`library_free`), so
    each height is made once, from the table, and never calculated while sliding. A height that is not in the table is still
    calculated the first time it is needed.
  - Measured on a PC only (not on a PSP): no `ICON_LIBRARY` render in the frames of the slide.
  - Tests (`host-tests`, not in git): `icons/librarytest.py` (baked == calculated for h 1 .. 96), `gfx/gfxtest.c` (every card height
    32 .. 63 is baked; a height that is not baked still draws), `ui/menu_test.c` (the sliding card test checks that the picture is
    baked at each frame).

- **Library: ↑ goes to the Sort button first from every card of the first row** (`ui.c`): `sort_above()` (was: the card is
  under the Sort button) is removed. In the grid, `move == -2 && *sel < GRID_COLS` selects the Sort button (`lib_focus = 1`) when
  the Library has more than one entry, otherwise (nothing to sort) goes to the cog wheel as before (`enter_top_bar(ui, TOP_COG)`).
  ↑ from the Sort button and ↓ from the cog wheel are unchanged. README and INSTALL updated.
  Tests: `ui/menu_test.c` (first card, last first-row card, per-language loop) and `ui/pixel_test.c` (first card -> Sort button).

- **Manage Saved Data: the Sort button and its popup line up with the rows** (`saves.c`): the frame of the Sort button starts at
  `LIST_X`, the same left edge as a row's highlight, and the text 9 px in, at the new `SORT_X` (`LIST_X + 9`), one pixel right
  of the rows' circles, check marks and padlocks (`ROW_ICON_X`, `LIST_X + 8`, new, `draw_rows` uses it). Before: text at
  `title_x + 9` (x 37) and frame at `LIST_X + 4` (x 28); now text x 33, frame x 24. `header_layout` sets `sort_x = SORT_X`,
  `draw_header` draws the frame at `sort_x - 9`, so it keeps 9 px round the text and the same width. `draw_sort_menu` draws the
  popup at `LIST_X` (before `LIST_X + 4`). Not moved: the title. Tested in `host-tests/saves/saves_test.c` (in all 11 languages:
  the text is at `SORT_X`, the frame at `LIST_X` with 9 px round the text; the popup starts at `LIST_X`).

- **The Library picture is an icon of the icon framework; the word "Library" is not selectable** (`icons.c/.h`, `gfx.c/.h`, `ui.c/.h`).
  The first version of the picture was drawn in `ui.c` from nine `gfx_round_rect` calls in the card's float box, with the right
  shoulder at x 33..56 and the left at 3..26 (so the controller was cut off on the right) and with every edge rounded from the
  card's fractional position, so it changed shape while the cards slid. Now:
  - `ICON_LIBRARY` in `icons.c`: `library_geo(h, &g)` rounds every edge of the 83 x 77 design to whole pixels (`PXL(v) =
    floorf(v * h / 77 + 0.5)`; `h` is the picture's height in pixels, the width follows: `icon_library_width(h)`). The controller
    is built from its left half: body (centred, so its own mirror image), left shoulder `{3, 0, 23, 16, 8}` and left grip
    `{0, 14, 20, 27, 10}` - the right ones are mirrored (`x = ox + cw - right_edge`) and the arch is what is left between the
    grips (`gap_r - gap_l`), so the controller is the same on both sides at every size. `sd_library` is one signed distance:
    (squares minus the controller dilated by the rim) united with the controller minus the arch, so the rim cut and the arch need
    no colour of the card and the picture can be translucent without seams. Below 32 px it is sampled 4 x 4 (`fine`).
  - `gfx_library_icon(l, t, w, h, color)` in `gfx.c`: picture height `floorf(h * 0.66 + 0.5)`, centred in the card's whole-pixel box
    (`(w - pw + 1) / 2`), one white alpha image per height (from `icon_library_image`; its own pool, see "The Library card does
    not calculate its picture while it grows"), drawn with `icon_blit` (`GU_NEAREST`, pixel for pixel) like the note and the
    speaker; if there is no image it falls back to plain rounded squares and a body. Only the card's size changes the picture, never its position, and the image for a size is always the same.
  - `draw_card` had a local `float b` (the placeholder's breathing) that shadowed the new `int b` of the card box; renamed `breath`.
  - `update_top_bar`: inside the Library the word is not selectable (see "Tabs" below).
  - Tests (`host-tests`, not in git): `icons/librarytest.py` (new; every height 12..96: whole-pixel edges, three equal squares at
    an even pitch, the second row the first one moved down, the controller's rows mirror exactly, nothing cut off at the right),
    `gfx/gfxtest.c` (`test_library_icon`: 320 placements of the picture in cards of eight sizes give the same image, centred, and
    exactly what `icon_render` makes), `ui/menu_test.c` (a card that slides and shrinks over 150 frames: the picture is the
    one of the card's height, in the middle, the controller mirror-symmetric; the top bar of the Library: ↑ goes to the cog
    wheel, ← → do nothing, ↓ comes back to the Sort button; the word never has a frame, in all 11 languages),
    `ui/pixel_test.c`, `run_all.sh`.

- **The Library is a card of Home, not a tab** (`home.c/.h`, `ui.c/.h`, `loader.c/.h`, `lang.c/.h`):
  - *Model.* `views[TAB_GAMES].idx` may now hold the sentinel `HOME_LIBRARY` (-3; -1 and -2 are the "none" / "not known yet" values
    `label_for`, `lib_label_for` and `cover_for` use) besides list indices. `home_build` puts the disc first, then the pinned cards
    in pin order (the Library card among them when it is pinned, key `?library` in `home.txt`; `?` cannot be in a file name),
    then the recents, and the Library card last when it is not pinned. `HOME_MAX` stays 10 and counts the Library card
    (`room = HOME_MAX - (library pinned ? 0 : 1)` while the others are placed); `HOME_PIN_MAX` is 8 (games only, `game_pins()`).
    `PIN_KEEP` (9) is how many game pins `load_file` still accepts, so a `home.txt` of 1.0.0 with nine pins keeps them all;
    `home_pin` refuses a new pin at 8 (toast `S_HOME_PIN_LIMIT` with `HOME_PIN_MAX`). `prune` never drops `?library`, `home_state`
    skips the sentinel, and when no keys can be made `home_build` still returns the Library card. New: `home_library_pinned`,
    `home_pin_library`, `home_unpin_library`.
  - *Tabs.* `enum { TAB_GAMES, TAB_MUSIC, TAB_LIBRARY, TAB_COUNT }`: the Library is not in the bar any more, it is the state
    `ui->tab == TAB_LIBRARY` that `open_library` (✕ on the card or on Open, via `activate_card`) enters and `close_library` (the back
    button - ○, or ✕ with ○ as confirm - on a card of the grid, also when the Library is empty) leaves; both go through
    `set_tab`. `switch_tab` (L/R and ✕ on a tab) only moves between Games and Music. `top_tabs()` gives the tabs the bar shows
    ({Games, Music}, or {Library} inside the Library) and `draw_top_bar` and the tab-fit code use it, so the word "Library" is
    drawn bold where Games is. It is not selectable: `update_top_bar` takes the tabs from `top_tabs()` only when
    `ui->tab != TAB_LIBRARY`, so inside the Library the only selectable thing is `TOP_COG` (`top_focus` is never `TAB_LIBRARY`, so
    `top_anim[TAB_LIBRARY]` stays 0 and the word never gets a frame); ↑ from a first-row card that is not under the Sort button calls
    `enter_top_bar(ui, TOP_COG)`. `sel_card()` returns the raw entry of
    the selected card, `ui_selected()` returns -1 for the Library card (so `main.c` never starts or deletes it), `remap_index`
    keeps the sentinel, `build_views` asks `home_build` for `count + 1` places, `loader.c` skips negative indices
    (`load_icon_near`, `loader_focus`).
  - *Card.* `draw_card` has a branch for `HOME_LIBRARY`: a card in `theme_cur()->card` (the colour of a game without a
    picture, opaque) with the picture from `gfx_library_icon` (see "The Library picture is an icon of the icon framework" above).
    The card's pixel box `l, t, rt, b` (`ceilf(x - 0.5)`, the rule of the cover and the ring) is computed at the top of
    `draw_card`, and both the face and the picture use it. The picture dims like a cover when the card is not selected (tint 200
    to 255). Nothing from the console is used.
  - *Under the card.* `fit_label` gives the title `S_TAB_LIBRARY` without a type tag, the Start button reads `S_F_OPEN`, ✕ on it
    does the same as ✕ on the card. START / ⋯ open a popup with one choice (`MI_PIN` or `MI_UNPIN`, `home_do`), toast
    `S_HOME_PINNED` / `S_HOME_UNPINNED`. `draw_empty` is only the *No games found* note and the two hints, shown under the Library
    card when the list has no games (and in an empty Library); `S_HOME_EMPTY` and `S_HOME_EMPTY_HINT` were not used any more and
    are removed (the string table has 162 rows).
  - *Tests* (`host-tests`, not in git): `home/home_test.c` (the Library card at the end, pinned first, 10 cards, 8 pins, legacy
    9 pins, random operations with the Library card), `ui/menu_test.c` (new tab order, the Library card section, per-language
    checks of the title, the Open button, the top bar with only the word "Library" and the hint, the card's colour and picture in
    four themes at both sizes), `ui/pixel_test.c` (the Library card at rest, pinned, empty Home, Library top bar), `ui/drive.c`,
    `run_all.sh` (162 strings).

## Version 1.0.0 (psp5-authentic)

Everything that changed compared with the original's v1.5.3 – see `CHANGELOG.md` for the version written for users.

- **Home and the options menu:** the Games tab is Home: the cards opened last (at most ten, newest first) and the ones
  that were pinned (at most nine, first). Saved in `<data>/home.txt` (`src/home.c`); the cards are named with
  `entry_cover_key` (the folder name for an EBOOT, the game ID for an ISO). START opens a small options menu
  (`draw_menu`) for the selected card: Manage Saved Data, Delete, Information, Remove from Home / Add to Home, Pin /
  Unpin. ✕ chooses, ○ or START closes. Under the card there are the buttons Start and ⋯ (options): ↓ selects Start,
  ← → move between them, ↑ or ○ go back to the card; ✕ on ⋯ opens the same menu. The UMD comes first and does not count
  among the ten.
- **The whole main menu is controlled with the D-pad:** the button hints (L R Tab / △ Settings at the bottom right, and
  ✕ □ ◀▶ in Music) are gone, and △ no longer opens the settings. ↑ from the first thing on a tab (the card in Games, the
  first row in Library, the first track in Music; also an empty tab) selects the top bar (`top_focus` in `ui.c`): the tabs
  and the cog wheel are buttons with the same frame as the Sort button (`popup_choice`), ← → move along it, ✕ on a tab
  opens it (the tab turns bold as before) and ✕ on the cog wheel opens the settings. ↓ goes back to where you came from
  (to the Sort button, if ↑ came from there) and ○ always goes back to the selected game; from the Sort button, ↑ gives
  the cog wheel. L/R switch tabs as before. A held ↑ never moves on into the top bar by itself.
  In the Library, ↑ selects the Sort button only from a card in the first row that sits right under it (the middle of
  the card lies within the button's frame, `sort_above` in `ui.c`; the button is only a stop when there is more than one
  card); from all other cards ↑ goes straight to the top bar, so nothing is in the way. Before, ↑ always went via the Sort
  button. The cog wheel sits 4 px further to the left. Start/⋯ and the Library's title row are on one line (centred on
  y = 243), so that the highlight ends 16 px from the bottom edge, as far as the top bar's text and icons are from the
  top edge.
- **The sort frame no longer flickers:** the frame round the Sort button was switched on and off while you passed through the
  tabs and the cog wheel, although the button was not selected. It now has its own animation (`sort_frame`) that only lights up
  when the button is selected and the top bar is not; `sort_anim` still controls how highlighted the button is (and that the
  card under it is dimmed).
- **Everything is drawn on whole pixels:** nothing drawn at rest has an edge between two pixels (that gave 1 px on one side
  and 2 px on the other, and soft icons). The Library's cards are 80 × 44 px with 8 px between them (before: 78.4 × 43.6 and
  10 px) and the selected card is 6 × 4 px larger (`SELECT_GROW_W/H`); the picture and the ring are drawn on the same
  whole-pixel box (`draw_card`). `gfx_frame` rounds edges and thickness to whole pixels. The battery (the fill 14 px), the
  cog wheel (radius 7), the bolt, the Options dots (radius 2), the switches, the file icon, the padlock, the hint rows, the
  toast and the colour swatches sit on whole pixels; text widths are rounded up with `ceilf`. The check is in `mockge.c`
  (`g_audit` counts drawn rectangles and pictures that have an edge between pixels; `MOCK_AUDIT=1` prints where they come
  from) and is tested in `ui/pixel_test.c` and in the settings, info, files and saves tests. Soft shadows (`gfx_glow`) and
  the background's beam of light and dust are soft on purpose and are not counted. The position of the text is unchanged
  (it cannot be measured without intraFont).
- **Sorting in Manage Saved Data** with the same Sort button and popup as the Library (`set_sort`, `draw_sort_menu` in
  `saves.c`): ↑ from the first row, ✕ opens the choices Last changed / Size / Name, ○ goes back. L/R and the key icons are
  gone.
- **The console-colour themes have a beam of light, golden dust and haze** (`deco_spot` in `theme.c`, `beam` and `dust` in
  `Theme`): a wide beam from the lamp at the top left, a band of dust and bokeh, a cool haze at the bottom left. The picture
  stands still and is the same on all screens. PS1–PS4 keep their own patterns.
- **The start-up screen no longer has a final blue picture:** the old beam of light in `boot.c` (and, for a while, the theme's
  surface) is gone. The dust keeps floating on black until the start-up screen fades out (`T_FADE` 6.6 s, `T_END` 7.15 s)
  and the menu fades in. The start-up sound is 5.2 s from the health notice (2.55 s) and finishes within the 400 ms that
  `sound_wait` waits. The circles and the dust have the theme's colours (`make_palette`): the big dark circles in the theme's
  `intro` colour, the small light ones in the haze's, the dust in the theme's `dust`/`beam` (PS1–PS4 have no `dust`: they use
  the accent colour). The logo and the health notice are grey.
- **PS1–PS4 also have the beam of light and the haze** (`beam` in the theme; `deco_spot` is drawn for all themes under the
  theme's own patterns) but no dust (`dust` = 0).
- **The background's tint from the game's icon is removed** (it cost performance): `accent_target`, `ui->accent`,
  `IconSlot.accent`, `texture_average`, `Theme.tone` and the `tone` parameter of `theme_backdrop` no longer exist.
- **The beam of light covers the whole haze at the bottom, and the background is the same everywhere:** the beam
  (`deco_beam` in `theme.c`) is a mesh of small quads with their own brightness at each corner, not five layers of large
  quads. A quad is split into two triangles and the colour is only blended between a triangle's three corners, so the right
  half of every large quad became almost dark and the beam got a hard edge along the diagonal (it showed as a sharp edge that
  ended at x = 235 at the bottom, while the haze reached x = 430). Now the beam leans to the right and widens downwards: the
  right edge is soft but clear and ends at x = 400 at the bottom, the left side is a long soft slope. The shape is measured
  against the reference's brightness. The haze at the bottom is dimmed so that the beam and the haze together are about as
  bright as the reference. The menu used to be darkened by three black gradients in `draw_background` (top, left, bottom)
  over both the picture and the surface, and had its own darker base tone; the base tone is gone and the gradients now lie
  only over a game's background picture (with the same strength as before, in proportion to how much of the picture is
  visible, so that they fade in and out with the picture). All screens draw the same surface (`surf_top`, `surf_bottom` =
  before `stop`, `sbottom`); the surface is only darkened by the Library and Music (`draw_dim`) and by the popups. The
  `bottom` fields are gone; `top` is now called `intro` and is only used by the start-up screen's circles (so that they look
  as before).
- **The selected card keeps its size when the focus is somewhere else:** the ring round the card (`card_ring` in `Ui`,
  faded over 1/8 s) goes away when the Sort button, Start / Options or the top bar is selected and comes back when the card
  is selected again. The card in the Library no longer shrinks when Sort is selected (it is 86 × 48 all the time); its
  dimming of the brightness remains. `draw_card` has got the parameter `ring` (before, `focus` controlled both the brightness
  and the ring).
- **The highlights sit in the middle of their text and on whole pixels:** a label looks centred when the baseline is about
  6.45 × the text size below the middle of the frame (like the choices in the popups). `popup_label_mid` and
  `popup_frame_around` in `popup.h` give the frame's top edge and height (the height or one pixel more, so that the edge
  lands on a whole pixel); the tabs' frame follows the text size (it shrinks in long languages), and the same goes for the
  cog wheel and the Sort buttons. `popup_choice` rounds the edges to whole pixels: a frame on parts of a pixel got 1 px on
  one side and 2 px on the other (rectangles are drawn with the pixels whose centre is covered).
- **The same background everywhere, and the same darkening in Library and Music:** Games, Library and Music drew the beam of
  light, the dust and the haze at full strength, while the settings, the information, the file manager and saved data drew
  them at 60 %, so Games looked brighter. `theme_backdrop` now has no parameter for list screens and draws the same
  everywhere: the beam of light (the beam and the lamp, `BEAM_STRENGTH` in `theme.c`) at 60 %, everything else (haze, dust and
  the PS1–PS4 shapes) at full strength. The PS1 symbols sit in a single place (top right, where the lists are not), so the
  theme looks the same on all screens. The Library and Music put the same darkening (`DIM_COLOR`, 8 8 12 with alpha 150, as
  Music had; the Library had 165) over the Games background, a game's picture or the theme's surface.
- **The background is drawn once (FPS):** the surface, the glows, the beam of light with haze and the dust look the same in
  every frame, but were redrawn every time: about 500 calls that touch every pixel three to four times (measured in the test
  environment). `theme_backdrop` now draws that part once into a picture in video memory (`gfx_snapshot_begin/end`, behind
  the two screen buffers, `sceGuDrawBufferList`) and shows it from there as one opaque picture in strips of 64 pixels
  (`gfx_snapshot_draw`). It is redrawn when the theme changes, after `gfx_init` has run again, and after sleep mode
  (`theme_backdrop_forget`). The PS1–PS4 shapes move and are still drawn every frame on top.
- **Fixes from the original project (psp5 1.5.4, the issues on GitHub):**
  - **.DAX games** (`scan_iso_dir` in `scan.c`, the delete guard in `gameops.c`, the colour in the file manager): libcisoread
    reads DAX and ARK starts it, so it was enough for the scanner to include the extension. The Information screen recognises
    DAX (`DAX_MAGIC`) and reads the unpacked size as 32 bits at position 4 in the header (CSO/ZSO have 64 bits at position 8).
  - **USB mode and the file manager jumped back at once:** `usb_mode` and `files_mode` in `main.c` only ignored a ✕ that was
    held down when they opened; with ○ as the confirm button they exited immediately. Now all buttons are ignored until they
    have been released (`old = ~0u`). `sys_usb_start` also does not give up if `sceUsbActivate` answers with an error, like
    arkMenu.
  - **PSP Go, psp5 started from the internal storage:** ARK then redirects `ms0:` to `ef0:`, but only during psp5's own
    session, so the games got the path `ms0:` and were started as if from a memory stick (PS1 games: error 800201A8, ISO:
    froze). `scan_all` now always scans `ef0:` and `ms0:` only when it is a real stick (`sys_ms_is_ef`), and `launch.c` then
    picks the Go modes (`on_ef0`). Home and the covers use the game's ID or folder name, not the drive, so nothing is lost; the
    scan cache is rebuilt once.
  - **Homebrew on the PSP Go's internal storage** (the GTA III and Super Smash Bros ports froze or switched the PSP off): is
    now started with apitype 0x152 (`HOMEBREW_RUNLEVEL_GO` in `launch.c`) like arkMenu, so that ARK redirects `ms0:` to `ef0:`
    and ports that read their data files from `ms0:` find them. Homebrew on a memory stick is started as before with 0x141.
    The test `host-tests/launch` checks the apitype for every kind of game and drive.
  - **The background picture is drawn in narrow strips** (`gfx_tex` in `gfx.c`): a picture wider than 64 texels is drawn as one
    sprite per 64 texels in the same call. The GE's texture cache copes with that much better than with a sprite as wide as
    the screen, and the games' pictures behind the cards are the biggest thing drawn. The strips' borders are moved to whole
    pixels and get the texture coordinate that belongs to the moved border, so that the picture looks the same and the strips
    meet without gaps or overlap.
  - **"Boot psp5" showed off** until the first game start: a paused launcher mode is switched on by `start_confirmed()` only
    after the menu has worked for a while, but the settings read the mode as soon as they opened. `main.c` now calls
    `start_confirmed()` before `settings_open()`.
  - **A crackle when the settings opened:** the sound was played in `ui.c` before the settings were read in (the font from
    flash0, ARK's files), and the heavy reading disturbed the mixer. `ui.c` no longer plays anything; `main.c` plays
    `SND_SELECT` after `settings_open()`.
  - **PSP minis' square icons are no longer stretched** (`fit_icon` in `art.c`): an icon narrower than 1.5 : 1 keeps its shape
    in the middle of the card, on a darker (55 %), blurred copy of itself (blur 8) that fills the space beside it. Other icons
    are scaled as before. The icons that were already in `cache/` are stretched, so `art_init` removes all `*.ico` there once
    (the marker `cache/icons2`, the same name as in psp5 1.6.0); `covers/` is not touched.
- **SELECT opens a power popup** (`MENU_POWER`, `menu_build_power`, `open_power_menu` in `ui.c`): on all tabs (also with the top
  bar selected) the same dark panel as the options menu is shown in the middle of the screen, 200 px wide, with Sleep,
  Shut down, Restart and Recovery menu – the same texts (`S_SLEEP`, `S_SHUTDOWN`, `S_RESTART`, `S_RECOVERY`), the same order
  and the same function as at the bottom of Settings → Power and USB. ✕ (or ○ with that setting) does the choice: `ui_update`
  returns `UI_SLEEP`, `UI_SHUTDOWN`, `UI_RESTART` or `UI_RECOVERY` and `main.c` runs the same `run_action` as the settings. ○,
  △, START or SELECT close the popup without doing anything.
- **The confirmation step "Press again to confirm" is gone:** `confirm` in `Item`, `CONFIRM_TIME`, `confirm_item`/`confirm_time`,
  the parameter of `action()` and the string `S_CONFIRM` (all 11 languages) no longer exist; Sleep, Restart, Shut down and
  Recovery menu run on the first ✕. The order in Power and USB is now Sleep, **Shut down**, **Restart**, Recovery menu.
  `tools/tests/settings.txt` no longer presses ✕ on Sleep (it would put PPSSPP to sleep).
- **Version 1.0.0** (`settings.c`, `tools/make_release.py`, `INSTALL.md`): the zips are called
  `psp5-authentic-v<version>-…`. `CHANGELOG.md` has all changes compared with the original's v1.5.3.

## Version 1.5.3 (the original – the base of the fork)

- **Category folders:** a folder in `PSP/GAME` without `EBOOT.PBP` (e.g. `GAME/PS1/<game>` or
  `GAME/Emulators/TempGBA`) is searched one level down, as arkMenu does (`scan_game_dir`, `depth`). Before, PS1 games and
  apps in such folders did not show up at all.

## Version 1.5.2

- **Move between Games and Apps:** □ on a card in Games or Apps opens a small options menu (`draw_menu`) with the card's name
  and *Move to Games/Apps*. ✕ chooses, ○ or □ closes, so nothing is moved by mistake. Saved in `<data>/moved.txt`, one line
  per card with `entry_cover_key` (the folder name for an EBOOT, the game ID for an ISO). The UMD has no menu. The hint
  "□ Options" is shown faintly next to the Start button.
- **File manager** (`src/files.c`, Settings → Power and USB): runs like the USB mode, with the rest of psp5 closed
  (`files_mode` in `main.c`), and the game list is read again afterwards. ms0: and, on the PSP Go, also ef0: (drive list at
  the top), never flash0. Folders first, sorted by name; □ marks, △ (or ✕ on a file) opens the menu: Paste here, Copy, Move,
  Rename, Delete, New folder. Copy/move/delete run in their own thread (`worker`) with progress and ○ to cancel; a cancelled
  or failed copy is cleaned up, and the free space is checked first (`sceIoDevctl` 0x02425818). A move on the same drive is a
  `sceIoRename`, otherwise copy + delete. If the name already exists it becomes "name (2)". Delete always asks, with Cancel
  preselected. Names are typed with the PSP's keyboard (`sceUtilityOsk*`, via `gfx_end_with`). The free space is shown with a
  thin bar at the top right.

## Version 1.5.1

- **Volume indicator** (`src/osd.c`): the volume buttons go straight to the system, so psp5 reads the volume (0–30) and
  sound on/off with the kernel's `sceImposeGetParam` – ARK finds the function (`sctrlHENFindFunction`, ARK translates the NID
  0x531C9778 for 6.60/6.61, otherwise 0xDC3BECFF is tried) and kubridge's `kuKernelCall` calls it. When the value changes, a
  bar slides in at the top for 1.6 s, in all tabs, the settings and the USB mode. Without ARK (PPSSPP) nothing is shown.
- **Seeking in the music** (`seek_to` in `src/music.c`): a short press of ← / → changes track when the button is released,
  hold for 0.35 s to seek (8, then 20 and 45 seconds per second). The decoder is restarted at the right byte in the file: the
  Xing table for VBR, otherwise proportionally, and then the first valid frame (the same version, layer and frequency as the
  track's first, and a valid frame right after it). The first 8 frames after a jump may fail without the track being changed:
  the PSP's decoder answers `MP3_END` (0x80671402) instead of an error for a frame that lacks data from the frames before it
  (the bit reservoir). Seeking stops one second before the end.
- **Covers in the music** (`art_load_cover`, `music_cover_data`): from the ID3 tag's APIC/PIC (preferably the front cover,
  v2.2–2.4 with unsynchronisation), otherwise `cover.jpg`, `folder.jpg`, `front.jpg`, `album.jpg`, `cover.png` or
  `folder.png` in the album folder. JPEG is decoded with libjpeg-turbo (scaled down already in the decoder), PNG with libpng.
  64×64 with rounded corners, saved in `cache/` per album and loaded in the loader thread. `loader_stop` now runs before
  `music_term`, because the loader reads the music list.
- **Charging animation:** while the battery charges, the fill sweeps from the level up to full, with a bolt beside it.
- Test script `tools/tests/media.txt` and new test commands: `hold BUTTON:N`, `volume N`, `charging 0/1`, `battery N`.

## Version 1.5.0

- **Themes** (`src/theme.c`, new *Theme* tab in the settings): the default colour theme (as before), the console-cover
  colours (Midnight Black, Cosmic Red, Nova Pink, Starlight Blue, Galactic Purple, the Deep Earth series Volcanic Red/Cobalt
  Blue/Sterling Silver and Chroma Indigo/Teal/Pearl) and PS1–PS4 as a setting for the *Appearance*: PS1 grey with the
  controller's symbols, PS2 dark with blue towers and orbs, PS3 with the XMB wave and PS4 blue with waves and particles.
  Everything is drawn in code. The highlighted theme is previewed in the settings, ✕ chooses it. The theme colours the
  background (when the game has no picture), the settings, the USB mode, the charging screen, the placeholder cards, the
  switches, the music's progress and the charging dots. *Game art in the background* can be switched off – then the theme is
  always visible and no background pictures are loaded. Saved in `psp5.cfg` (`theme=`, `themeart=`).
- **Faster drawing:** the settings re-wrapped the category description (many `text_width` calls) for every row in every
  frame – now only when the text changes. The widths of the tabs and the hints are only measured again when something
  changes. `gfx.c` does not rebind the same texture (and does not flush the texture cache) for every shape in a row. The
  surface under the game's picture is not drawn when the picture covers the whole screen. The icon's average colour is
  calculated directly from the texture instead of on a copy (46 KB less to copy per icon).
- **Fixed:** a background picture that finished loading just as the highlight moved was thrown away and then never loaded
  again if you went back to the game (`s_bg_last` in `src/loader.c`). A broken directory entry in an ISO (too short an
  entry or too long a name) could be read outside the sector buffer (`src/iso.c`).
- Test script `tools/tests/theme.txt`; `settings.txt` and `lang.txt` have one more step for the new tab.

## Version 1.4.0

- **PSP Go** – the same EBOOT on all models: `sys_set_root` (`src/system.c`) picks `g_root` from argv[0]. If psp5 is started
  from `ef0:` (the PSP Go's internal storage), psp5's files are in `ef0:/PSP5`, ARK's default folder and `SEPLUGINS` are
  there, and the hint texts show `ef0:/`. Games (`ISO`, `PSP/GAME`) and music (`MUSIC`) are always searched on both `ms0:` and
  `ef0:`, like arkMenu – so also a Memory Stick Micro in a PSP Go. Games on `ef0:` are started with ARK's apitypes for the
  internal storage (0x125/0x126 ISO and PSN, 0x155 PS1; homebrew as usual 0x141), and updates (`PBOOT.PBP`) are searched on
  both. No UMD thread on the PSP Go (model 4). Before, psp5 only looked on `ms0:`, so games in `ef0:/PSP/GAME` and music in
  `ef0:/MUSIC` did not show up on the PSP Go. The release zips `psp5-v1.4.0-PSPGo-ARK5.zip` and `-PSPGo-ARK4.zip` have the
  same EBOOT as the ARK5/ARK4 zips, only with a READ ME for the PSP Go (`tools/make_release.py`).
- **Settings on the PSP Go** (`src/settings.c`, model 4): *Disable Pause Game* (`disablepause`) and *Keep the paused game*
  (`hibblock`) are shown, *Disable UMD drive* is hidden – as in arkMenu.
- **Build in GitHub Actions** (`.github/workflows/build.yml`): both versions and all four zips.

## Version 1.3.3

- **Start-up guard against semi-bricks** (`src/main.c`): `ms0:/PSP5/starting` is created first of all at start-up and
  contains the number of starts in a row that did not finish. The file is removed when the menu has run for 5 s and the music
  list has been read, or when a game is started or something is chosen in the settings.
  - 1 failed start: **safe start** – no start-up screen, no music, no UMD thread, `ms0:/PSP5/cache` is emptied and a toast is
    shown.
  - 2 in a row as ARK's launcher: psp5 goes straight to the XMB (`sys_exit_xmb`, the launcher mode is paused) and writes 1 to
    the file, so that the next start as a launcher is a safe one instead of the XMB again.
  - A paused launcher mode is only switched on again when a start has finished (`sys_resume_launcher`), not directly in
    `sys_init`.
- **Sleep mode without graphics:** when the PSP falls asleep, two black frames are drawn, then psp5 waits with
  `sceKernelDelayThread` instead of drawing all the time while the PSP falls asleep and wakes up. No start-up screen after
  sleep mode in safe mode.
- `sctrlArkSetConfig` is only called with a configuration that could be read (`IS_ARK_CONFIG`).
- The HOME menu in games: psp5 is not in memory then (LoadExec restarts the system) and starts games with exactly the same
  call as arkMenu. Help text about plugins and ARK's combination L + R + START + ↓ in INSTALL.md.

## Version 1.3.2

- **No menu before the start-up screen after sleep mode:** "finished waking up" (`PSP_POWER_CB_RESUME_COMPLETE`) arrives about a
  second after the screen has lit up, and until then the menu was visible. Now the screen is drawn black already when the PSP
  falls asleep (`PSP_POWER_CB_SUSPENDING`) and is kept black until it has finished waking up, then the start-up screen
  (`sleep_and_wake` in `src/main.c`, at most 4 s awake without a signal). The music is paused when the PSP falls asleep.
  Test script: `tools/tests/resume.txt` (`sleep 1`, `resume 1`).

## Version 1.3.1

- **No start-up screen after a game:** as ARK's launcher, psp5 is restarted when a game ends. Before a game starts,
  `ms0:/PSP5/ingame` is created; if the file exists when psp5 starts, it is removed and the start-up screen is skipped. When
  psp5 was not started as ARK's `VBOOT.PBP` (Game → Memory Stick) the file is not created.
- **Start-up screen after sleep mode:** a power callback (`scePowerRegisterCallback`, `PSP_POWER_CB_RESUME_COMPLETE`) shows
  the start-up screen again when the PSP is woken with the side button. The start-up sound is recalculated in the sound
  thread (`sound_prepare_boot`), music is paused in the meantime and the cards slide in again (`ui_replay_intro`).
  Test script: `tools/tests/resume.txt` (`resume 1` pretends the PSP woke up).
- **The UMD first under Games** (`src/umd.c`): a thread looks for the disc as arkMenu does (sceUmdCheckMedium,
  sceUmdActivate, wait for PSP_UMD_READY) and reads `disc0:/PSP_GAME/PARAM.SFO`. The disc is put first in the list when it is
  inserted and removed when it is taken out, and the highlight stays on the same game. It is never saved in the list cache.
  Start: apitype 0x120 with `disc0:/PSP_GAME/SYSDIR/EBOOT.BIN`, or 0x160 with `ms0:/PSP/GAME/<DISC_ID>/PBOOT.PBP` if an
  update exists. In the test build, `ms0:/PSP5/fakeumd/` works as a disc (test script `tools/tests/umd.txt`).

## Version 1.3

- **ARK-4 and ARK-5, PSP-1000/2000/3000:** two builds – `tools/build.sh` (ARK-5, links ArkCtrl) and `tools/build.sh ark4`
  (ARK-4, `-DPSP5_ARK4`, without ArkCtrl → `dist-ark4/`). Both detect the ARK version (`sctrlSEGetVersion()`: 4/5) and the model
  (`kuKernelGetModel()`: 0 = PSP-1000) at start-up. On ARK-4 the ARK configuration is read through a separate
  SystemCtrlForUser import (`src/ark4stub.S`) and SETTINGS.TXT is written in ARK-4's format (overclock/defaultclock/powersave,
  highmem/mscache on/off). The PSP-1000 hides extra memory and USB charging.
- **The "ARK not found" error:** `sctrlHENGetVersion()` only gives the middle version (0 on ARK 5.0.x) – now the major version
  is used.
- **Music:** the Music tab plays MP3 from `ms0:/MUSIC` (sceMp3, its own thread, `src/music.c`), like the XMB's player.
- **A start-up screen like the console's:** grey logo on black → health notice with dark blue bokeh → golden dust → beam of
  light.
- The heap leaves 7 MB free (ARK's RECOVERY.PRX needs 4 MB).

## Start-up screen and library (1.2.0)

- **Start-up screen:** particles of light swirl in towards the middle, meet in a flash and leave the psp5 logo behind while a
  chord of its own is played. Everything is drawn and calculated in `src/boot.c` / `src/sound.c`. Any button skips it; it is
  turned off under Settings → psp5 → Startup animation (`bootanim=0` in `ms0:/PSP5/psp5.cfg`).
- **Library:** the second tab (L/R). All games and apps in a grid with 5 columns, ↑↓←→ move. ↑ from the first row that sits
  under the button selects the Sort button (from the others ↑ goes straight to the top bar), ✕ opens the sorting (name /
  recently added), ○ or ↓ back to the cards. START opens the options menu for the selected card.

## Languages
All texts are in `src/lang.c` (English, Swedish, German, French, Spanish, Italian, Dutch, Portuguese, Russian, Japanese,
Czech). The language follows the PSP's system language; Swedish and Czech are chosen manually under
Settings → psp5 → Language (saved in `ms0:/PSP5/psp5.cfg`).

## Sounds
The interface sounds are WAV files in `PSP5/sounds/` on the storage (`move`, `select`, `back`, `error`, `boot`; 44.1 kHz,
16 bit; the zips contain them). If a file is missing, psp5 plays the built-in sound with the same name – they are calculated
in `src/sound.c`, no sound files are needed. `tools/convert_sounds.py` converts your own recordings to the right format.
On/off under Settings → psp5 → Interface sounds (saved in `ms0:/PSP5/psp5.cfg`).

## Settings (the cog wheel)
Laid out like a console's settings pages, and reads/writes the same files as arkMenu:
- **psp5:** autoboot, interface sounds, startup animation, show battery percentage, language, clear image cache, version
- **Performance:** CPU in games / XMB, extra memory, memory stick cache, ISO cache
- **System:** USB charging, WPA2, logos, hide pictures/MAC/DLC, lights, UMD, analog stick, regions and more
- **Plugins:** switch rows in `ARK_01234/PLUGINS.TXT` and `SEPLUGINS/PLUGINS.TXT` on/off
- **Power and USB:** USB mode, file manager, XMB, sleep, shut down, restart, recovery menu (the same four as in the SELECT
  popup; no confirmation)

The CFW settings (`SETTINGS.TXT`) are written in exactly the same format as arkMenu and take effect after a restart.
Unknown lines and comments are kept.

## Installing on the PSP
1. Connect the PSP with USB.
2. Copy `dist/PSP` to the root of the memory stick (so that the file ends up in `PSP/GAME/PSP5/EBOOT.PBP`).
3. As ARK's launcher: copy the same file to `PSP/SAVEDATA/ARK_01234/VBOOT.PBP` (save arkMenu as `VBOOT_ARKMENU.PBP` first).
4. (Optional) Prepare covers: `py tools\covers.py E:\` (E: = the PSP).
5. Start "psp5" from the XMB → Game → Memory Stick, or via ARK's Custom Launcher.

## Controls
| Button | Function |
|---|---|
| ← → / the stick | browse (hold to go faster) |
| L / R | switch tab (Games / Library / Music) |
| ↑ from the first thing on a tab | select the top bar: ← → between the tabs and the cog wheel, ✕ opens the tab (or the settings on the cog wheel), ↓ or ○ back |
| ✕ (○ with the Japanese button setting) | start (on the card or the Start button) |
| START | options menu for the selected card (Games and Library) |
| SELECT | power popup in the middle of the screen: Sleep, Shut down, Restart, Recovery menu (all tabs) |
| ↓ in Games | select the Start and ⋯ buttons under the card (← → between them, ↑ or ○ back) |
| HOME | back to the XMB (the system's own question) |

## What is shown
- `ms0:/ISO` (+ one level of sub-folders): `.iso`, `.cso`, `.zso`, `.dax`, `.jso`
- `ms0:/PSP/GAME/*/EBOOT.PBP`: PSN and PS1 games and apps
- Everything shows up in the **Library**; **Games** (Home) shows the cards opened last and the pinned ones
- The UMD in the drive: first under **Games** and in the **Library**

Games are started with the same systemctrl calls as ARK-5's arkMenu (`src/launch.c`), including installed updates
(`PBOOT.PBP`), patched ISOs (`EBOOT.OLD`) and an unencrypted `BOOT.BIN`. Before the start everything is released: the loader
thread, textures, fonts, GU and the game list.

## Pictures and cache
Card and background pictures are looked for in this order:
1. `ms0:/PSP5/covers/<key>.ico|.bg` – from `tools/covers.py` (key = DISC_ID for ISO and UMD, folder name for EBOOT)
2. `ms0:/PSP5/cache/` – created by the launcher itself the first time a game is shown
3. ICON0/PIC1 inside the game

Only the cards near the highlight are loaded (in a background thread), and the background is loaded only when the highlight
has stood still for a short while. The game list is cached in `ms0:/PSP5/cache/list.bin`, so unchanged files are not read
again. Delete `ms0:/PSP5/cache` (or Settings → Clear image cache) if something looks wrong – it is rebuilt.

### Your own covers
Put pictures in `tools/covers-src/` (or give `--src`), named after the game ID or title:
`ULUS10041.jpg` or `My Game.png` → card picture, `ULUS10041_bg.jpg` → background. The pictures are cropped automatically.
Flags: `--force` (rewrite all), `--clean` (remove covers for games that no longer exist), `--dry-run`.

## Building (Windows + WSL1)
The toolchain (pspdev + psp-cfw-sdk) is in the WSL1 distro `pspdev` (`C:\WSL\pspdev`). WSL1 is used on purpose – WSL2 turns on
the hypervisor, which clashes with VMware.

```
wsl -d pspdev --exec bash /mnt/c/path/to/psp5/tools/build.sh
powershell -ExecutionPolicy Bypass -File tools\run-ppsspp.ps1      # test in PPSSPP
```

| Script | What it does |
|---|---|
| `tools/setup-wsl.sh` | installs pspdev + packages in WSL (run as root) |
| `tools/build.sh` | builds to `dist/PSP/GAME/PSP5/EBOOT.PBP` |
| `tools/make_icon.py` | draws psp5's own ICON0/PIC1 into `res/` |
| `tools/make_testdata.py` | made-up test games (iso/cso/zso/EBOOT) in `testdata/` |
| `tools/run-ppsspp.ps1` | copies to PPSSPP's memory stick and starts it |
| `tools/ppsspp-capture.ps1` | sends keys to PPSSPP and takes a screenshot |
| `tools/covers.py` | prepares covers on the memory stick |
| `tools/build.sh test` | test build with a scripted test mode (`src/testrun.c`) to `dist-test/` |
| `tools/ppsspp-test.ps1` | runs a test script (`tools/tests/*.txt`) in PPSSPP and saves screenshots – without key presses |

PPSSPP does not have ARK's systemctrl, so "ARK not found" is shown there when you press ✕. The game start itself has to be
tried on the PSP.

## Source code
| File | Contents |
|---|---|
| `src/main.c` | start-up, main loop, releasing before a game starts |
| `src/ui.c` | layout, animations, tabs |
| `src/gfx.c` | a small GU renderer + intraFont text |
| `src/loader.c` | background thread that loads pictures |
| `src/art.c`, `src/image.c` | covers/cache, PNG, scaling, blur, NTX format |
| `src/scan.c`, `src/iso.c`, `src/sfo.c` | scanning, ISO/CSO/ZSO/DAX/JSO, PARAM.SFO |
| `src/lzo.c` | LZO1X unpacking for JSO |
| `src/folders.c` | the folder of a game (Library), its name and letter |
| `src/launch.c` | start through ARK-5's systemctrl |
| `src/settings.c` | the settings screen |
| `src/arkcfg.c`, `src/plugins.c` | ARK's SETTINGS.TXT and PLUGINS.TXT |
| `src/system.c` | USB, power, XMB, recovery menu |
