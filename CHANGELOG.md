# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## 0.8.0 — the overlay stops being a one-way door

### Added — an action bar on the settled selection

Once a rectangle settles, a small bar appears beside it: copy, save, edit,
recognise text, pin, upload. Hovering a button names it.

The settings list under *After capture* answers "what should happen to **every**
capture". There was no way to answer "what should happen to **this** one" —
saving a single screenshot meant opening Settings, ticking a box, capturing,
then going back and unticking it. The bar moves that decision to the moment of
capture, and leaves the defaults alone.

The Upload button is absent, not greyed, when no service is chosen: a disabled
button invites a click and then explains nothing.

### Added — scrolling capture

Select a region, and Crisp scrolls the window under it, capturing as it goes,
then joins the frames into one tall image. A chat log or a long page can be
taken in one piece instead of three screenshots stitched by hand.

The joining is the hard part and it lives in `crisp_core`: give it two frames
and it finds how far the second moved, or says it cannot. **It says "cannot"
rather than guessing** — every search has a best candidate, including between
two unrelated images, and treating that as an answer would produce a picture
made of two unrelated strips, silently.

The comparison band is taken from a third of the way down the frame, not the
top: a window title bar or a sticky page header does not move when you scroll,
and comparing a region that never changes only ever answers "it did not move".

Scrolling stops on its own when nothing new appears — the end of the page, or a
window that does not scroll at all. 30 frames is the ceiling, for pages that
scroll forever.

### Added — pinned images survive a restart

Pins were held in memory only. Closing Crisp — or a Windows update — took them
with it, and nothing had been written anywhere, so there was no way back.

They are saved to `%LOCALAPPDATA%\Crisp\Pins` as plain PNGs with a
tab-separated index holding position, zoom and opacity, and restored at startup.
The history folder deliberately has no index; here there is one, because a pin
has a place on screen and a PNG does not record that.

*Being killed from Task Manager still loses them: the save runs on a clean
exit.*

### Added — type a size into the overlay

`Ctrl+C` on the overlay copied the selection's position and size as text. The
reverse now exists: `Ctrl+V` reads a size from the clipboard and applies it.
`1200x630`, `1200 × 630`, `1200, 630` and the exact text `Ctrl+C` produces all
work — the separators are ignored, the numbers are not.

"This has to be exactly 1200×630" previously meant dragging pixel by pixel while
reading the magnifier.

### Added — delayed window and delayed monitor

Delayed capture existed but always meant *region*. The machinery could already
delay anything; only the way to ask for it was missing. The tray menu's
**Delayed** entry is now a submenu of three, and `-delayed-window` /
`-delayed-monitor` work on the command line.

### Changed — the editor

- **The select tool resizes shapes.** It moved, deleted and restyled them but
  did not resize — while drawing four corner handles that did nothing, the same
  broken promise the overlay's handles used to make. One proportional mapping
  handles arrows, rectangles and freehand alike, because all three are
  ultimately lists of coordinates; the eight special cases the old comment
  predicted were never needed. Handles are grabbed and drawn from the same
  function as the overlay's.
- **Blur, mosaic and crop preview live.** They drew a dashed outline and nothing
  else, on the grounds that computing the real effect every mouse-move would be
  slow. It was not measured, and it was wrong: the blur is a two-pass sliding
  window, linear in area, and the preview needs only the selected region at
  screen scale. The cost of the outline was real, though — blur is a redaction
  tool, and not seeing how much it hides until you let go means looking at the
  thing you are trying to hide one more time.
- Crop dims what will be discarded rather than outlining what will be kept.

### Fixed

- The auto-upload progress notification could not animate while the capture loop
  held the message queue; the loop pumps messages now.
- 327 tests, 41 of them new.

## 0.7.1 — read the whole row

### Fixed

- **The two longest rows in the service list were cut off at the edge.** A
  combo box drops its list at the width of the box, and
  `bashupload.app · 1 gün · ücretsiz` and `Freeimage.host · kalıcı · API key`
  did not fit. Widening the box would have disturbed the column it sits in, so
  only the list widens, to the measured width of its longest entry. The point of
  putting the lifetime and the price on every row was that they could be read.

