# Crisp

Native Win32 screen capture for Windows: region, window, active window, monitor
and full virtual desktop, with a pixel magnifier, a twelve-tool annotation
editor, OCR through the Windows engine, pin-to-screen and a local history. One
statically linked executable, no third-party libraries.

Copy to clipboard is the **only** default output action. Save to file, open
editor, pin to screen, reveal in folder, copy path, copy file, OCR to clipboard
and upload are all off until you turn them on.

## Get it

Two downloads on the [releases page](https://github.com/shadesofdeath/Crisp/releases),
both the same executable:

- **`Crisp-x.y.z-portable-x64.zip`** — unzip and run. Writes nothing outside
  `HKCU`, or nothing at all if you drop an empty `Crisp.ini` next to it.
- **`Crisp-x.y.z-setup-x64.exe`** — Start menu entry, optional desktop shortcut,
  optional start-at-sign-in, and an uninstaller. Asks for no administrator
  rights: with none it installs under your own profile. Uninstalling removes
  what it installed and leaves your settings alone.

## Region

`Ctrl+Shift+S`. The overlay freezes the whole virtual desktop and paints that
frozen bitmap, so nothing can move underneath you mid-selection and the
magnifier reads real pixels.

- Drag to select. Hold **Shift** to snap to a square — it snaps on and off
  mid-drag.
- **Enter** accepts, 6 px minimum per side. **Esc**, right-click or middle-click
  cancels.
- A click too short to be a drag captures the window under the cursor instead.
- The magnifier — 21×21 pixels at 7×, with coordinates, hex colour and a swatch —
  is on by default.
- **Releasing does not capture.** The rectangle settles and stays adjustable:
  drag a handle to resize, drag inside it to move, **Enter** or a double-click
  inside to capture. Small selections drop to corner handles only, and smaller
  ones to none at all, so the handles never crowd out the room to move.

Four more keys, all of them named in the on-screen hint:

| Key | Does |
| --- | --- |
| `Space` | monitor under the cursor |
| `1`–`9` | Nth monitor, ordered left to right rather than in driver order |
| `0` | the entire virtual desktop |
| `Ctrl+C` | copy the selection's position and size as text, taking no screenshot at all |

Once a selection is settled the monitor keys reshape it instead of capturing;
`Enter` remains the one commit.

## Other captures

- **Window** — the same overlay with hover highlighting forced on. Capture uses
  the DWM extended frame bounds, so no strip of desktop comes along.
- **Active window** — no overlay at all: raises the foreground window, waits
  220 ms, captures it.
- **Monitor** under the cursor, or **all monitors** in one blit.
- **Last region** — replays the last dragged rectangle, clamped to the current
  layout, and silently does nothing if none was stored. Click-picked windows are
  never stored.
- **Delayed** — 3 s by default, 1–30, and region capture only; there is no
  delayed window or monitor variant. With notifications off the countdown is
  invisible, though the wait still happens.

Every path is a GDI `BitBlt` of what is genuinely on screen — no DXGI, no
`PrintWindow`, which returns black frames for hardware-accelerated content. The
cursor is drawn in by hand and is off by default.

Clipboard writes `CF_DIB` and a registered `PNG` format, so alpha-aware
applications get real alpha.

## Editor

Off by default. Twelve tools — select, arrow, line, rectangle, ellipse, pen,
highlighter, text, step number, blur, mosaic, crop. `1`–`9` and `0` pick them,
`V` is select, `C` is crop. Shift snaps rectangle to square, ellipse to circle,
line and arrow to 45 degrees.

- Blur is a box blur — redaction wants irreversibility, not elegance — and
  mosaic averages each tile's own pixels. Both scale with the selection.
- Rotation is quarter turns only, and lossless. Scale is a fixed
  25/50/75/100/150/200 menu that always resamples from the preserved original,
  so 25 % and back to 100 % returns real pixels.
- Effects: flip horizontal and vertical, auto-crop, padding, grayscale, invert,
  sepia, sharpen, and brightness, contrast and saturation in fixed steps.
- Undo and redo, up to 64 document snapshots. The select tool moves, deletes and
  restyles a shape; it does not resize one.
- The text tool appends only: no caret navigation, no selection. There is no
  live preview while dragging blur, mosaic or crop either, just a dashed outline.
- Crop, rotate, scale and every effect bake the annotations into pixels and clear
  the shape list. One undo reverses both.
- Drop an image file on the window to edit it without capturing anything first.

Files are written as PNG, JPEG or WebP through Windows Imaging Component. WebP
needs the optional Windows encoder component; without it the option is hidden,
and a configured WebP quietly saves as PNG rather than losing the capture.

## OCR

Select text straight off the screen, OCR a region, or open OCR mode inside the
editor with drag word-range selection, `Ctrl+A` and `Ctrl+C`.

Recognition is the Windows engine, `Windows.Media.Ocr`. No Tesseract, no bundled
language data — **it works only if your Windows profile languages include an
OCR-capable language pack**. When they do not, the About window says so and the
mode declines to open. Word boxes are re-sorted into reading order by vertical
overlap, because the engine's own line grouping fragments things like code.
Post-capture OCR is off by default.

## Pin

Puts a capture in a topmost layered window: drag anywhere on it, wheel to zoom,
double-click for actual size, right-click for copy, save as, opacity and close.
Nothing is written to disk, its Save As is PNG-only regardless of your chosen
format, and pins do not survive app exit. Off by default.

## History

A plain folder of PNGs in `%LOCALAPPDATA%\Crisp\History` — no index, no database,
ordered by file timestamp — so deleting one in Explorer simply removes it.
Twenty-four entries by default, 0–200, and 0 disables recording entirely. Always
PNG whatever your save format is, and no annotation or source-window data is
kept. Editing an entry creates a new one and leaves the original untouched. In
the window: double-click or Enter to edit, `Ctrl+C` to copy, right-click to
reveal, delete or clear all.

## Upload

Off twice over: the service defaults to none, and the "upload and copy the link"
after-capture option defaults to off. Nothing leaves the machine until you pick a
service yourself. **Tick that after-capture option and every capture uploads on
its own** — the one thing here that can surprise you after you have opted in,
which is why it sits last in a list of nine where the other eight keep the image
on your machine.

Every service in the list is one somebody else runs. Read what you are sending
before you send it.

Thirteen services. The list is split in two, with a divider, and every entry
says how long its links live and what it costs you:

| No account | Link lives |
| --- | --- |
| Catbox, kappa.lol, pone.rs | permanent |
| 0x0.st, qu.ax | 30 days |
| Litterbox, x0.at, temp.sh | 3 days |
| bashupload.app | 24 hours |
| Uguu | 3 hours |

| Your own key | |
| --- | --- |
| Imgur | Client ID |
| ImgBB, Freeimage.host | API key |

- Without a key the request is never built; there is no anonymous fallback.
- Most services hand back a direct image link. **qu.ax and temp.sh return a
  page** with the image on it, not the image itself — fine for sending to a
  person, no good as an `<img src>`. **bashupload.app** serves its files as
  downloads rather than displaying them, and 24 hours is its ceiling.
- The returned link goes to the clipboard. No browser is opened.
- HTTPS only, over WinHTTP, with no certificate checks relaxed. bashupload
  answers with an `http://` address; it is upgraded before it reaches you.
- The API key is stored in plain text in the registry or the ini file. It is
  masked on screen and nowhere else.
- The list is closed — no custom or self-hosted endpoint.
- Recent links live in the tray menu: the last ten, click one to copy it again.
- Slow services take fifteen to twenty seconds. A notification stays up for the
  whole wait, counting the seconds, and the result replaces it.

Nothing else here touches the network: no update check, no telemetry, no crash
reporting.

## Keys, tray and command line

Six global hotkey slots, and any slot can be bound to any of the eleven actions.
Four ship bound: `Ctrl+Shift+S` region, `Ctrl+Shift+F` monitor, `Ctrl+Shift+W`
window, `Ctrl+Shift+D` delayed. Two ship empty.

- Modifiers are optional — a bare `F9` is a legal hotkey. Bind a letter or a
  digit on its own and you lose that key everywhere else; the settings window
  says which keys those are.
- Print Screen is a separate always-region toggle, on by default. A slot that
  claims Print Screen wins over it, and Windows' own "use PrtScn to open
  Snipping Tool" setting can take the key first — logged as a conflict, not an
  error.
- Backspace or Delete clears a slot. A slot set to no action keeps its key but
  never registers it, leaving the combination free for other applications.

Active window, all monitors, last region, text select, region OCR, colour picker
and history have no default hotkey. All of them are in the tray menu, and all of
them are on the command line:

```
Crisp.exe -region
```

`region`, `window`, `active`, `monitor` (or `fullscreen`), `all`, `last`,
`delayed`, `text`, `ocr`, `color`, `history`. A `/` prefix works as well as `-`.
An unknown or missing argument means region capture; a bare path opens that image
in the editor.

Settings live in `HKCU\Software\ShadesOfDeath\Crisp`. Put a `Crisp.ini` next to
the executable — it may be empty — and Crisp uses that file instead; its
existence is the whole of portable mode. Missing or tampered values fall back to
defaults per key rather than wholesale, and if you manage to turn every output
action off, copy-to-clipboard is forced back on. The interface is translated into
16 languages.

## Build

MSVC only — CMake hard-fails on any other compiler. C++20, CMake 3.25 or newer,
the Windows SDK, no external dependencies and no package manager step. The
application manifest declares Windows 10/11.

```
tools\build.ps1 -Config Release -Test -Package -Installer
```

`-Installer` needs Inno Setup (`winget install JRSoftware.InnoSetup`); everything
else needs nothing but MSVC. Or by hand:

```
cmake -S . -B build\Release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build\Release
```

Output is `build\Release\Crisp.exe`, linked against the static CRT so there is
nothing to install beside it. `-CoreOnly` builds the non-UI library and the tests
without the interface layer; `-Package` produces the portable ZIP. There is no
installer and no MSIX manifest.

284 tests run as a single CTest entry against `crisp_core`, the static library
holding everything that never creates a window — which is what makes the capture
pipeline testable at all. A GitHub Actions workflow builds, tests and packages on
every push. No test asserts anything about what is on screen at the time: one
that expected particular pixels would pass on one machine and fail on the next.

## Licence

MIT. See [LICENSE](LICENSE).
