# ZeroCam Nintendo 3DS Port

This is a native homebrew implementation of the camera app. The existing React/Vite app remains unchanged; its DOM and browser media APIs do not run as a 3DS application.

## Included

- Two-screen XP-style camera UI with a full top-screen preview/HUD and a preview on the bottom Paint workspace
- Live outer/inner-camera preview using libctru's camera service
- Thirteen selectable color effects, including calibrated background Cutout, face-tracked Hearts, a blurred Dream effect, and a four-shade Game Boy camera view with brightness, contrast, and dither controls
- Still photos saved as JPEG files under `sdmc:/3ds/ZeroCam/Gallery/`
- 288x176 MJPEG video at JPEG quality 86 with mono microphone audio saved in the same folder in QuickTime MOV containers, targeting up to 14 FPS with actual frame timing and a 30-second limit; photos remain 400x240. A background worker buffers up to eight frames and may still drop frames during sustained overload.
- Paged thumbnail gallery for JPEG/BMP photos and ZeroCam MJPEG MOV videos from the Gallery folder and the older `sdmc:/3ds/ZeroCam/` folder
- Gallery video play/pause and confirmed deletion of selected or all gallery media
- Software-rendered symbol buttons, filter swatches, adjustment sliders, antialiased touchscreen drawing, and keyboard text placement
- Touch-enabled File and Edit menus, photo renaming, and four levels of drawing undo/redo
- Selectable HUD themes and an optional date/time stamp on photos and video
- Decorative heart, star, film, and rainbow frames for photos and video
- A touch-selectable 4, 6, or 8-second photo timer
- Hardware-button controls and separate `.3dsx` and optional `.cia` build targets

## Not Yet Ported

Persistent settings and the browser-style desktop/boot sequence need further native implementation. Filter algorithms are lightweight approximations of the web versions and have not been tuned on hardware.

## Build

Install devkitPro with the 3DS development tools, `3ds-libjpeg-turbo`, and `3ds-libpng`, then run these commands from the devkitPro MSYS2 shell:

```sh
cd 3ds
make
```

The homebrew target is `3ds/zerocam.3dsx`. To also build an installable CIA, install `makerom` and run:

```sh
make cia
```

The CIA target uses makerom's test key-chain and the default `app:4` access-description template. It is an optional homebrew packaging target, not a Nintendo eShop or retail-signed package. Test on an emulator or a 3DS set up for homebrew.

ZeroCam creates `3ds/ZeroCam/Gallery` on the SD card. Its gallery reads photos from that folder; it does not add them to the Nintendo 3DS Camera application's gallery. A failed save is reported on the bottom-screen status line.

Gallery playback supports ZeroCam's MJPEG MOV recordings, not arbitrary MOV formats. Older recordings made without audio remain silent. Gallery audio requires a usable DSP component, normally `sdmc:/3ds/dspfirm.cdc`; if unavailable, the app reports `AUDIO UNAVAILABLE`. Recording requires microphone access and reports `MIC UNAVAILABLE` if it cannot start. `DELETE ALL` deletes JPEG, BMP, and MOV files in both ZeroCam gallery folders after confirmation, including files beyond the 60-item display limit.