## 0.7.0 — thirteen places to put a screenshot, and a way to install it

### Added — six more upload services

kappa.lol and pone.rs (permanent), qu.ax (30 days), x0.at (3 days and up by
size), temp.sh (3 days) and bashupload.app (24 hours). None of them wants an
account. That makes ten keyless services and three that take a key.

- **The list has a divider in it now.** Thirteen entries where three behave
  differently is a list you have to read twice; the keyless ones come first,
  longest-lived first, then a rule, then the three that want a key. Every row
  also carries its own lifetime and either "free" or the name of the key it
  wants — because with the list closed you only see the row you picked, and a
  divider you cannot see explains nothing.
- **The lifetime is written in days where days make sense.** It was stored in
  hours and printed as `72 sa` — hours, in Turkish, in all sixteen languages.
- Not every service returns a direct image link and the README now says which:
  qu.ax and temp.sh hand back a page, bashupload serves a download. They are
  in the list because they were asked for, not because they are equivalent.

Each of the six was verified by uploading through Crisp's own code, not by
reading documentation — which is how two of the following were found.

### Fixed

- **bashupload was storing the multipart envelope as the file.** It does not
  parse `multipart/form-data`; it treats the request body as the file, so what
  landed on the server was the boundary lines, the headers and the PNG, saved
  as one file — and it answered with a working link the whole time. Nothing
  looked wrong until you clicked it. It gets the raw bytes over `PUT` now, which
  is what its own documentation always said, and a test pins the request shape.
- bashupload also answers with an `http://` address. It is upgraded to `https://`
  before it reaches the clipboard; sending the request over TLS and then handing
  back a plaintext link would have been theatre.
- Without an expiry header bashupload deletes the file after one download — the
  first person you send the link to gets the image and the second gets nothing.
  Crisp asks for the longest window the server allows.

### Added — an installer

`Crisp-x.y.z-setup-x64.exe`, alongside the portable ZIP and containing the same
executable. Start menu entry, optional desktop shortcut, optional start at
sign-in, proper uninstaller, fifteen languages.

- **It asks for no administrator rights.** Crisp writes only to `HKCU` and
  installs one file; with no elevation available it installs under your own
  profile instead of demanding a UAC prompt for nothing.
- **Uninstalling leaves your settings alone.** Shortcuts, the save folder and
  the API key stay in `HKCU` so a reinstall finds them.
- **It closes a running Crisp first.** Restart Manager could not: it asks
  *windows* to close and Crisp is a tray application whose only window is
  hidden, so an uninstall while Crisp was running removed the shortcuts, left
  the executable behind, and reported success. The installer now asks the window
  class to close, which is the same clean shutdown as quitting from the tray.

## 0.6.1 — say that it is still working

### Added

- **A notification while an upload is in flight.** Some services take fifteen to
  twenty seconds, and for all of that time the screen said nothing: the capture
  notification faded and then there was silence. Work in progress and work
  forgotten looked identical. This one does not fade — that is the point of it —
  and it counts the seconds, so a slow upload is visibly different from a stuck
  one. The result notification takes its place; if the result never comes, it
  gives up after two minutes rather than sitting there forever.

  While it is up, the capture notification stays out of the way. It runs first
  and would otherwise destroy the progress box the moment it was created.

### Fixed

- **The shutter-sound test asserted that the machine had a sound card.**
  `PlaySound` returns false with no output device, which says nothing about the
  bytes being correct — the test's own comment had said so and asserted anyway.
  The build server has no audio, so the suite went red on a commit that changed
  nothing about sound. It skips where there is no device and runs everywhere
  else.

## 0.6.0 — the selection you can actually adjust

### Changed — the selection overlay

