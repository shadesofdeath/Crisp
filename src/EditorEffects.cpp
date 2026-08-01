// EditorEffects.cpp — Görüntünün tamamına uygulanan işlemler: çevirme,
// otomatik kırpma, kenar boşluğu ve renk ayarları.
//
// TEK MENÜ, ON DÖRT KOMUT. Her biri için araç çubuğuna düğme koymak çubuğu
// ikiye katlar ve hiçbiri sık kullanılmıyor: bir ekran görüntüsünü gri yapmak
// oturumda bir kez olur, ok çizmek yirmi kez.
//
// HEPSİ TEK BİR GERİ ALMA ADIMI: efekt pikselleri değiştirir ve şekilleri
// tabana pişirir; kullanıcı beğenmezse Ctrl+Z ile hem efekt hem pişirme geri
// döner.
#include "EditorInternal.h"

#include "ImageAdjust.h"
#include "ImageCodec.h"
#include "MessageWindow.h"
#include "ImageTransform.h"
#include "Localization.h"
#include "resource.h"

#include <utility>

namespace crisp {
namespace editor {
namespace {

// Kenar boşluğu, görüntünün kısa kenarının yüzdesi olarak. Sabit bir piksel
// sayısı, 400 piksellik bir kırpmada devasa, 4K bir ekranda görünmez olurdu.
constexpr int kPaddingPercent = 6;

[[nodiscard]] std::wstring WithSign(UINT labelId, bool plus) {
    std::wstring text = Loc::Str(labelId);
    text += plus ? L"  +" : L"  −";
    return text;
}

// Efekti geçerli görüntüye uygular ve sonucu yeni taban yapar.
template <typename Fn>
void ApplyInPlace(State& state, Fn&& effect) {
    Image flattened;
    if (!CurrentFlattened(state, flattened)) {
        return;
    }
    effect(flattened);
    DropScaleSource(state);
    BakeAndReplace(state, std::move(flattened));
}

void ApplyFlip(State& state, FlipAxis axis) {
    Image flattened;
    if (!CurrentFlattened(state, flattened)) {
        return;
    }
    Image flipped;
    if (!FlipImage(flattened, axis, flipped)) {
        return;
    }
    DropScaleSource(state);
    BakeAndReplace(state, std::move(flipped));
}

void ApplyAutoCrop(State& state) {
    Image flattened;
    if (!CurrentFlattened(state, flattened)) {
        return;
    }
    Image cropped;
    // TOLERANS 6: JPEG'den geçmiş bir görüntüde "beyaz" kenar tam olarak 255
    // değildir ve sıfır tolerans hiçbir şey kırpmazdı.
    if (!AutoCropImage(flattened, 6, cropped)) {
        return;
    }
    DropScaleSource(state);
    BakeAndReplace(state, std::move(cropped));
}

void ApplyPadding(State& state) {
    Image flattened;
    if (!CurrentFlattened(state, flattened)) {
        return;
    }
    const int shorter = flattened.Width() < flattened.Height()
                            ? flattened.Width()
                            : flattened.Height();
    int pad = shorter * kPaddingPercent / 100;
    if (pad < 8) {
        pad = 8;
    }

    // BOŞLUK GEÇERLİ ÇİZİM RENGİNDE: ayrı bir "kenar boşluğu rengi" ayarı,
    // kullanıcının zaten seçtiği rengin yanında ikinci bir renk kararı olurdu.
    const uint32_t fill = 0xFF000000u |
                          (static_cast<uint32_t>(GetRValue(state.color)) << 16) |
                          (static_cast<uint32_t>(GetGValue(state.color)) << 8) |
                          static_cast<uint32_t>(GetBValue(state.color));

    Image padded;
    if (!ResizeCanvas(flattened, flattened.Width() + pad * 2,
                      flattened.Height() + pad * 2, pad, pad, fill, padded)) {
        return;
    }
    DropScaleSource(state);
    BakeAndReplace(state, std::move(padded));
}

}  // namespace

void ShowEffectsMenu(HWND window, State& state) {
    const HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    auto add = [menu](UINT command, const std::wstring& text) {
        ::AppendMenuW(menu, MF_STRING, command, text.c_str());
    };
    auto separator = [menu]() { ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr); };

