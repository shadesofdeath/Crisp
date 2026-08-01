// EditorText.cpp — Metin aracının yazma kipi: kutu, imleç, ipucu ve tuşlar.
//
// NEDEN AYRI BİR DOSYA VE NEDEN BU KADAR İŞ: metin aracına basıp tuvale
// tıklandığında EKRANDA HİÇBİR ŞEY OLMUYORDU. Ne bir imleç, ne bir çerçeve,
// ne bir ipucu — kip açılıyordu ama görünmüyordu. Kullanıcının makul çıkarımı
// "bu düğme bozuk" oluyordu; nitekim tam olarak bu bildirildi.
//
// Yazılan metnin önizlemesini çizmek tek başına yetmez: ilk harf yazılana
// kadar önizlenecek bir şey yoktur ve boşluk tam da o an doğar.
#include "EditorInternal.h"

#include "EditorRender.h"
#include "Geometry.h"
#include "Localization.h"
#include "Theme.h"
#include "resource.h"

#include <imm.h>

#include <algorithm>
#include <string>

namespace crisp {
namespace editor {
namespace {

// İmleç yanıp sönme aralığı. Windows'un kendi değeri GetCaretBlinkTime ile
// okunur; kullanıcı onu sistem ayarlarından değiştirdiyse buna da uymalı.
[[nodiscard]] UINT CaretPeriod() noexcept {
    const UINT blink = ::GetCaretBlinkTime();
    // 0 ve INFINITE "yanıp sönme" demek değildir; ikisi de sabit imleç ister.
    return (blink == 0 || blink == INFINITE) ? 0 : blink;
}

[[nodiscard]] std::wstring LastLine(const std::wstring& text) {
    const size_t breakAt = text.find_last_of(L'\n');
    return breakAt == std::wstring::npos ? text : text.substr(breakAt + 1);
}

[[nodiscard]] int LineCount(const std::wstring& text) noexcept {
    int lines = 1;
    for (const wchar_t ch : text) {
        if (ch == L'\n') {
            ++lines;
        }
    }
    return lines;
}

// IME aday penceresini imlecin yanına taşır. Yapılmazsa Japonca/Korece/Çince
// yazan kullanıcı, aday listesini pencerenin sol üst köşesinde bulur.
void MoveImeCaret(HWND window, POINT caret) {
    const HIMC context = ::ImmGetContext(window);
    if (context == nullptr) {
        return;
    }
    COMPOSITIONFORM form{};
    form.dwStyle = CFS_POINT;
    form.ptCurrentPos = caret;
    (void)::ImmSetCompositionWindow(context, &form);
    (void)::ImmReleaseContext(window, context);
}

}  // namespace

void DrawTextDraft(HDC dc, const State& state) {
    const Shape& draft = state.textDraft;
    const Palette& colors = theme::Colors();
    const POINT origin = ToClient(state, draft.start);

    const HFONT font =
        CreateTextFont(draft.thickness, state.dpi, state.scale, false);
    if (font == nullptr) {
        return;
    }
    const HGDIOBJ oldFont = ::SelectObject(dc, font);
    ::SetBkMode(dc, TRANSPARENT);

    TEXTMETRICW metrics{};
    ::GetTextMetricsW(dc, &metrics);
    const int lineHeight = metrics.tmHeight;

    // BOŞKEN İPUCU: kutunun içinde soluk bir "Yazmaya başlayın" durur. Boş bir
    // çerçeve, kullanıcıya orada ne yapması gerektiğini söylemiyordu.
    const bool empty = draft.text.empty();
    const std::wstring shown = empty ? Loc::Str(IDS_TEXT_HINT) : draft.text;

    RECT measure{0, 0, 0, 0};
    ::DrawTextW(dc, shown.c_str(), -1, &measure, DT_CALCRECT | DT_NOPREFIX);
    const int width = (std::max)(static_cast<int>(geom::Width(measure)),
                                 Scale(24, state.dpi));
    const int height = (std::max)(static_cast<int>(geom::Height(measure)),
                                  lineHeight);

    const int pad = Scale(4, state.dpi);
    const RECT box{origin.x - pad, origin.y - pad, origin.x + width + pad,
                   origin.y + height + pad};

    // Kutu KESİKLİ ve vurgu renginde: düz bir çerçeve, kullanıcının çizdiği
    // dikdörtgen aracının sonucuyla karışırdı.
    const HPEN pen = ::CreatePen(PS_DOT, 1, colors.accent);
    if (pen != nullptr) {
        const HGDIOBJ oldPen = ::SelectObject(dc, pen);
        const HGDIOBJ oldBrush = ::SelectObject(dc, ::GetStockObject(NULL_BRUSH));
        ::Rectangle(dc, box.left, box.top, box.right, box.bottom);
        ::SelectObject(dc, oldBrush);
        ::SelectObject(dc, oldPen);
        ::DeleteObject(pen);
    }

    ::SetTextColor(dc, empty ? colors.textDim : draft.color);
    RECT area{origin.x, origin.y, origin.x + width + Scale(400, state.dpi),
              origin.y + height + Scale(400, state.dpi)};
    ::DrawTextW(dc, shown.c_str(), -1, &area, DT_NOPREFIX | DT_NOCLIP);

    // İMLEÇ: son satırın sonunda, yanıp sönerek. Sabit bir çizgi metnin parçası
    // sanılırdı; yanıp sönen bir çizgi "buraya yazılıyor" demenin evrensel yolu.
    const std::wstring tail = empty ? std::wstring() : LastLine(draft.text);
    RECT tailMeasure{0, 0, 0, 0};
    if (!tail.empty()) {
        ::DrawTextW(dc, tail.c_str(), -1, &tailMeasure,
                    DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    }
    const int caretX = origin.x + static_cast<int>(geom::Width(tailMeasure));
    const int caretY =
        origin.y + (empty ? 0 : (LineCount(draft.text) - 1) * lineHeight);

    if (state.caretOn) {
        FillRectColor(dc,
                      RECT{caretX, caretY,
                           caretX + (std::max)(1, Scale(2, state.dpi)),
                           caretY + lineHeight},
                      empty ? colors.accent : draft.color);
    }

    ::SelectObject(dc, oldFont);
    ::DeleteObject(font);
}

bool TextTypingChar(HWND window, State& state, wchar_t ch) {
    if (!state.typing) {
        return false;
    }
    std::wstring& text = state.textDraft.text;

    if (ch == L'\b') {
        // VEKİL ÇİFTİ TEK KARAKTERDİR: emoji yazıp geri sildiğinde tek bir
        // pop_back yarım bir kod birimi bırakır ve metin bozulur.
        if (!text.empty()) {
            const wchar_t last = text.back();
            text.pop_back();
            if (last >= 0xDC00 && last <= 0xDFFF && !text.empty()) {
                text.pop_back();
            }
        }
    } else if (ch == L'\r') {
        text.push_back(L'\n');   // Enter satır atlar
    } else if (ch == L'\n') {
        // Ctrl+Enter bitirir. Enter'ın kendisi artık satır atladığı için
        // yazmayı sonlandıracak bir tuş gerekiyordu.
        CommitTextDraft(state);
        RECT client{};
        ::GetClientRect(window, &client);
        LayoutButtons(state, client);
    } else if (ch == L'\t') {
        text.append(4, L' ');
    } else if (ch >= L' ') {
        text.push_back(ch);
    } else {
        return true;   // diğer denetim karakterleri yutulur
    }

    // İmleç her tuşta yeniden görünür olmalı: yanıp sönme evresi kapalıyken
    // yazmak, harfin nereye gittiğini göstermezdi.
    state.caretOn = true;
    const UINT period = CaretPeriod();
    if (period != 0 && state.typing) {
        ::SetTimer(window, kCaretTimer, period, nullptr);
    }
    if (state.typing) {
        MoveImeCaret(window, ToClient(state, state.textDraft.start));
    }
    ::InvalidateRect(window, nullptr, FALSE);
    return true;
}

}  // namespace editor
}  // namespace crisp
