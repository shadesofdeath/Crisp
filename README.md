<div align="center">

# Crisp

**Screenshots that stay sharp.**

A native Windows screen capture tool — region, window, full screen — with a
pixel magnifier, annotation, OCR and pin-to-screen.

<br>

![Windows](https://img.shields.io/badge/Windows-10%20%7C%2011-0078D4?style=flat-square&logo=windows&logoColor=white)
![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat-square&logo=cplusplus&logoColor=white)
![Win32](https://img.shields.io/badge/Win32-native-1a1a1a?style=flat-square)
![No dependencies](https://img.shields.io/badge/dependencies-none-2ea44f?style=flat-square)
![Tests](https://img.shields.io/badge/tests-86%20passing-2ea44f?style=flat-square)
![Size](https://img.shields.io/badge/exe-220%20KB-2ea44f?style=flat-square)
![License](https://img.shields.io/badge/license-MIT-1a1a1a?style=flat-square)

<br>

**[Screenshots](#screenshots)** · **[Status](#status)** ·
**[Planned features](#planned-features)** · **[Build](#build)** · **[Tests](#tests)**

</div>

<br>

> **Work in progress.** Capture works end to end — region, window, screen and
> delayed, to the clipboard or a PNG. Annotation, pinning, OCR and history are
> not built yet. See [Status](#status).

<br>

## Screenshots

Hover a window and the frame lights up; click to capture it. The magnifier
follows the cursor with the pixel under it, its coordinates and its colour.

<div align="center">
<img src="docs/screenshots/overlay-window-pick.png" width="820" alt="Window pick with magnifier">
</div>

<br>

Drag instead and you get a live selection with its size. Everything outside the
selection stays dimmed.

<div align="center">
<img src="docs/screenshots/overlay-selection.png" width="820" alt="Region selection">
</div>

<br>

## Why

Windows ships a capable snipping tool, and most free alternatives do the basics
well. Crisp exists for the parts they do not:

- **Pin a capture to the screen.** Compare two things side by side without a
  second monitor or an editor.
- **A real magnifier while selecting.** Pixel-exact selection with live
  coordinates, not a best guess.
- **Text out of an image, offline.** OCR through the engine already in Windows —
  no upload, no account, no extra download.

<br>

## Status

| Area | State |
|---|---|
| Image buffer, capture (region / window / screen) | done, tested |
| Crop, PNG encode and decode | done, tested |
| Clipboard (CF_DIB + PNG) | done, tested |
| Settings (registry + portable `.ini`) | done, tested |
| Window pick and frame bounds | done, tested |
| Selection overlay — dim, magnifier, size readout, square snap | done |
| Tray icon, context menu, global hotkeys | done |
| Delayed capture | done |
| Save as PNG, copy to clipboard | done |
| Pin to screen — drag, wheel zoom, opacity, copy, save as | done |
| OCR — text out of the image, offline | done |
| Colour picker | done |
| Annotation editor | not started |
| Capture history | not started |

98 tests pass. See [Tests](#tests).

### Shortcuts

| | |
|---|---|
| `Ctrl+Shift+S` | Select a region |
| `Ctrl+Shift+W` | Capture the window under the cursor |
| `Ctrl+Shift+F` | Capture the current monitor |
| `Ctrl+Shift+D` | Countdown, then select |

While the overlay is open: drag to select, click to take the highlighted window,
hold `Shift` to lock the selection square, `Enter` to accept, `Esc` or
right-click to cancel.

**Metni kopyala (OCR)** and **Renk seç** are in the tray menu. OCR runs on the
engine already in Windows — nothing is uploaded and nothing extra is installed,
but it needs an OCR-capable language in your language profile. About → tells you
whether this machine has one.

On a pinned window: drag anywhere to move it, wheel to zoom, double-click for
actual size, `Ctrl+C` to copy, `Ctrl+S` to save, `Esc` to close, right-click for
the rest.

<br>

## Planned features

**Capture** — drag a region, pick a window under the cursor, whole screen or one
monitor, and a countdown capture for menus and tooltips that vanish when you
click.

**After capture** — copy to the clipboard, save as PNG, pin the result on screen
as a floating window, or open it in the annotation editor. Any combination.

**Annotation** — arrows and rectangles, text and highlighter, step number
badges, and blur or pixelate for the parts of a screenshot that should not be
shared.

**Extras** — OCR that lifts text out of the image and onto the clipboard, a pixel
magnifier with live coordinates during selection, a colour picker, and a history
of recent captures.

<br>

## Build

Requires Visual Studio 2022 (MSVC v143) or newer with the C++ toolset, Windows
SDK 10.0.22621+, CMake 3.25+ and Ninja. There are no third-party dependencies
and no package manager.

```powershell
tools\build.ps1 -Config Release -Test
```

Output: `build\Release\Crisp.exe`.

Useful switches:

```powershell
tools\build.ps1 -CoreOnly -Test    # core + tests, no user interface
tools\build.ps1 -Clean             # wipe the build directory first
tools\build.ps1 -Package           # portable ZIP via cpack
```

<br>

## Tests

The build is split so that everything without a user interface lives in
`crisp_core`, and a console runner links against it. That is not an accident of
layout — it is what makes the capture pipeline testable at all:

```powershell
build\Release\crisp_tests.exe
```

The runner is 200 lines with no dependencies. Tests cover selection geometry,
the image buffer, screen capture, PNG round-trips, the clipboard and the
settings store.

What the tests deliberately do **not** assert: anything about what is on screen
at the time. A test that expected particular pixels would pass on one machine
and fail on the next.

<br>

## Licence

MIT · © 2026 ShadesOfDeath