Game Boy mode offers four selectable four-tone palettes: classic DMG green, PICO-8-inspired, C64-inspired, and CGA cyan/magenta. Touch a swatch in the palette row to change the live image and frame colors. These are four-color adaptations, not the full multi-color console palettes. The mode creates `sdmc:/3ds/ZeroCam/Frames/` and loads up to 16 PNG frames placed there. Standard 160x144 and wild 160x224 frame images are supported; the central camera window is kept transparent. The linked [Game Boy Camera frame project](https://github.com/cristofercruz/gb-camera-frames) has no repository license, so its artwork is not bundled. Only add images you have permission to use.

Cutout mode replaces a calibrated still background without a green screen. Set the camera down, leave the scene empty, and touch `CAPTURE BG`; after `BG READY` appears, enter the scene and choose a replacement background. Touch `RECAPTURE` to capture a new empty reference. Before calibration the live camera is shown unchanged. The `DIFF` slider sets the required difference from the reference: move it left to retain subtler changes, or right to suppress more background noise. Camera switching, zooming, mirroring, opening the gallery, or leaving Cutout mode clears the reference, so capture it again afterwards. Calibration is unavailable during recording or a pending photo.

This is approximate background subtraction, not person recognition: keep the camera and background still, and recapture after lighting changes. Move `SENS` right to detect smaller differences more strongly; moving it left reduces false detections. `BRIGHT` and `CONTRAST` adjust the camera subject and share their values with the other camera views. Moving scenery, shadows, or foreground colors similar to the reference can create artifacts or holes. Edges and movement are smoothed, but performance and visual quality depend on the scene and device. Cutout includes four photographic backgrounds and loads up to 16 additional JPG, JPEG, or PNG files from `sdmc:/3ds/ZeroCam/Backgrounds/`; PNG files may be up to 4 megapixels. Leave and re-enter the mode to refresh the folder. Built-in photos are from Unsplash: [beach](https://images.unsplash.com/photo-1507525428034-b723cf961d3e), [road](https://images.unsplash.com/photo-1500530855697-b586d89ba3ee), [Earth](https://images.unsplash.com/photo-1446776811953-b23d57bd21aa), and [mountains](https://images.unsplash.com/photo-1464822759023-fed622ff2c3b).

## Controls

- `A`: take a photo in Photo mode, or start/stop recording in Video mode; select a menu option or gallery item
- `L` or `ZL`: start/stop video recording; `R` or s`ZR`: capture a JPEG photo
- D-pad left/right: cycle filters, or browse photos in the gallery
- D-pad up/down: zoom, navigate menus, or move between gallery rows
- Circle pad up/down: zoom continuously in live view
- `B`: return from Game Boy or Cutout mode, close a menu or gallery, or switch inner/outer camera in the main live view
- `X`: open or close the gallery
- `Y`: switch between Photo and Video modes in live view, or play/pause a selected gallery video
- `SELECT`: open the editing menu; choose a frame, mirror, or timestamp using the D-pad and `A`, or touch an option
- `START`: exit
- Touch `B OUT` / `B IN` to switch cameras, or `PHOTO` / `VIDEO` to switch modes
- Touch the shutter in Photo mode to save a JPEG, or in Video mode to start/stop recording
- Touch `FILE` to save the current image or rename the most recently saved or gallery-selected photo
- Touch `EDIT` for drawing undo/redo or to cycle filters
- Touch `EXTRAS` to cycle HUD themes or toggle the date/time stamp
- Select the rightmost filter swatch or cycle filters to enter Game Boy mode; touch a palette swatch, use its sliders and frame arrows, or touch `BACK` to return
- Select the green filter swatch to enter Cutout mode; capture an empty background, select a replacement, adjust `SENS`, `BRIGHT`, or `CONTRAST`, or touch `BACK` to return
- Touch the clock tool to choose Off, 4, 6, or 8 seconds for photo capture
- Touch the `T` tool, enter up to 32 supported characters, then touch the preview to place the text in the current brush color; opening the keyboard stops an active recording
- Touch a gallery thumbnail to select it; use `PREV` and `NEXT` for more photos
- Touch `DELETE SELECTED` or `DELETE ALL` in the gallery, then confirm or cancel
- Touch the brush tool to open its color palette; choose `PICK`, then touch the preview to sample a color
- Touch `RESET` to clear drawings and restore neutral filter, frame, adjustments, zoom, and mirror settings

The Hearts effect uses the bundled [Pico face detector](https://github.com/nenadmarkus/pico), runs locally, and displays hearts above one detected face. Tracking can miss faces or lag under poor lighting, fast movement, or heavy processing. Decorative frames are fixed around the image edges and do not track faces. Include [PICO_LICENSE.txt](PICO_LICENSE.txt) when distributing the bundled detector or builds containing it.