- **Releasing the mouse no longer captures.** The rectangle settles and stays
  there: drag a handle to resize it, drag inside it to move it, press **Enter**
  or double-click inside to capture. The eight handles were already being drawn
  and had never done anything — the interface was making a promise the code did
  not keep, and a screenshot missed by three pixels meant starting the drag over.

  Small selections shed their handles rather than letting them overlap: below
  five handle-widths a side the edge handles go, below three the corners go too
  and the whole rectangle becomes a move target. Handles are grabbed over a wider
  area than they are drawn — a seven-pixel square is not a mouse target.

- **Once settled, the monitor keys reshape the selection** instead of capturing
  immediately. `Enter` stays the single commit.

- Clicking a window still captures it, but only before a selection settles. With
  one on screen a stray click outside it means "start again", not "grab whatever
  window happens to be under the pointer".

- The crosshair, the magnifier and the window highlight all switch off once a
  rectangle exists; aiming is over. The crosshair and magnifier come back while a
  handle is being dragged, which is aiming again.

### Fixed

- **The auto-upload notification did not appear, and the link never reached the
  clipboard.** Both were done on the upload thread. A window created there gets
  no message loop, so its fade-in timer never ran and it stayed fully
  transparent; `DestroyWindow` on the capture notification failed silently
  because that window belonged to another thread, leaving two notifications
  sharing one piece of state. What the user saw was a notification that appeared
  out of nowhere when the pointer crossed it. `OpenClipboard` was failing for the
  same reason — it wants a window owned by the calling thread — so the "link
  copied" message was, on the occasions it showed at all, not true.

  The thread now waits on the network and nothing else, and posts its result to
  the window. Closing a notification reports failure to the log instead of
  swallowing it, and a notification that cannot replace the previous one is not
  opened on top of it.

- **Half the overlay's keys were undocumented.** `Space`, `1`–`9`, `0` and
  `Ctrl+C` all worked and none of them appeared anywhere but the README — the
  hint listed four mouse gestures and stopped. It is two lines now, mouse above
  and keyboard below, in all sixteen languages, and it wraps rather than running
  off a narrow screen. The settled selection gets its own second line, since
  there the monitor keys reshape instead of capturing.

- **Upload errors were Turkish in all sixteen languages.** The sentences were
  written into `crisp_core`, which cannot call the localiser — the core must not
  depend on the UI layer. The core reports a code now and the app layer
  translates it, leaving the status number and the host name as the only
  untranslated parts, which is correct: they read the same in every language.

- Settings hints were a fixed three lines tall and the longer strings were
  clipped against the window edge. They are measured now, so no translation can
  overflow them.

### Added

- **Recent links.** Upload put a link on the clipboard and left it there; the
  next copy erased it. The tray menu keeps the last ten now, and clicking one
  copies it again. Stored as plain tab-separated text in
  `%LOCALAPPDATA%\Crisp\uploads.txt` — a line you can delete by hand, not a
  database. The image history stays what it was: a plain folder of PNGs with no
  index to keep in step.

- **Continuous integration.** 269 tests existed and nothing ran them. A Windows
  workflow builds, tests and packages on every push. 284 now.

### Known limitations

- Alt+Tab away from the overlay still cancels a settled selection outright.
  Suppressing that risks a full-screen topmost window that has lost keyboard
  focus, so `Esc` can no longer reach it; preserving the rectangle across the
  cancel needs state that outlives the overlay, which is its own change.
- The size label can cover the top or bottom handles. They stay grabbable — the
  hit test reads the selection, not the painter — so this costs a visual cue and
  nothing else.

## 0.5.0 — the shortcut you actually wanted

Three things a user reported: two fixed here, the third half-built and honest
about it.

### Changed — shortcuts

- **Any key can be a shortcut on its own now.** `Ctrl`, `Alt` and `Shift` are no
  longer required. The old rule was well meant — a letter bound by itself is
  taken away from every text box on the system — but it enforced that judgement
  on the user's behalf, and *silently*: the shortcut box refused the key with a
  beep and no explanation, and a modifier-less shortcut already in the settings
  file was deleted on load. Someone who wanted one key for one screenshot found
  their choice quietly undone at the next start.

  The cost has not gone away, it moved to where it can be read. The hint under
  the shortcut list now says which keys are taken from typing, in all sixteen
  languages, and `HotkeyNeedsModifier` — a rule — became
  `HotkeyTypesCharacters` — a fact. It no longer refuses anything.

