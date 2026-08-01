// EditorActions.cpp — Araç çubuğu eylemleri ve çizim adımları.
//
// AYRI DOSYA: pencere yordamı tek başına dört yüz satıra yaklaşıyor;
// eylemler ve çizim onunla aynı dosyada kalınca EditorInput.cpp ev
// kuralının sınırını aşıyordu (docs §9). Ayrım işlevsel: burası NE
// yapıldığını, EditorInput.cpp hangi mesajla tetiklendiğini anlatır.
#include "EditorInternal.h"

#include "ColorPicker.h"
#include "Geometry.h"
#include "ImageTransform.h"
#include "ThicknessPicker.h"

#include <algorithm>
#include <memory>
#include <utility>

namespace crisp {
namespace editor {
// ÖLÇEK KAYNAĞI, ÖLÇEKLEME DIŞINDAKİ HER İŞLEMDE DÜŞER. Kırpma, döndürme,
// yeni bir şekil ya da geri alma sonrası elde tutulan kopya artık ekrandaki
// görüntünün geçmişi değildir; onu kullanmak, kullanıcının arada yaptığı işi
// sessizce geri almak olurdu.
void DropScaleSource(State& state) noexcept {
    state.scaleSource.reset();
    state.scalePercent = 100;
}

// Şekilleri tabana PİŞİRİR ve yeni tabanı belgeye verir. Kırpma, döndürme,
// çevirme ve renk ayarları bundan geçer: şekil koordinatları eski görüntünün
// uzayında olduğu için işlem sonrası korunamazlar, ama piksele dönüşmüş
// hâlleri korunur.
void BakeAndReplace(State& state, Image&& newBase) {
    auto shared = std::make_shared<Image>();
    *shared = std::move(newBase);
    state.document.ApplyImageOp(shared);
    Rebuild(state);
}

namespace {

void RotateBy(State& state, int quarterTurns) {
    Image flattened;
    if (!CurrentFlattened(state, flattened)) {
        return;
    }
    Image rotated;
    if (!RotateImage(flattened, quarterTurns, rotated)) {
        return;
    }
    DropScaleSource(state);
    BakeAndReplace(state, std::move(rotated));
}

// Yüzdeyi ÖLÇEK KAYNAĞINA uygular, ekrandaki görüntüye değil.
//
// SEBEBİ DOĞRUDAN BİR HATA RAPORU: %25'e indirip sonra %100'e dönen kullanıcı
// bulanık bir görüntü buluyordu. Çünkü ikinci işlem, artık dörtte bir
// piksele sahip olan görüntüyü dört kat büyütüyordu — atılan pikselleri hiçbir
// süzgeç geri getiremez. Ölçekleme öncesi hâli saklamak, ardışık her yüzdeyi
// ÖZGÜN piksellerden üretir ve %100 gerçekten %100 olur.
void ApplyScale(State& state, int percent) {
    if (percent <= 0 || percent == state.scalePercent) {
        return;
    }
    if (!state.scaleSource) {
        Image flattened;
        if (!CurrentFlattened(state, flattened)) {
            return;
        }
        auto source = std::make_shared<Image>();
        *source = std::move(flattened);
        state.scaleSource = source;
        state.scalePercent = 100;
    }

    const Image& source = *state.scaleSource;
    Image scaled;
    const bool ok =
        percent == 100
            ? CropImage(source, 0, 0, source.Width(), source.Height(), scaled)
            : ScaleImageByPercent(source, percent, scaled);
    if (!ok) {
        return;
    }

    // Kaynak BakeAndReplace içinde düşmemeli: ardışık ölçeklemelerin tamamı
    // aynı özgün pikselleri kullanır.
    const std::shared_ptr<const Image> keep = state.scaleSource;
    BakeAndReplace(state, std::move(scaled));
    state.scaleSource = keep;
    state.scalePercent = percent;
}

void ShowScaleMenu(HWND window, State& state) {
    const HMENU menu = ::CreatePopupMenu();
    if (menu == nullptr) {
        return;
    }

    const Image* source =
        state.scaleSource ? state.scaleSource.get() : state.image;
    if (source == nullptr || !source->Valid()) {
        ::DestroyMenu(menu);
        return;
    }

    // Ölçek YÜZDE OLARAK sunulur, piksel girişi olarak değil: bir ekran
    // görüntüsünde asıl istenen "yarısı kadar" gibi bir şey ve serbest giriş
    // için bir iletişim kutusu yazmak, aynı işi daha çok tıklamayla yapardı.
    //
    // SONUÇ ÖLÇÜSÜ DE YAZAR: "%25" tek başına kaç piksel edeceğini söylemiyor
    // ve kullanıcı sonucu ancak uyguladıktan sonra görüyordu.
    for (const int percent : {25, 50, 75, 100, 150, 200}) {
        wchar_t label[64];
        ::swprintf_s(label, L"%%%d\t%d × %d", percent,
                     (std::max)(1, ::MulDiv(source->Width(), percent, 100)),
                     (std::max)(1, ::MulDiv(source->Height(), percent, 100)));
        const UINT flags =
            MF_STRING | (percent == state.scalePercent ? MF_CHECKED : 0u);
        ::AppendMenuW(menu, flags, static_cast<UINT_PTR>(percent), label);
    }

    POINT cursor{};
    ::GetCursorPos(&cursor);
    ::SetForegroundWindow(window);
    const int chosen = ::TrackPopupMenuEx(
        menu, TPM_RETURNCMD | TPM_NONOTIFY, cursor.x, cursor.y, window, nullptr);
    ::DestroyMenu(menu);

    if (chosen > 0) {
        ApplyScale(state, chosen);
    }
}

// Düğmenin ekran koordinatındaki dikdörtgeni; açılır paneller buranın altına
// yerleşir.
[[nodiscard]] RECT AnchorOf(HWND window, const Button& button) noexcept {
    RECT anchor = button.bounds;
    ::MapWindowPoints(window, nullptr, reinterpret_cast<POINT*>(&anchor), 2);
    return anchor;
}

void Refresh(HWND window, State& state) {
    RECT client{};
    ::GetClientRect(window, &client);
    LayoutButtons(state, client);
    LayoutCanvas(state, client);
    ::InvalidateRect(window, nullptr, FALSE);
}

}  // namespace

void CommitTextDraft(State& state) {
    if (!state.typing) {
        return;
    }
    state.typing = false;
    if (!state.textDraft.text.empty()) {
        state.document.AddShape(state.textDraft);
        DropScaleSource(state);
        Rebuild(state);
    }
    state.textDraft = Shape{};
}

void OpenColorPicker(HWND window, State& state, const Button& button) {
    COLORREF chosen = state.color;
    if (PickColor(state.instance, window, AnchorOf(window, button),
                  state.settings, chosen, state.recentColors)) {
        state.color = chosen;
        // Yazılmakta olan metnin rengi de anında değişsin.
        if (state.typing) {
            state.textDraft.color = chosen;
        }
        // SEÇİLİ ŞEKİL DE: renk düğmesine basan kullanıcı, seçtiği okun
        // rengini değiştirmek istiyordur.
        RestyleSelectedShape(state);
    }
    Refresh(window, state);
}

void OpenThicknessPicker(HWND window, State& state, const Button& button) {
    int chosen = state.thickness;
    if (PickThickness(state.instance, window, AnchorOf(window, button),
                      state.color, chosen)) {
        state.thickness = chosen;
        if (state.typing) {
            state.textDraft.thickness = chosen;
        }
        RestyleSelectedShape(state);
    }
    Refresh(window, state);
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
        case kActionEffects:
            ShowEffectsMenu(window, state);
            break;
        case kActionFill:
            state.fillShapes = !state.fillShapes;
            // SEÇİLİ ŞEKİL DE DEĞİŞİR: dolgu düğmesine basan kullanıcı,
            // seçtiği şeklin dolgusunu değiştirmek istiyordur; yalnızca
            // bundan sonrakileri etkilemesi şaşırtıcı olurdu.
            RestyleSelectedShape(state);
            break;
        case kActionUndo:
            if (state.document.Undo()) {
                DropScaleSource(state);
                Rebuild(state);
            }
            break;
        case kActionRedo:
            if (state.document.Redo()) {
                DropScaleSource(state);
                Rebuild(state);
            }
            break;
        case kActionClear:
            state.document.Clear();
            DropScaleSource(state);
            Rebuild(state);
            break;
        // ÜÇÜ DE PENCEREYİ KAPATMAZ: kullanıcı kopyaladıktan sonra çizmeye
        // devam edebilmeli ve ne olduğunu görebilmeli.
        case kActionCopy:
            CopyToClipboard(window, state);
            return;
        case kActionSave:
            SaveToFolder(window, state);
            return;
        case kActionSaveAs:
            SaveAsDialog(window, state);
            return;
        case kActionClose:
            ::DestroyWindow(window);
            return;
        default:
            break;
    }

