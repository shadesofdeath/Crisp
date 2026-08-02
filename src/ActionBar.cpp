// ActionBar.cpp — bkz. ActionBar.h.
#include "ActionBar.h"

#include "Geometry.h"
#include "resource.h"

namespace crisp {
namespace {

// Çubuğun içeriği, soldan sağa. Sıra rastgele değil: en sık kullanılan solda.
//
// KOPYALA BAŞTA çünkü ekran görüntülerinin çoğu bir yere yapıştırılmak için
// alınıyor. İğnele ve yükle sonda: ikisi de daha nadir ve biri ağa çıkıyor.
struct ActionSpec {
    OverlayAction action;
    UINT nameId;
};

constexpr ActionSpec kSpecs[] = {
    {OverlayAction::Copy,   IDS_BAR_COPY},
    {OverlayAction::Save,   IDS_BAR_SAVE},
    {OverlayAction::Edit,   IDS_BAR_EDIT},
    {OverlayAction::Ocr,    IDS_BAR_OCR},
    {OverlayAction::Pin,    IDS_BAR_PIN},
    {OverlayAction::Upload, IDS_BAR_UPLOAD},
};

[[nodiscard]] LONG Scale(int value, unsigned dpi) noexcept {
    return ::MulDiv(value, static_cast<int>(dpi), 96);
}

}  // namespace

int ActionButtons(const RECT& selection, const RECT& monitor, unsigned dpi,
                  bool uploadEnabled, ActionButton* buttons,
                  RECT& barBounds) noexcept {
    barBounds = RECT{};
    if (buttons == nullptr || geom::IsEmpty(selection)) {
        return 0;
    }

    const LONG side = Scale(kActionButtonSide, dpi);
    const LONG padding = Scale(kActionBarPadding, dpi);
    const LONG gap = Scale(kActionBarGap, dpi);

    int count = 0;
    for (const ActionSpec& spec : kSpecs) {
        if (spec.action == OverlayAction::Upload && !uploadEnabled) {
            continue;
        }
        buttons[count].action = spec.action;
        buttons[count].nameId = spec.nameId;
        ++count;
    }

    const SIZE barSize{side * count + padding * 2, side + padding * 2};

    // ÇUBUK SEÇİMDEN GENİŞSE HİÇ ÇİZİLMEZ. Kırk piksellik bir seçimin yanında
    // duran iki yüz piksellik bir çubuk, seçimi anlatmak yerine örter — ve o
    // ölçekte kullanıcı zaten Enter'a basacaktır.
    if (barSize.cx > geom::Width(selection)) {
        return 0;
    }

    const POINT origin = geom::ActionBarPlacement(selection, barSize, gap, monitor);
    barBounds = RECT{origin.x, origin.y, origin.x + barSize.cx,
                     origin.y + barSize.cy};

    for (int i = 0; i < count; ++i) {
        const LONG left = origin.x + padding + side * i;
        buttons[i].bounds = RECT{left, origin.y + padding, left + side,
                                 origin.y + padding + side};
    }
    return count;
}

int ActionButtonAt(const ActionButton* buttons, int count,
                   POINT screenPoint) noexcept {
    if (buttons == nullptr) {
        return -1;
    }
    for (int i = 0; i < count; ++i) {
        if (::PtInRect(&buttons[i].bounds, screenPoint) != FALSE) {
            return i;
        }
    }
    return -1;
}

}  // namespace crisp