- **`Insert`, `Home`, `End`, `Page Up`/`Page Down`, the arrows and the menu key**
  join the list of keys with nothing to lose. None of them types a character, and
  on a keyboard without a numeric pad they are exactly the spare keys left to
  bind.

- **`Win`+letter is not deleted either.** Windows reserves *most* of those
  combinations, not all, and the only component that knows which are free is
  `RegisterHotKey`. The ones Windows owns were already reported as "could not be
  registered"; deleting them beforehand withheld that explanation.

### Fixed — multiple monitors

- **The overlay hint no longer straddles two screens.** It was centred on a
  rectangle named `monitor` that actually held the *virtual screen* — every
  display joined together. On one monitor the two are the same thing and the bug
  is invisible; on two, the horizontal centre of the virtual screen is precisely
  the seam between the displays, so the box was cut in half down the middle. It
  is now centred on the monitor the pointer is on.

- **The magnifier no longer jumps to the other screen.** Same root cause, quieter
  symptom: the panel flips to the other side of the cursor when it runs out of
  room, and it was measuring that room against the whole desktop. With the
  pointer near the right edge of the first monitor there was "room", so the
  magnifier appeared on the second one, away from where the user was looking.
  What it magnifies still comes from the whole desktop, which is correct; only
  the placement changed.

### Added — upload

- **Send a capture to an image host and get a link back.** Seven services, four
  of which need no account at all: Catbox (permanent), Litterbox (72 h), Uguu
  (3 h), 0x0.st, plus Imgur, ImgBB and Freeimage.host, which take an API key you
  paste into Settings. The key box is disabled until the chosen service needs
  one, and each service's lifetime and key requirement is written next to its
  name in the list.

- **Two ways to trigger it, both off until you ask.** The editor gains an Upload
  button, disabled while no service is chosen. Or tick **Upload and copy the
  link** under *After capture* and every capture goes up on its own, with the
  link on the clipboard and a notification when it lands — not when the capture
  was taken, so a failed upload cannot be mistaken for a link you can paste.

  This is the first network code Crisp has had. "No upload, no account" survives
  as the default rather than as a rule: the setting ships as *Do not upload*, and
  with no service chosen the program never opens a socket.

- **The transport is deliberately boring.** Everything is HTTPS, certificate
  validation is not relaxed anywhere, and the API key travels in the header for
  Imgur and in the body for ImgBB — never in a query string, where it would end
  up in redirects and server logs. Both are pinned by tests.

  21 tests cover request building and response parsing without touching the
  network, including the one that matters: ImgBB returns `url` three times in one
  response — `data.url`, `data.display_url` and `data.thumb.url` — and a naive
  search for the first one hands back the thumbnail.

  Verified against the keyless services from a real machine: Catbox and Uguu
  return links that download back as the same PNG. Litterbox accepts the upload
  but its host was unreachable from the test network, and 0x0.st answered 503 to
  the POST while serving GET normally — `curl` reproduced both identically, so
  those are the services refusing, not the code.

## 0.4.0 — the gaps ShareX fills

A feature-by-feature comparison against ShareX, then the parts worth having
that this tool did not. Not everything ShareX does: uploading, video and the
utilities unrelated to screen capture stay out, because "no upload, no account,
one 600 KB exe" is the product.

### Added — capture

- **Last region.** Re-captures the exact rectangle you dragged last time,
  without opening the overlay. Only *dragged* regions are remembered: after
  clicking a window you do not expect "last region" to repeat where that window
  happened to be, since it may have moved.
- **Active window.** Takes the foreground window directly, no overlay. The
  window is read *before* anything of ours can take focus, and the desktop and
  shell classes are rejected — otherwise "active window" would hand back an
  empty wallpaper.
