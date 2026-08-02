// EditorLayout.cpp — Araç çubuğu ve tuval yerleşimi, koordinat dönüşümü,
// yakınlaştırma.
//
// HESAP BURADA DEĞİL: sığdırma ölçeği, kaydırma sınırı, çıpalı yakınlaştırma
// ve koordinat dönüşümleri geom:: altında, crisp_core'da. Burada kalan tek şey
// bunları pencere durumuna bağlamak. Ayrım bilinçli — matematik pencere
// açmadan sınanabilmeli, ve bu dosya bir zamanlar o kuralın tek istisnasıydı.
#include "EditorInternal.h"

#include "Geometry.h"
#include "Upload.h"
#include "resource.h"

#include <algorithm>
#include <cmath>

namespace crisp {
namespace editor {
namespace {

constexpr ToolKind kTools[] = {
    ToolKind::Select,      ToolKind::Arrow, ToolKind::Line,
    ToolKind::Rectangle,   ToolKind::Ellipse, ToolKind::Pen,
    ToolKind::Highlighter, ToolKind::Text,  ToolKind::StepNumber,
    ToolKind::Blur,        ToolKind::Mosaic, ToolKind::Crop};

constexpr int kImageActions[] = {kActionRotateLeft, kActionRotateRight,
                                 kActionScale, kActionEffects, kActionOcr};
constexpr int kEditActions[] = {kActionUndo, kActionRedo, kActionClear};
constexpr int kFileActions[] = {kActionCopy, kActionUpload, kActionSave,
                                kActionSaveAs, kActionClose};

[[nodiscard]] int GroupWidth(int group, unsigned dpi) noexcept {
    const int side = Scale(kButtonSide, dpi);
    const int gap = Scale(kButtonGap, dpi);
    const int drop = Scale(kDropdownWidth, dpi);
    switch (group) {
        case kGroupTools:
            return static_cast<int>(std::size(kTools)) * (side + gap) - gap;
        case kGroupStyle:
            return 2 * (drop + gap) + (side + gap) - gap;
        case kGroupImage:
            return static_cast<int>(std::size(kImageActions)) * (side + gap) - gap;
        case kGroupEdit:
            return static_cast<int>(std::size(kEditActions)) * (side + gap) - gap;
        default:
            return static_cast<int>(std::size(kFileActions)) * (side + gap) - gap;
    }
}

void AddAction(State& state, int group, int action, int& x, int top, int side,
               int gap) {
    Button button;
    button.kind = ButtonKind::Action;
    button.action = action;
    button.group = group;
    button.bounds = RECT{x, top, x + side, top + side};
    if (action == kActionUndo) {
        button.enabled = state.document.CanUndo();
    } else if (action == kActionRedo) {
        button.enabled = state.document.CanRedo();
    } else if (action == kActionClear) {
        button.enabled = !state.document.empty();
    } else if (action == kActionUpload) {
        // SERVİS SEÇİLMEMİŞSE DÜĞME KAPALI. Her basışta "önce ayarlardan bir
        // servis seç" diyen bir düğme, kapalı bir düğmenin zaten sessizce
        // söylediği şeyi gürültüyle söylerdi.
        button.enabled =
            UploadServiceFromId(state.settings.uploadService) != UploadService::None;
    }
    state.buttons.push_back(button);
    x += side + gap;
}

// Bir grubu `x`ten başlayarak yerleştirir ve grubun sınırlarını kaydeder.
void LayoutGroup(State& state, int group, UINT labelId, int& x, int top) {
    const int side = Scale(kButtonSide, state.dpi);
    const int gap = Scale(kButtonGap, state.dpi);
    const int drop = Scale(kDropdownWidth, state.dpi);
    const int start = x;

    switch (group) {
        case kGroupTools:
            for (const ToolKind tool : kTools) {
                Button button;
                button.kind = ButtonKind::Tool;
                button.tool = tool;
                button.group = group;
                button.bounds = RECT{x, top, x + side, top + side};
                state.buttons.push_back(button);
                x += side + gap;
            }
            break;
        case kGroupStyle: {
            Button color;
            color.kind = ButtonKind::Color;
            color.color = state.color;
            color.group = group;
            color.dropdown = true;
            color.bounds = RECT{x, top, x + drop, top + side};
            state.buttons.push_back(color);
            x += drop + gap;

            Button thickness;
            thickness.kind = ButtonKind::Thickness;
            thickness.thickness = state.thickness;
            thickness.group = group;
            thickness.dropdown = true;
            thickness.bounds = RECT{x, top, x + drop, top + side};
            state.buttons.push_back(thickness);
            x += drop + gap;

            AddAction(state, group, kActionFill, x, top, side, gap);
            break;
        }
        case kGroupImage:
            for (const int action : kImageActions) {
                AddAction(state, group, action, x, top, side, gap);
            }
            break;
        case kGroupEdit:
            for (const int action : kEditActions) {
                AddAction(state, group, action, x, top, side, gap);
            }
            break;
        default:
            for (const int action : kFileActions) {
                AddAction(state, group, action, x, top, side, gap);
            }
            break;
    }

    state.groups[group].labelId = labelId;
    state.groups[group].bounds = RECT{start, top, x - gap, top + side};
    x += Scale(kGroupGap, state.dpi) - gap;
}

}  // namespace

int RequiredToolbarWidth(unsigned dpi) noexcept {
    int total = Scale(kToolbarPad, dpi) * 2;
    for (int group = 0; group < kGroupCount; ++group) {
        total += GroupWidth(group, dpi);
    }
    total += Scale(kGroupGap, dpi) * (kGroupCount - 1);
    return total;
}

void LayoutButtons(State& state, const RECT& client) {
    state.buttons.clear();

    const int top = Scale(6, state.dpi);
    int x = Scale(kToolbarPad, state.dpi);

    LayoutGroup(state, kGroupTools, IDS_GROUP_TOOLS, x, top);
    LayoutGroup(state, kGroupStyle, IDS_GROUP_STYLE, x, top);
    LayoutGroup(state, kGroupImage, IDS_GROUP_IMAGE, x, top);
    LayoutGroup(state, kGroupEdit, IDS_GROUP_EDIT, x, top);

    // DOSYA GRUBU SAĞA YASLANIR: araç sayısı değiştikçe yerinin kayması, kas
    // hafızasıyla çalışan kullanıcıyı her sürümde yanıltırdı.
    //
    // PENCERE DARSA soldan devam eder. Sağa yaslamayı koşulsuz uygulamak, dar
    // bir pencerede dosya düğmelerini düzenleme düğmelerinin ÜSTÜNE bindirirdi
    // ve iki grup da tıklanamaz hâle gelirdi.
    const int fileWidth = GroupWidth(kGroupFile, state.dpi);
    int fileLeft = static_cast<int>(geom::Width(client)) -
                   Scale(kToolbarPad, state.dpi) - fileWidth;
    fileLeft = (std::max)(fileLeft, x);
    LayoutGroup(state, kGroupFile, IDS_GROUP_FILE, fileLeft, top);

    // Durum çubuğundaki yakınlaştırma denetimleri de düğmedir: aynı vurgu,
    // aynı ipucu, aynı tıklama yolu. Ayrı bir denetim tipi yazmak, hepsini
    // ikinci kez uygulamak olurdu.
    const int statusHeight = Scale(kStatusHeight, state.dpi);
    const int statusTop = static_cast<int>(geom::Height(client)) - statusHeight;
    const int smallSide = Scale(24, state.dpi);
    const int smallTop = statusTop + (statusHeight - smallSide) / 2;
    int right = static_cast<int>(geom::Width(client)) - Scale(12, state.dpi) -
                Scale(56, state.dpi);   // yüzde etiketine ayrılan yer

    // SAĞDAN SOLA: yüzde etiketi, artı, oluk, eksi, sığdır.
    auto addSmall = [&](int action) {
        Button button;
        button.kind = ButtonKind::Action;
        button.action = action;
        button.bounds =
            RECT{right - smallSide, smallTop, right, smallTop + smallSide};
        state.buttons.push_back(button);
        right -= smallSide + Scale(4, state.dpi);
    };

    addSmall(kActionZoomIn);

    const int trackWidth = Scale(120, state.dpi);
    Button slider;
    slider.kind = ButtonKind::ZoomSlider;
    slider.bounds = RECT{right - trackWidth, statusTop, right,
                         statusTop + statusHeight};
    state.buttons.push_back(slider);
    right -= trackWidth + Scale(4, state.dpi);

    addSmall(kActionZoomOut);
    addSmall(kActionZoomFit);
}

void ClampPan(State& state) {
    if (geom::IsEmpty(state.viewport) || state.image == nullptr) {
        return;
    }
    state.pan = geom::ClampPan(
        state.pan, static_cast<int>(state.image->Width() * state.scale),
        static_cast<int>(state.image->Height() * state.scale),
        static_cast<int>(geom::Width(state.viewport)),
        static_cast<int>(geom::Height(state.viewport)));
}

void LayoutCanvas(State& state, const RECT& client) {
    const int toolbar = Scale(kToolbarHeight, state.dpi);
    const int status = Scale(kStatusHeight, state.dpi);
    const int pad = Scale(10, state.dpi);

    state.viewport = RECT{pad, toolbar + pad,
                          static_cast<LONG>(geom::Width(client)) - pad,
                          static_cast<LONG>(geom::Height(client)) - status - pad};

    if (state.image == nullptr || !state.image->Valid() ||
        geom::IsEmpty(state.viewport)) {
        state.canvas = RECT{};
        state.scale = 1.0;
        return;
    }

    if (state.fitToWindow) {
        state.scale = geom::FitScale(state.image->Width(), state.image->Height(),
                                     static_cast<int>(geom::Width(state.viewport)),
                                     static_cast<int>(geom::Height(state.viewport)));
        state.zoom = state.scale;
        state.pan = POINT{0, 0};
    } else {
        state.scale = state.zoom;
    }

    ClampPan(state);
    state.canvas = geom::CanvasRect(state.viewport, state.image->Width(),
                                    state.image->Height(), state.scale, state.pan);
}

POINT ToImage(const State& state, POINT client) noexcept {
    return geom::ViewToImage(client, POINT{state.canvas.left, state.canvas.top},
                             state.scale);
}

POINT ToClient(const State& state, POINT image) noexcept {
    return geom::ImageToView(image, POINT{state.canvas.left, state.canvas.top},
                             state.scale);
}

RECT ToClientRect(const State& state, const RECT& image) noexcept {
    const POINT topLeft = ToClient(state, POINT{image.left, image.top});
    const POINT bottomRight = ToClient(state, POINT{image.right, image.bottom});
    return RECT{topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
}

void ZoomAt(HWND window, State& state, double factor, POINT anchor) {
    if (state.image == nullptr || !state.image->Valid() || factor <= 0.0) {
        return;
    }

    const double previous = state.scale;
    const double target = geom::ClampZoom(previous * factor, kMinZoom, kMaxZoom);
    if (::fabs(target - previous) < 0.0001) {
        return;
    }

    state.pan = geom::PanForZoomAnchor(anchor, state.viewport,
                                       state.image->Width(),
                                       state.image->Height(), previous, state.pan,
                                       target);
    state.fitToWindow = false;
    state.zoom = target;

    RECT client{};
    ::GetClientRect(window, &client);
    LayoutCanvas(state, client);
    ::InvalidateRect(window, nullptr, FALSE);
}

void ApplyZoom(HWND window, State& state, double zoom) {
    const POINT centre{(state.viewport.left + state.viewport.right) / 2,
                       (state.viewport.top + state.viewport.bottom) / 2};
    if (state.scale <= 0.0) {
        return;
    }
    ZoomAt(window, state, zoom / state.scale, centre);
}

void ZoomToFit(HWND window, State& state) {
    state.fitToWindow = true;
    state.pan = POINT{0, 0};
    RECT client{};
    ::GetClientRect(window, &client);
    LayoutCanvas(state, client);
    ::InvalidateRect(window, nullptr, FALSE);
}

void ZoomToActual(HWND window, State& state) {
    state.fitToWindow = false;
    state.zoom = 1.0;
    state.pan = POINT{0, 0};
    RECT client{};
    ::GetClientRect(window, &client);
    LayoutCanvas(state, client);
    ::InvalidateRect(window, nullptr, FALSE);
}

}  // namespace editor
}  // namespace crisp