    add(kEffectFlipHorizontal, Loc::Str(IDS_IMG_FLIP_H));
    add(kEffectFlipVertical, Loc::Str(IDS_IMG_FLIP_V));
    add(kEffectAutoCrop, Loc::Str(IDS_IMG_AUTOCROP));
    add(kEffectPadding, Loc::Str(IDS_IMG_PADDING));
    separator();
    add(kEffectGrayscale, Loc::Str(IDS_IMG_GRAYSCALE));
    add(kEffectInvert, Loc::Str(IDS_IMG_INVERT));
    add(kEffectSepia, Loc::Str(IDS_IMG_SEPIA));
    add(kEffectSharpen, Loc::Str(IDS_IMG_SHARPEN));
    separator();
    // ARTI VE EKSİ AYNI ETİKETTEN TÜRETİLİR: "Parlaklık +" ve "Parlaklık −"
    // için iki ayrı çeviri dizesi tutmak, on altı dilde altı gereksiz satır
    // demek olurdu.
    add(kEffectBrighter, WithSign(IDS_IMG_BRIGHTNESS, true));
    add(kEffectDarker, WithSign(IDS_IMG_BRIGHTNESS, false));
    add(kEffectMoreContrast, WithSign(IDS_IMG_CONTRAST, true));
    add(kEffectLessContrast, WithSign(IDS_IMG_CONTRAST, false));
    add(kEffectMoreSaturation, WithSign(IDS_IMG_SATURATION, true));
    add(kEffectLessSaturation, WithSign(IDS_IMG_SATURATION, false));

    POINT cursor{};
    ::GetCursorPos(&cursor);
    ::SetForegroundWindow(window);
    const int chosen = ::TrackPopupMenuEx(
        menu, TPM_RETURNCMD | TPM_NONOTIFY, cursor.x, cursor.y, window, nullptr);
    ::DestroyMenu(menu);

    // ADIM BÜYÜKLÜĞÜ 15: daha küçüğü tek tıklamada fark edilmez ve kullanıcı
    // menüyü beş kez açar; daha büyüğü ince ayar yapmayı imkânsız kılar.
    constexpr int kStep = 15;
    switch (chosen) {
        case kEffectFlipHorizontal: ApplyFlip(state, FlipAxis::Horizontal); break;
        case kEffectFlipVertical:   ApplyFlip(state, FlipAxis::Vertical); break;
        case kEffectAutoCrop:       ApplyAutoCrop(state); break;
        case kEffectPadding:        ApplyPadding(state); break;
        case kEffectGrayscale:
            ApplyInPlace(state, [](Image& image) { ApplyGrayscale(image); });
            break;
        case kEffectInvert:
            ApplyInPlace(state, [](Image& image) { ApplyInvert(image); });
            break;
        case kEffectSepia:
            ApplyInPlace(state, [](Image& image) { ApplySepia(image); });
            break;
        case kEffectSharpen:
            ApplyInPlace(state, [](Image& image) { ApplySharpen(image, 60); });
            break;
        case kEffectBrighter:
            ApplyInPlace(state,
                         [](Image& image) { AdjustBrightness(image, kStep); });
            break;
        case kEffectDarker:
            ApplyInPlace(state,
                         [](Image& image) { AdjustBrightness(image, -kStep); });
            break;
        case kEffectMoreContrast:
            ApplyInPlace(state,
                         [](Image& image) { AdjustContrast(image, kStep); });
            break;
        case kEffectLessContrast:
            ApplyInPlace(state,
                         [](Image& image) { AdjustContrast(image, -kStep); });
            break;
        case kEffectMoreSaturation:
            ApplyInPlace(state,
                         [](Image& image) { AdjustSaturation(image, kStep * 2); });
            break;
        case kEffectLessSaturation:
            ApplyInPlace(state,
                         [](Image& image) { AdjustSaturation(image, -kStep * 2); });
            break;
        default:
            break;
    }
}

void OpenDroppedImage(HWND window, State& state, const std::wstring& path) {
    Image loaded;
    if (!LoadImageFile(path, loaded)) {
        ShowMessage(state.instance, window, Loc::Str(IDS_OPEN_FAILED),
                    MessageIcon::Error);
        return;
    }
    DropScaleSource(state);
    BakeAndReplace(state, std::move(loaded));

    // Görüntünün BOYUTU değişti: tuval yeniden yerleşmezse eski ölçekle
    // çizilir ve fare koordinatları kayar.
    RECT client{};
    ::GetClientRect(window, &client);
    LayoutButtons(state, client);
    LayoutCanvas(state, client);
    ::InvalidateRect(window, nullptr, FALSE);
}

}  // namespace editor
}  // namespace crisp