- **All monitors.** The whole virtual desktop in one shot. What used to be
  called "full screen" only ever captured the monitor under the cursor; both are
  now separate, correctly named actions.
- **Capture the cursor**, off by default. `BitBlt` does not include it —
  `CAPTUREBLT` adds layered windows, not the pointer — so the icon is queried
  and drawn with its hotspot subtracted; without that correction the arrow lands
  a few pixels below what it is pointing at.
- **Delay applies to every mode**, not just region, with an on-screen countdown
  in the notification window. Waiting for a menu to open before capturing a
  window was previously impossible.
- **Overlay: crosshair, `Ctrl+C` for coordinates, monitor keys.** Two thin lines
  across the screen for aligning an edge with something far away; `Ctrl+C`
  copies `x, y  w × h` as text; `Space` takes the monitor under the cursor,
  `1`–`9` a numbered monitor (left to right), `0` everything. All of it reads
  from the already-frozen screen — no second capture.
- **Dimming strength is a setting** (0–80 %). One fixed value cannot suit both a
  dark and a light desktop.

### Added — after capture

- Four more tasks: **copy the file path**, **copy the file itself** (`CF_HDROP`,
  so it pastes into Explorer or a chat as an attachment), **show in the folder**,
  and **recognise the text and copy it**.
- **File name and subfolder templates** with `%y %mo %d %h %mi %s %px %py %pn
  %i %ra %un %cn`. The name used to be hard-coded and everything landed in one
  folder; after a month that folder tells you nothing. The expander is pure
  string work in the core with its own tests, and `..` is stripped from
  subfolder templates so a template cannot write outside the save folder.

### Added — hotkeys and launching

- **An action per hotkey**, chosen from a list of twelve, across six slots. The
  mapping used to be fixed at compile time: OCR, screen text, the colour picker,
  history and last region could not have a hotkey at all. Existing bindings
  migrate on first run.
- **Command line**: `Crisp.exe -region | -window | -active | -monitor | -all |
  -last | -delayed | -text | -ocr | -color | -history`, and `Crisp.exe foto.png`
  to open a file in the editor. A second instance forwards the request to the
  running one. The command line was not read at all before.
- **Edit the clipboard image** from the tray, and **drop an image file onto the
  editor** to open it. `ReadImageFromClipboard` had existed for a year with only
  the tests using it.

### Added — editor

- **Selection tool** (`V`): click a shape to select it, drag to move it, `Delete`
  to remove it, and the colour/thickness/fill controls restyle it. Until now a
  misplaced arrow could only be undone — taking everything drawn after it with
  it. Hit-testing measures distance *to the stroke*, not to the bounding box, so
  a thin diagonal arrow is not a screen-sized target.
- **Line tool** (`2`), **shape fill** toggle, and **`Shift` locks** rectangles to
  squares, ellipses to circles and lines to 45°. Snapping rotates rather than
  projects: holding `Shift` should fix the angle, not shorten the line.
- **Effects menu**: flip horizontally/vertically, trim the borders, add a
  margin, greyscale, invert, sepia, sharpen, and ± brightness, contrast and
  saturation. One button rather than fourteen — none of them is used more than
  once per session, while the arrow is used twenty times.
- **The highlighter is really translucent now.** It was faked with `R2_MASKPEN`,
  a bitwise AND that does not blend colours but extinguishes them: yellow over
  blue came out black. The stroke is now drawn opaque on a copy and blended
  back.
- **Blur and mosaic strength** are settings, recorded *on the shape* so undo
  replays what you actually drew rather than the current setting.

### Changed

- Tool shortcuts shift by one: `1` arrow, `2` line, `3` rectangle, `4` ellipse,
  `5` pen, `6` highlighter, `7` text, `8` step, `9` blur, `0` mosaic, plus `V`
  select and `C` crop. Twelve tools do not fit ten digits.
- The settings window is three columns; the label column is wider because
  "Bulanıklık (%)" was sliding under its own edit box.
