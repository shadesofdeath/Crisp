// EditorInput.cpp — Düzenleyicinin mesaj işleme ve pencere ömrü. Yerleşim ve
// çizim EditorWindow.cpp'de.
#include "EditorInternal.h"

#include "Geometry.h"
#include "Localization.h"
#include "Theme.h"
#include "Util.h"
#include "resource.h"

#include <shellscalingapi.h>
#include <windowsx.h>

#include <algorithm>
#include <string>

namespace crisp {
namespace editor {
namespace {

constexpr const wchar_t* kWindowClass = L"CrispEditorWindow";

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

void ApplyAction(HWND window, State& state, int action) {
    // Bir eylem her zaman önce yazılmakta olan metni kesinleştirir; yoksa
    // kullanıcı yazarken "kaydet"e bastığında yazdığı kaybolurdu.
    CommitTextDraft(state);

    switch (action) {
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
    if (usable) {
        state.document.AddShape(state.draft);
        Rebuild(state);
    }
    state.draft = Shape{};

    RECT client{};
    ::GetClientRect(window, &client);
    LayoutButtons(state, client);
    ::InvalidateRect(window, nullptr, FALSE);
}

LRESULT CALLBACK EditorProc(HWND window, UINT message, WPARAM wParam,
                            LPARAM lParam) {
    auto* state = reinterpret_cast<State*>(::GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
        case WM_NCCREATE: {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            ::SetWindowLongPtrW(window, GWLP_USERDATA,
                                reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return ::DefWindowProcW(window, message, wParam, lParam);
        }

        case WM_SIZE: {
            if (state != nullptr) {
                RECT client{};
                ::GetClientRect(window, &client);
                LayoutButtons(*state, client);
                LayoutCanvas(*state, client);
                ::InvalidateRect(window, nullptr, FALSE);
            }
            return 0;
        }

        case WM_PAINT:
            if (state != nullptr) {
                Paint(window, *state);
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_MOUSEMOVE: {
            if (state == nullptr) {
                break;
            }
            const POINT client{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (state->dragging) {
                UpdateDraw(window, *state, client);
                return 0;
            }
            const int hover = ButtonAt(*state, client);
            if (hover != state->hoverButton) {
                state->hoverButton = hover;
                ::InvalidateRect(window, nullptr, FALSE);
            }
            ::SetCursor(::LoadCursorW(
                nullptr, ::PtInRect(&state->canvas, client) ? IDC_CROSS : IDC_ARROW));
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (state == nullptr) {
                break;
            }
            const POINT client{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            const int index = ButtonAt(*state, client);
            if (index >= 0) {
                const Button button = state->buttons[static_cast<size_t>(index)];
                if (!button.enabled) {
                    return 0;
                }
                switch (button.kind) {
                    case ButtonKind::Tool:
                        CommitTextDraft(*state);
                        state->tool = button.tool;
                        break;
                    case ButtonKind::Color:
                        state->color = button.color;
                        // Yazılmakta olan metnin rengi de anında değişsin.
                        if (state->typing) {
                            state->textDraft.color = button.color;
                        }
                        break;
                    case ButtonKind::Thickness:
                        state->thickness = button.thickness;
                        if (state->typing) {
                            state->textDraft.thickness = button.thickness;
                        }
                        break;
                    case ButtonKind::Action:
                        ApplyAction(window, *state, button.action);
                        return 0;
                }
                ::InvalidateRect(window, nullptr, FALSE);
                return 0;
            }
            BeginDraw(window, *state, client);
            return 0;
        }

        case WM_LBUTTONUP:
            if (state != nullptr) {
                EndDraw(window, *state);
            }
            return 0;

        case WM_CHAR: {
            if (state == nullptr || !state->typing) {
                break;
            }
            const wchar_t ch = static_cast<wchar_t>(wParam);
            if (ch == VK_BACK) {
                if (!state->textDraft.text.empty()) {
                    state->textDraft.text.pop_back();
                }
            } else if (ch == VK_RETURN) {
                CommitTextDraft(*state);
                RECT client{};
                ::GetClientRect(window, &client);
                LayoutButtons(*state, client);
            } else if (ch >= L' ') {
                state->textDraft.text.push_back(ch);
            }
            ::InvalidateRect(window, nullptr, FALSE);
            return 0;
        }

        case WM_KEYDOWN: {
            if (state == nullptr) {
                break;
            }
            const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;

            if (wParam == VK_ESCAPE) {
                if (state->typing) {
                    // İlk Esc yazmayı iptal eder, pencereyi kapatmaz: kullanıcı
                    // bir harfi yanlış yazdı diye tüm düzenlemeyi kaybetmemeli.
                    state->typing = false;
                    state->textDraft = Shape{};
                    ::InvalidateRect(window, nullptr, FALSE);
                    return 0;
                }
                ::DestroyWindow(window);
                return 0;
            }
            if (control && wParam == 'Z') {
                ApplyAction(window, *state, kActionUndo);
                return 0;
            }
            if (control && (wParam == 'Y' || (wParam == 'Z' && (::GetKeyState(VK_SHIFT) & 0x8000) != 0))) {
                ApplyAction(window, *state, kActionRedo);
                return 0;
            }
            if (control && wParam == 'C') {
                ApplyAction(window, *state, kActionCopy);
                return 0;
            }
            if (control && wParam == 'S') {
                ApplyAction(window, *state, kActionSave);
                return 0;
            }
            break;
        }

        case WM_DPICHANGED: {
            if (state != nullptr) {
                state->dpi = HIWORD(wParam);
                const auto* suggested = reinterpret_cast<const RECT*>(lParam);
                ::SetWindowPos(window, nullptr, suggested->left, suggested->top,
                               static_cast<int>(geom::Width(*suggested)),
                               static_cast<int>(geom::Height(*suggested)),
                               SWP_NOZORDER | SWP_NOACTIVATE);
            }
            return 0;
        }

        // Kullanıcı pencereyi araç çubuğunun sığmayacağı kadar daraltamasın.
        case WM_GETMINMAXINFO: {
            if (state != nullptr) {
                auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
                info->ptMinTrackSize.x =
                    RequiredToolbarWidth(state->dpi) + Scale(20, state->dpi);
                info->ptMinTrackSize.y = Scale(kToolbarHeight + 160, state->dpi);
            }
            return 0;
        }

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    return ::DefWindowProcW(window, message, wParam, lParam);
}

[[nodiscard]] bool EnsureWindowClass(HINSTANCE instance) {
    static bool registered = false;
    if (registered) {
        return true;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = EditorProc;
    wc.hInstance = instance;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClass;
    wc.hIcon = ::LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
    wc.hbrBackground = nullptr;

    registered = ::RegisterClassExW(&wc) != 0;
    return registered;
}

}  // namespace
}  // namespace editor

EditorResult RunEditor(HINSTANCE instance, const Settings& settings, Image& image) {
    using namespace editor;

    EditorResult result{};
    if (!image.Valid() || !EnsureWindowClass(instance)) {
        return result;
    }

    State state;
    state.image = &image;
    state.settings = settings;
    if (!CropImage(image, 0, 0, image.Width(), image.Height(), state.original)) {
        return result;
    }

    POINT cursor{};
    ::GetCursorPos(&cursor);
    const HMONITOR monitor = ::MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    RECT work{0, 0, 1920, 1080};
    if (::GetMonitorInfoW(monitor, &info)) {
        work = info.rcWork;
    }

    UINT dpiX = 96;
    UINT dpiY = 96;
    if (SUCCEEDED(::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
        state.dpi = dpiX;
    }

    // Pencere görüntüyü sarar ama ekranın dışına taşmaz; büyük bir yakalama
    // çalışma alanına sığdırılır ve tuval küçültülerek gösterilir.
    const int chromeWidth = Scale(24, state.dpi);
    const int chromeHeight = Scale(kToolbarHeight + 24, state.dpi);
    int width = image.Width() + chromeWidth;
    int height = image.Height() + chromeHeight;

    // ARAÇ ÇUBUĞU SIĞMALI. Gerekli genişlik hesaplanır, tahmin edilmez:
    // sabit bir sayı bir sonraki araç eklendiğinde sessizce yetersiz kalır ve
    // eylem düğmeleri renk örneklerinin üstüne biner.
    width = (std::max)(width, RequiredToolbarWidth(state.dpi));
    width = (std::min)(width, static_cast<int>(geom::Width(work)));
    height = (std::min)(height, static_cast<int>(geom::Height(work)));

    const int x = work.left + (static_cast<int>(geom::Width(work)) - width) / 2;
    const int y = work.top + (static_cast<int>(geom::Height(work)) - height) / 2;

    const std::wstring title = Loc::Str(IDS_EDITOR_TITLE);
    const HWND window = ::CreateWindowExW(
        0, kWindowClass, title.c_str(), WS_OVERLAPPEDWINDOW, x, y, width, height,
        nullptr, nullptr, instance, &state);
    if (window == nullptr) {
        LogV(L"Düzenleyici penceresi oluşturulamadı (hata %lu)", ::GetLastError());
        return result;
    }

    theme::ApplyToWindow(window);
    ::ShowWindow(window, SW_SHOW);
    ::SetForegroundWindow(window);

    MSG message{};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }

    return state.result;
}

}  // namespace crisp
