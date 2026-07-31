# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