- 27 new interface strings, translated into all 16 languages.
- The shortcut list carries a line stating the rule ("Ctrl, Alt or Shift is
  needed; `F1`–`F12` and `Print Screen` also work on their own"). A rule that is
  only enforced, never stated, is discovered by having it applied to you.

### Added — Explorer integration

- **"Edit with Crisp" in the right-click menu**, switched on from Settings. Not
  a shell extension: an `IExplorerCommand` handler is a COM DLL that Explorer
  loads *into its own process*, where a crash takes the file manager down with
  it. A registry verb is four values, and the exe already accepts a file path
  on the command line — the whole feature is registration plus a checkbox.
  - Registered under `HKEY_CURRENT_USER`, so it needs no administrator and one
    user's choice does not affect another's.
  - One key covers every image format: `SystemFileAssociations\image` is
    Windows' own *perceived type*, so PNG, JPEG, BMP, GIF, TIFF, WebP and HEIF
    all come along. Registering extension by extension would have made keeping
    that list in sync with Windows our problem.
  - **The registration is the setting.** There is no copy of the flag in our own
    store: a second source of truth would disagree the moment the user deleted
    the key by hand. It is read back from the verb itself on startup, and if the
    exe has been moved the stale command is rewritten.
  - Launched *only* to edit a file, Crisp **exits when the editor closes**.
    Someone who right-clicked a picture did not ask to start a screenshot tool,
    and leaving a tray icon and four global hotkeys behind would be a surprise.
    Requests forwarded to an already-running instance do not do this.
  - **Windows 11 note:** third-party verbs live in the classic menu — *Show more
    options*, or `Shift+F10`. Appearing in the short menu requires a packaged
    (MSIX) shell extension, which a single portable exe cannot have.

### Fixed after the first pass

- **"Active window" never fired from the tray menu.** `TrackPopupMenuEx` cannot
  show a menu unless the owner is brought to the foreground first — a
  documented shell requirement — so by the time the command ran, the foreground
  window *was* Crisp's own invisible message window and the code bailed out
  every single time. It now falls back to walking the Z order for the topmost
  Alt-Tab candidate (visible, uncloaked, not ours, not the shell, not a tool
  window, has a title), and activates the target before capturing: `BitBlt`
  copies what is *on screen*, so a window that is no longer in front would come
  back with whatever is covering it.
- **The select tool's icon was a blob.** The cursor polygon was being stroked
  with the 2 px tool pen, which rounded off the tip and swallowed the tail
  notch on an 11 px arrow. It is filled with an explicit `NULL_PEN` now.
- **Rebound hotkeys came back empty.** Reported by a user on a tenkeyless
  keyboard, and it had three separate causes, each of which silently threw the
  binding away after *OK*:
  - **A single key was accepted by the box and deleted by validation.** `F9` or
    `Print Screen` on its own showed up in the field, and `Settings::Clamp` —
    which requires `Ctrl`/`Alt`/`Shift` so that a bare letter cannot steal
    typing system-wide — wiped it on the way out. Keys that produce no text
    (`F1`–`F24`, `Print Screen`, `Pause`, `Scroll Lock`, the media keys) are now
    valid on their own; that is exactly the set a keyboard without a numpad has
    to spare. Anything else is refused *when it is pressed*, with a beep and the
    previous value left intact, instead of vanishing later.
  - **A combination already registered as a global hotkey never reached the
    box.** Windows delivers it straight to whoever owns it, so the field did not
    move — and if the old value had been cleared with `Backspace` first, it
    stayed empty. Crisp now suspends its own hotkeys while Settings is open and
    the field installs a low-level keyboard hook while focused, which runs
    *before* hotkey dispatch: any combination can be typed, including ones
    another application holds.
  - **A key in a slot whose action was still "None" was erased.** Such a slot is
    never handed to `RegisterHotKey`, so it steals nothing from anyone; deleting
    the key only destroyed what the user had just typed.
- **`Print Screen` and the media keys were mislabelled.** `GetKeyNameText` goes
  by scan code and returned "Sys Req" for Print Screen, the letter sitting at
  that position ("G", "B") for the media keys, and nothing at all for `Pause`
  and `F13`+. They have fixed names now — layout-independent keys, unlike
  letters. A slot bound to bare `Print Screen` also takes precedence over the
  "Print Screen captures a region" checkbox rather than losing the registration
  race to it.

### Not done

Speech balloons, spotlight, magnifier box and cursor stamps; the ruler and image
combiner; per-hotkey after-capture profiles; gradient/rounded/shadow beautify
beyond the plain margin. Screen recording, uploading and the unrelated utilities
are deliberately out of scope.

241 tests pass, warning-free at `/W4`, 621 KB.

## 0.3.0 — the editor

Seven complaints from a real session, all about the annotation editor. Each one
is answered below with what was actually wrong, not with what was added.

### Fixed

- **The text tool looked broken.** Picking it and clicking the canvas produced
  *nothing on screen* — no caret, no box, no placeholder. The mode was open, it
  just could not be seen, so the only reasonable conclusion was that the button
  did not work. Typing now starts inside a dashed accent box with a blinking
  caret (at the system blink rate) and a dimmed **"Yazmaya başlayın"** while it
  is empty. `Enter` breaks a line instead of ending the text, `Ctrl+Enter` ends
  it, `Esc` cancels only the text. Surrogate pairs delete as one character, so
  backspacing an emoji no longer corrupts the string, and the IME candidate
  window follows the caret instead of sitting in the window corner.
- **Shrinking to 25 % and back to 100 % destroyed the image.** Two separate
  causes. First, every resize resampled with bilinear interpolation, which
  reads a 2 × 2 neighbourhood no matter the ratio — at quarter size twelve of
  every sixteen source pixels were never read at all. Downscaling is now an
  **area average** and upscaling a **Catmull-Rom cubic**, chosen per axis.
  Second, the second resize was scaling the *already shrunk* image; no filter
  can recover discarded pixels. Consecutive resizes now work from the pixels
  held before the first one, so 25 % → 100 % is **bit-for-bit the original**
  (verified: 75 396 sampled pixels, zero differences).
- **Copy and save slammed the window shut with no feedback.** Both merely set a
  flag and destroyed the window; the real work happened in the caller and the
  editor's only signal was disappearing. Worse, on the *history → edit* path
  nothing was reported at all. The editor now does the work itself, **stays
  open**, and shows a short confirmation in the status bar (`Panoya kopyalandı`,
  `Kaydedildi — <dosya adı>`). The history path finally raises the same
  notification as a fresh capture.

### Added

- **Colour picker panel** replacing the seven fixed swatches. Saturation-value
  square, hue strip, hex field that accepts `#1e90ff` and `#0f0`, 36 presets in
  three tones, recently used colours, and an eyedropper that reuses the existing
  screen-pick overlay. Not `ChooseColorW`: that dialog ignores dark mode, shows
  a 1990s custom-colour grid and has no hex field. The panel holds the mouse
  capture while open, so a click outside dismisses it **without** reaching the
  canvas underneath — otherwise closing the panel drew an arrow.
- **Thickness dropdown** replacing three buttons whose only difference was the
  diameter of a dot; 2 px and 4 px were indistinguishable at 38 px. Seven widths
  (1–18) each drawn as a line of that width, in the selected colour. When the
  colour cannot be told apart from the row background — black ink on the dark
  theme — it falls back to a contrasting one, decided by luminance difference
  rather than a dark/light guess.
- **Save as…** (`Ctrl+Shift+S`), with PNG, JPEG and — only when this machine has
  the encoder — WebP. The format follows the **extension typed**, not the filter
  dropdown, so `note.jpg` saved under a PNG filter is still a JPEG.
- **Grouped toolbar** with a label under each group (Araçlar · Biçim · Resim ·
  Düzen · Dosya) and dividers only where a real gap exists. Dropdown buttons
  carry a chevron so they are distinguishable from tools that act immediately.
- **Zoom slider in the status bar**, logarithmic between 10 % and 800 % — linear
  would squeeze every useful zoom into the first eighth of the track.
- The resize menu shows the **resulting pixel size** next to each percentage and
  ticks the current one.

### Changed

- `EditorResult` reports what happened (`copied`, `savedPath`) instead of what
  the caller should do. Saving the capture path is built once in
  `BuildCapturePath` rather than copied into both the capture flow and the
  editor.
- Eleven new interface strings, translated into all 16 languages.

208 tests pass, warning-free at `/W4`, 552 KB.

## 0.2.0

### Added

- **Pin to screen.** A capture can stay on top as a floating window: drag from
  anywhere on it to move, wheel to zoom, double-click for actual size, `Ctrl+C`
  to copy, `Ctrl+S` to save, `Esc` to close. The pin opens **where the capture
  was taken**, not in the middle of the screen, so the result appears where the
  eye already is. Zoom is anchored to the cursor — the pixel under the pointer
  stays put — because growing from the top-left corner loses whatever the user
  was looking at.
- **OCR.** Lifts text out of a selected region and puts it on the clipboard,
  using the engine already in Windows (`Windows.Media.Ocr`) through the raw
  WinRT ABI. No third-party library, no language data to download, nothing
  uploaded. If the user's language profile has no OCR-capable language the
  engine cannot be created; that is reported as the configuration problem it is
  rather than as a failure. Recognising nothing is a **success** with an empty
  result — the clipboard is left alone instead of being overwritten with an
  empty string.
- **Colour picker.** Click any pixel on screen and its hex value goes to the
  clipboard. It reuses the capture overlay in a second mode rather than
  duplicating the frozen screen, the magnifier and the pixel readout; the
  magnifier is forced on in this mode even if the user turned it off, since
  without it there is no way to see which pixel is being taken.

### Changed

- `RunSelectionOverlay` takes an `OverlayMode` instead of a bool. Region and
  colour picking share the frozen screenshot, the dimming and the magnifier —
  only the click handler and the hint text differ.
- The About box reports whether OCR is available on this machine, so the answer
  is visible before the feature is tried rather than after it fails.

### Verified

Every feature was exercised against the running application, not inspected:

- Colour picker: sampled a screen pixel independently, ran the picker over the
  same point, and compared — `#272727` both times.
- OCR: selected a region over real UI text and got `Dosya Düzenle Görünüm` on
  the clipboard, Turkish diacritics intact.
- Pin: a 700 × 300 drag produced a pin at exactly the capture origin at
  700 × 300; two wheel notches grew it to 1022 × 438.

98 tests pass, warning-free at `/W4`, 239 KB.

## 0.1.0

### Added

- Capture core: 32-bit top-down image buffer, region / window / monitor capture,
  crop, PNG encode and decode through WIC, clipboard in both `CF_DIB` and PNG,
  settings in the registry or a portable `.ini`, and the selection geometry.
- Selection overlay: the screen is frozen first, then drawn — the magnifier can
  read the pixels underneath, nothing shifts between seeing and clicking, and a
  repaint is one `BitBlt`. Dimmed desktop, bright selection and hovered window,
  live size readout, 21 × 21 magnifier at 7×, `Shift` to lock square.
- Tray icon with light and dark artwork, context menu, four global hotkeys,
  delayed capture, single-instance lock.
- A dependency-free test runner, split from the application so that everything
  without a user interface can be exercised directly.

### Fixed

- `BitBlt` leaves the alpha byte undefined on a 32 bpp target; encoded to PNG
  that produced a fully transparent image.
- WIC decodes a truncated PNG without complaining and returns a half image with
  undefined rows. `DecodePng` now checks the signature and the trailing `IEND`
  chunk first, so a caller falls back to `CF_DIB` instead of accepting garbage.
- The test runner did not declare the DPI awareness the application declares, so
  `GetWindowRect` was virtualised while DWM reported physical pixels — on a
  150 % display the two disagreed by half again, and the tests were exercising
  an imitation of the conditions the product runs in.
