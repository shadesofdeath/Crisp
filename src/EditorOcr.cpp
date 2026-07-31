// EditorOcr.cpp — Düzenleyicideki metin tanıma kipi.
//
// KAPLAMADAKİ METİN SEÇİMİYLE AYNI MANTIK, farklı yüzey: kelimeler
// OcrLayout'tan gelir, seçim kelime ARALIĞIDIR (piksel dikdörtgeni değil) ve
// kopyalanan metin satır yapısını korur. Fark, burada görüntünün
// yakınlaştırılabilir olması: kelime kutuları görüntü koordinatında saklanır
// ve çizim anında ölçeklenir, yoksa yakınlaştırınca kutular yerinde kalırdı.
#include "EditorInternal.h"

#include "ClipboardImage.h"
#include "Geometry.h"
#include "Localization.h"
#include "Ocr.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

#include <string>

namespace crisp {
namespace editor {
namespace {

enum MenuId { kMenuCopy = 1, kMenuCopyAll, kMenuSelectAll, kMenuExit };

[[nodiscard]] std::wstring SelectedText(const State& state) {
    if (!state.ocr.HasSelection()) {
        return std::wstring();
    }
    int lo = 0;
    int hi = 0;
    ocrsel::NormalizeRange(state.ocr.anchor, state.ocr.cursor, lo, hi);
    return ocrsel::TextForRange(state.ocr.layout, lo, hi);
}

void Redraw(HWND window) { ::InvalidateRect(window, nullptr, FALSE); }

}  // namespace

void ToggleOcrMode(HWND window, State& state) {
    if (state.ocr.active) {
        state.ocr.active = false;
        state.ocr.Clear();
        Redraw(window);
        return;
    }

    // TANIMA GÖRÜNTÜ DEĞİŞTİKÇE TAZELENİR: kullanıcı kırpma ya da döndürme
    // yaptıysa eski kelime kutuları başka yerleri gösterirdi. Bu yüzden kip
    // her açılışta yeniden tanır; önbelleğe almak yanlış kutu göstermekten
    // ucuz değil.
    Image copy;
    if (state.image == nullptr ||
        !CropImage(*state.image, 0, 0, state.image->Width(),
                   state.image->Height(), copy)) {
        return;
    }

    const HCURSOR previous = ::SetCursor(::LoadCursorW(nullptr, IDC_WAIT));
    OcrLayout layout;
    const bool recognized = RecognizeLayout(copy, layout);
    ::SetCursor(previous);

    if (!recognized) {
        ::MessageBoxW(window, Loc::Str(IDS_OCR_UNAVAILABLE).c_str(),
                      Loc::Str(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONWARNING);
        return;
    }
    if (layout.empty()) {
        ::MessageBoxW(window, Loc::Str(IDS_OCR_NO_TEXT_REGION).c_str(),
                      Loc::Str(IDS_APP_TITLE).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }

    state.ocr.layout = std::move(layout);
    state.ocr.attempted = true;
    state.ocr.active = true;
    state.ocr.Clear();
    Redraw(window);
}

bool OcrMouseDown(HWND window, State& state, POINT client) {
    if (!state.ocr.active) {
        return false;
    }
    if (!::PtInRect(&state.canvas, client)) {
        return true;   // kip açıkken tuval dışına çizim yapılmaz
    }

    const POINT image = ToImage(state, client);
    // EN YAKIN KELİME, tam isabet değil: kullanıcı kelimeler arasındaki
    // boşluğa tıkladığında hiçbir şey seçilmemesi, metin seçmeye alışkın
    // birine bozuk gelirdi.
    const int word = ocrsel::NearestWord(state.ocr.layout, image);
    if (word < 0) {
        return true;
    }
    state.ocr.anchor = word;
    state.ocr.cursor = word;
    state.ocr.selecting = true;
    ::SetCapture(window);
    Redraw(window);
    return true;
}

bool OcrMouseMove(HWND window, State& state, POINT client) {
    if (!state.ocr.active) {
        return false;
    }

    if (state.ocr.selecting) {
        const POINT image = ToImage(state, client);
        const int word = ocrsel::NearestWord(state.ocr.layout, image);
        if (word >= 0 && word != state.ocr.cursor) {
            state.ocr.cursor = word;
            Redraw(window);
        }
        ::SetCursor(::LoadCursorW(nullptr, IDC_IBEAM));
        return true;
    }

    const bool overCanvas = ::PtInRect(&state.canvas, client) != FALSE;
    ::SetCursor(::LoadCursorW(nullptr, overCanvas ? IDC_IBEAM : IDC_ARROW));
    return true;
}

bool OcrMouseUp(HWND window, State& state) {
    if (!state.ocr.active || !state.ocr.selecting) {
        return state.ocr.active;
    }
    state.ocr.selecting = false;
    if (::GetCapture() == window) {
        ::ReleaseCapture();
    }
    Redraw(window);
    return true;
}

void OcrCopySelection(HWND window, const State& state) {
    const std::wstring text = SelectedText(state);
    if (text.empty()) {
        return;
    }
    if (!CopyTextToClipboard(text.c_str(), window)) {
        LogV(L"Seçilen metin panoya kopyalanamadı");
    }
}

bool OcrKeyDown(HWND window, State& state, unsigned key, bool control) {
    if (!state.ocr.active) {
        return false;
    }
    if (key == VK_ESCAPE) {
        state.ocr.active = false;
        state.ocr.Clear();
        Redraw(window);
        return true;
    }
    if (control && key == 'C') {
        OcrCopySelection(window, state);
        return true;
    }
    if (control && key == 'A') {
        state.ocr.anchor = 0;
        state.ocr.cursor = state.ocr.layout.count() - 1;
        Redraw(window);
        return true;
    }
    return false;
}

void OcrShowMenu(HWND window, State& state, POINT screen) {
    const HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    const bool hasSelection = state.ocr.HasSelection();
    ::AppendMenuW(menu, hasSelection ? MF_STRING : (MF_STRING | MF_GRAYED),
                  kMenuCopy, Loc::Str(IDS_TEXTMENU_COPY).c_str());
    ::AppendMenuW(menu, MF_STRING, kMenuCopyAll,
                  Loc::Str(IDS_TEXTMENU_COPY_ALL).c_str());
    ::AppendMenuW(menu, MF_STRING, kMenuSelectAll,
                  Loc::Str(IDS_TEXTMENU_SELECT_ALL).c_str());
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, kMenuExit,
                  Loc::Str(IDS_TEXTMENU_CANCEL).c_str());

    ::SetForegroundWindow(window);
    const int chosen = ::TrackPopupMenuEx(
        menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, screen.x, screen.y,
        window, nullptr);
    ::DestroyMenu(menu);

    switch (chosen) {
        case kMenuCopy:
            OcrCopySelection(window, state);
            break;
        case kMenuCopyAll: {
            const std::wstring all = ocrsel::AllText(state.ocr.layout);
            if (!all.empty() && !CopyTextToClipboard(all.c_str(), window)) {
                LogV(L"Tüm metin panoya kopyalanamadı");
            }
            break;
        }
        case kMenuSelectAll:
            state.ocr.anchor = 0;
            state.ocr.cursor = state.ocr.layout.count() - 1;
            Redraw(window);
            break;
        case kMenuExit:
            state.ocr.active = false;
            state.ocr.Clear();
            Redraw(window);
            break;
        default:
            break;
    }
}

void DrawOcrOverlay(HDC dc, const State& state) {
    if (!state.ocr.active || geom::IsEmpty(state.canvas)) {
        return;
    }

    const Palette& colors = theme::Colors();
    int selectedFrom = -1;
    int selectedTo = -1;
    if (state.ocr.HasSelection()) {
        ocrsel::NormalizeRange(state.ocr.anchor, state.ocr.cursor, selectedFrom,
                               selectedTo);
    }

    const HPEN outline = ::CreatePen(PS_SOLID, 1, colors.accent);
    const HBRUSH fill = ::CreateSolidBrush(colors.accent);
    if (outline == nullptr || fill == nullptr) {
        if (outline != nullptr) {
            ::DeleteObject(outline);
        }
        if (fill != nullptr) {
            ::DeleteObject(fill);
        }
        return;
    }

    const HGDIOBJ oldPen = ::SelectObject(dc, outline);
    const HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));

    for (int i = 0; i < state.ocr.layout.count(); ++i) {
        const RECT box =
            ToClientRect(state, state.ocr.layout.words[static_cast<size_t>(i)].bounds);
        const bool selected =
            selectedFrom >= 0 && i >= selectedFrom && i <= selectedTo;
        if (selected) {
            // Seçili kelimeler DOLU çizilir. Yalnızca çerçeveyi kalınlaştırmak,
            // bitişik kelimelerde neyin seçili olduğunu okunmaz hâle getirirdi.
            ::SelectObject(dc, fill);
            ::PatBlt(dc, box.left, box.top, static_cast<int>(geom::Width(box)),
                     static_cast<int>(geom::Height(box)), PATINVERT);
            ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
        } else {
            ::Rectangle(dc, box.left, box.top, box.right, box.bottom);
        }
    }

    ::SelectObject(dc, oldBrush);
    ::SelectObject(dc, oldPen);
    ::DeleteObject(outline);
    ::DeleteObject(fill);
}

}  // namespace editor
}  // namespace crisp
