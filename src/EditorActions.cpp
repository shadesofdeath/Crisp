// EditorActions.cpp — Araç çubuğu eylemleri ve çizim adımları.
//
// AYRI DOSYA: pencere yordamı tek başına dört yüz satıra yaklaşıyor;
// eylemler ve çizim onunla aynı dosyada kalınca EditorInput.cpp ev
// kuralının sınırını aşıyordu (docs §9). Ayrım işlevsel: burası NE
// yapıldığını, EditorInput.cpp hangi mesajla tetiklendiğini anlatır.
#include "EditorInternal.h"

#include "ClipboardImage.h"
#include "EditorRender.h"
#include "Geometry.h"
#include "ImageEffects.h"
#include "ImageTransform.h"
#include "Localization.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace crisp {
namespace editor {

void CommitTextDraft(State& state) {
    if (!state.typing) {
        return;
    }
    state.typing = false;
    if (!state.textDraft.text.empty()) {
        state.document.AddShape(state.textDraft);
        Rebuild(state);
    }
    state.textDraft = Shape{};
}

// Şekilleri tabana PİŞİRİR ve yeni tabanı belgeye verir. Kırpma, döndürme ve
// ölçekleme bundan geçer: şekil koordinatları eski görüntünün uzayında olduğu
// için işlem sonrası korunamazlar, ama piksele dönüşmüş hâlleri korunur.
void BakeAndReplace(State& state, Image&& newBase) {
    auto shared = std::make_shared<Image>();
    *shared = std::move(newBase);
    state.document.ApplyImageOp(shared);
    Rebuild(state);
}

// Geçerli görüntüyü (şekiller pişmiş hâliyle) verir.
[[nodiscard]] bool CurrentFlattened(const State& state, Image& out) {
    if (state.image == nullptr || !state.image->Valid()) {
        return false;
    }
    return CropImage(*state.image, 0, 0, state.image->Width(),
                     state.image->Height(), out);
}

void RotateBy(State& state, int quarterTurns) {
    Image flattened;
    if (!CurrentFlattened(state, flattened)) {
        return;
    }
    Image rotated;
    if (!RotateImage(flattened, quarterTurns, rotated)) {
        return;
    }
    BakeAndReplace(state, std::move(rotated));
}

void ShowScaleMenu(HWND window, State& state) {
    const HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }
    // Ölçek YÜZDE OLARAK sunulur, piksel girişi olarak değil: bir ekran
    // görüntüsünde asıl istenen "yarısı kadar" gibi bir şey ve serbest giriş
    // için bir iletişim kutusu yazmak, aynı işi daha çok tıklamayla yapardı.
    for (const int percent : {25, 50, 75, 150, 200}) {
        wchar_t label[16];
        ::swprintf_s(label, L"%%%d", percent);
        ::AppendMenuW(menu, MF_STRING, static_cast<UINT_PTR>(percent), label);
    }

    POINT cursor{};
    ::GetCursorPos(&cursor);
    ::SetForegroundWindow(window);
    const int chosen = ::TrackPopupMenuEx(
        menu, TPM_RETURNCMD | TPM_NONOTIFY, cursor.x, cursor.y, window, nullptr);
    ::DestroyMenu(menu);

    if (chosen <= 0) {
        return;
    }

    Image flattened;
    if (!CurrentFlattened(state, flattened)) {
        return;
    }
    Image scaled;
    if (!ScaleImageByPercent(flattened, chosen, scaled)) {
        return;
    }
    BakeAndReplace(state, std::move(scaled));
}

void ApplyAction(HWND window, State& state, int action) {
    // Bir eylem her zaman önce yazılmakta olan metni kesinleştirir; yoksa
    // kullanıcı yazarken "kaydet"e bastığında yazdığı kaybolurdu.
    CommitTextDraft(state);

    switch (action) {
        case kActionZoomOut:
        case kActionZoomIn: {
            // Düğmeyle yakınlaştırırken çıpa GÖRÜNÜR ALANIN ORTASI olur; fare
            // düğmenin üstünde durduğu için imleci çıpa almak, görüntüyü araç
            // çubuğuna doğru kaydırırdı.
            const POINT centre{
                (state.viewport.left + state.viewport.right) / 2,
                (state.viewport.top + state.viewport.bottom) / 2};
            ZoomAt(window, state, action == kActionZoomIn ? 1.25 : 0.8, centre);
            return;
        }
        case kActionZoomFit:
            // Sığdırma ile %100 arasında gidip gelir: iki ayrı düğme, araç
            // çubuğunda yer kaplayıp aynı işi yapardı.
            if (state.fitToWindow) {
                ZoomToActual(window, state);
            } else {
                ZoomToFit(window, state);
            }
            return;
        case kActionOcr:
            ToggleOcrMode(window, state);
            return;
        case kActionRotateLeft:
            RotateBy(state, -1);
            break;
        case kActionRotateRight:
            RotateBy(state, 1);
            break;
        case kActionScale:
            ShowScaleMenu(window, state);
            break;
        case kActionUndo:
            if (state.document.Undo()) {
                Rebuild(state);
            }
            break;
        case kActionRedo:
            if (state.document.Redo()) {
                Rebuild(state);
            }
            break;
        case kActionClear:
            state.document.Clear();
            Rebuild(state);
            break;
        case kActionCopy:
            state.result.accepted = true;
            state.result.copyToClipboard = true;
            ::DestroyWindow(window);
            return;
        case kActionSave:
            state.result.accepted = true;
            state.result.saveToFile = true;
            ::DestroyWindow(window);
            return;
        case kActionClose:
            ::DestroyWindow(window);
            return;
        default:
            break;
    }

    RECT client{};
    ::GetClientRect(window, &client);
    LayoutButtons(state, client);   // geri al/yinele etkinliği değişmiş olabilir
    // Döndürme ve ölçekleme görüntünün BOYUTUNU değiştirir; tuval yeniden
    // yerleşmezse görüntü eski ölçekle çizilir ve fare koordinatları kayar.
    LayoutCanvas(state, client);
    ::InvalidateRect(window, nullptr, FALSE);
}