    // Geri al/yinele etkinliği değişmiş olabilir; döndürme ve ölçekleme
    // görüntünün BOYUTUNU değiştirir ve tuval yeniden yerleşmezse görüntü eski
    // ölçekle çizilir, fare koordinatları kayar.
    Refresh(window, state);
}

void BeginDraw(HWND window, State& state, POINT client) {
    if (!::PtInRect(&state.canvas, client)) {
        return;
    }
    const POINT image = ToImage(state, client);

    if (state.tool == ToolKind::Text) {
        CommitTextDraft(state);
        state.typing = true;
        state.caretOn = true;
        state.textDraft = Shape{};
        state.textDraft.kind = ToolKind::Text;
        state.textDraft.start = image;
        state.textDraft.end = image;
        state.textDraft.color = state.color;
        state.textDraft.thickness = state.thickness;
        const UINT blink = ::GetCaretBlinkTime();
        if (blink != 0 && blink != INFINITE) {
            ::SetTimer(window, kCaretTimer, blink, nullptr);
        }
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
        DropScaleSource(state);
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
    state.draft.filled = state.fillShapes;
    state.draft.strength = state.tool == ToolKind::Mosaic
                               ? static_cast<int>(state.settings.mosaicStrength)
                               : static_cast<int>(state.settings.blurStrength);
    if (ToolIsFreehand(state.tool)) {
        state.draft.points.push_back(image);
    }
    ::SetCapture(window);
}

void UpdateDraw(HWND window, State& state, POINT client) {
    if (!state.dragging) {
        return;
    }
    POINT image = ToImage(state, client);

    // SHIFT KİLİDİ: dikdörtgen kare, elips daire, çizgi ve ok 45°'nin katı
    // olur. Elle "tam kare" çizmeye çalışmak, her seferinde birkaç piksel
    // şaşan bir sonuç veriyordu.
    if ((::GetKeyState(VK_SHIFT) & 0x8000) != 0 &&
        !ToolIsFreehand(state.draft.kind)) {
        image = state.draft.kind == ToolKind::Arrow ||
                        state.draft.kind == ToolKind::Line
                    ? geom::SnapToAngle(state.draft.start, image)
                    : geom::SnapToSquare(state.draft.start, image);
    }

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
                DropScaleSource(state);
                BakeAndReplace(state, std::move(cropped));
            }
        }
    } else if (usable) {
        state.document.AddShape(state.draft);
        DropScaleSource(state);
        Rebuild(state);
    }
    state.draft = Shape{};

    Refresh(window, state);
}

}  // namespace editor
}  // namespace crisp