void BeginDraw(HWND window, State& state, POINT client) {
    if (!::PtInRect(&state.canvas, client)) {
        return;
    }
    const POINT image = ToImage(state, client);

    if (state.tool == ToolKind::Text) {
        CommitTextDraft(state);
        state.typing = true;
        state.textDraft = Shape{};
        state.textDraft.kind = ToolKind::Text;
        state.textDraft.start = image;
        state.textDraft.end = image;
        state.textDraft.color = state.color;
        state.textDraft.thickness = state.thickness;
        ::InvalidateRect(window, nullptr, FALSE);
        return;
    }

    CommitTextDraft(state);

    if (state.tool == ToolKind::StepNumber) {
        Shape shape;
        shape.kind = ToolKind::StepNumber;
        shape.start = image;
        shape.end = image;
        shape.color = state.color;
        shape.thickness = state.thickness;
        state.document.AddShape(std::move(shape));
        Rebuild(state);

        RECT clientRect{};
        ::GetClientRect(window, &clientRect);
        LayoutButtons(state, clientRect);
        ::InvalidateRect(window, nullptr, FALSE);
        return;
    }

    state.dragging = true;
    state.draft = Shape{};
    state.draft.kind = state.tool;
    state.draft.start = image;
    state.draft.end = image;
    state.draft.color = state.color;
    state.draft.thickness = state.thickness;
    if (ToolIsFreehand(state.tool)) {
        state.draft.points.push_back(image);
    }
    ::SetCapture(window);
}

void UpdateDraw(HWND window, State& state, POINT client) {
    if (!state.dragging) {
        return;
    }
    const POINT image = ToImage(state, client);
    state.draft.end = image;
    if (ToolIsFreehand(state.draft.kind)) {
        // Aynı pikselde tekrar eden noktalar biriktirilmez: uzun bir çizimde
        // binlerce gereksiz nokta hem belleği hem çizimi şişirir.
        if (state.draft.points.empty() ||
            state.draft.points.back().x != image.x ||
            state.draft.points.back().y != image.y) {
            state.draft.points.push_back(image);
        }
    }
    ::InvalidateRect(window, nullptr, FALSE);
}

void EndDraw(HWND window, State& state) {
    if (!state.dragging) {
        return;
    }
    state.dragging = false;
    ::ReleaseCapture();

    const RECT bounds = state.draft.Bounds();
    const bool freehand = ToolIsFreehand(state.draft.kind);
    // Kazara tıklama şekil üretmesin: serbest çizimde en az iki nokta, diğer
    // araçlarda en az birkaç piksellik bir sürükleme gerekir.
    const bool usable = freehand ? state.draft.points.size() >= 2
                                 : (geom::Width(bounds) >= 3 ||
                                    geom::Height(bounds) >= 3);
    if (usable && ToolIsImageOp(state.draft.kind)) {
        // Kırpma şekil eklemez: görüntüyü küçültür. Alan görüntünün içine
        // hapsedilir, yoksa dışarı taşan bir sürükleme CropImage'i reddettirir.
        Image flattened;
        if (CurrentFlattened(state, flattened)) {
            const RECT clipped = geom::ClampTo(
                bounds, RECT{0, 0, flattened.Width(), flattened.Height()});
            Image cropped;
            if (geom::Width(clipped) >= 8 && geom::Height(clipped) >= 8 &&
                CropImage(flattened, static_cast<int>(clipped.left),
                          static_cast<int>(clipped.top),
                          static_cast<int>(geom::Width(clipped)),
                          static_cast<int>(geom::Height(clipped)), cropped)) {
                BakeAndReplace(state, std::move(cropped));
            }
        }
    } else if (usable) {
        state.document.AddShape(state.draft);
        Rebuild(state);
    }
    state.draft = Shape{};

    RECT client{};
    ::GetClientRect(window, &client);
    LayoutButtons(state, client);
    LayoutCanvas(state, client);
    ::InvalidateRect(window, nullptr, FALSE);
}

}  // namespace editor
}  // namespace crisp
