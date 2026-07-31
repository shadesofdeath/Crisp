// EditorInput.cpp — Düzenleyicinin mesaj işleme ve pencere ömrü. Yerleşim ve
// çizim EditorWindow.cpp'de.
#include "EditorInternal.h"

#include "Geometry.h"
#include "EditorRender.h"
#include "ImageTransform.h"
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

        case WM_TIMER:
            if (state != nullptr && wParam == kTooltipTimer) {
                ShowTooltipNow(window, *state);
            }
            return 0;

        case WM_MOUSELEAVE:
            if (state != nullptr) {
                HideTooltip(window, *state);
            }
            return 0;

        // Tekerlek YAKINLAŞTIRIR, kaydırmaz: kaydırma çubuğu olmayan bir
        // tuvalde tekerleğin kaydırması, kullanıcının görüntüyü nereye
        // ittiğini göremediği bir hareket olurdu.
        case WM_MOUSEWHEEL: {
            if (state == nullptr) {
                break;
            }
            POINT cursor{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ::ScreenToClient(window, &cursor);
            const int notches = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            if (notches != 0) {
                ZoomAt(window, *state, notches > 0 ? 1.15 : (1.0 / 1.15), cursor);
            }
            return 0;
        }

        // Orta tuş SÜRÜKLER. Sol tuş çizim aracına ait olduğu için kaydırma
        // ona bindirilemezdi; orta tuş her fare aygıtında var ve boşta.
        case WM_MBUTTONDOWN: {
            if (state == nullptr) {
                break;
            }
            state->panning = true;
            state->panGrab = POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            state->panStart = state->pan;
            ::SetCapture(window);
            return 0;
        }

        case WM_MBUTTONUP:
            if (state != nullptr && state->panning) {
                state->panning = false;
                if (::GetCapture() == window) {
                    ::ReleaseCapture();
                }
            }
            return 0;

        case WM_MOUSEMOVE: {
            if (state == nullptr) {
                break;
            }
            const POINT client{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};

            if (state->panning) {
                state->pan.x = state->panStart.x + (client.x - state->panGrab.x);
                state->pan.y = state->panStart.y + (client.y - state->panGrab.y);
                RECT area{};
                ::GetClientRect(window, &area);
                LayoutCanvas(*state, area);
                ::SetCursor(::LoadCursorW(nullptr, IDC_SIZEALL));
                ::InvalidateRect(window, nullptr, FALSE);
                return 0;
            }

            // İmlecin görüntü koordinatı durum çubuğunda gösterilir; tuval
            // dışındayken -1 ile gizlenir.
            const POINT image = ToImage(*state, client);
            const POINT shown =
                ::PtInRect(&state->canvas, client) ? image : POINT{-1, -1};
            if (shown.x != state->hoverImage.x || shown.y != state->hoverImage.y) {
                state->hoverImage = shown;
                ::InvalidateRect(window, nullptr, FALSE);
            }

            if (state->dragging) {
                UpdateDraw(window, *state, client);
                return 0;
            }
            const int hover = ButtonAt(*state, client);
            if (hover != state->hoverButton) {
                state->hoverButton = hover;
                ::InvalidateRect(window, nullptr, FALSE);
            }
            UpdateTooltipHover(window, *state, hover);

            // WM_MOUSELEAVE KENDİLİĞİNDEN GELMEZ: her hareket sonrası yeniden
            // istenmeli. Onsuz, fare pencereden çıktığında ipucu ekranda asılı
            // kalırdı.
            TRACKMOUSEEVENT track{};
            track.cbSize = sizeof(track);
            track.dwFlags = TME_LEAVE;
            track.hwndTrack = window;
            ::TrackMouseEvent(&track);
            if (hover < 0 && OcrMouseMove(window, *state, client)) {
                return 0;
            }
            ::SetCursor(::LoadCursorW(
                nullptr, ::PtInRect(&state->canvas, client) ? IDC_CROSS : IDC_ARROW));
            return 0;
        }

        case WM_RBUTTONUP: {
            if (state == nullptr || !state->ocr.active) {
                break;
            }
            POINT screen{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ::ClientToScreen(window, &screen);
            OcrShowMenu(window, *state, screen);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (state == nullptr) {
                break;
            }
            const POINT client{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            HideTooltip(window, *state);
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
            if (OcrMouseDown(window, *state, client)) {
                return 0;
            }
            BeginDraw(window, *state, client);
            return 0;
        }

        case WM_LBUTTONUP:
            if (state != nullptr) {
                if (OcrMouseUp(window, *state)) {
                    return 0;
                }
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

            // OCR kipi tuşları ÖNCE görür: açıkken Ctrl+C seçili METNİ
            // kopyalamalı, görüntüyü değil.
            if (OcrKeyDown(window, *state, static_cast<unsigned>(wParam),
                           control)) {
                return 0;
            }

            if (control && (wParam == VK_OEM_PLUS || wParam == VK_ADD)) {
                ApplyAction(window, *state, kActionZoomIn);
                return 0;
            }
            if (control && (wParam == VK_OEM_MINUS || wParam == VK_SUBTRACT)) {
                ApplyAction(window, *state, kActionZoomOut);
                return 0;
            }
            if (control && wParam == '0') {
                ZoomToFit(window, *state);
                return 0;
            }

            // SAYI TUŞLARI ARAÇ SEÇER. On aracın hepsine fareyle gitmek,
            // işaretlerken elin sürekli araç çubuğuna dönmesi demekti.
            // Metin yazılırken devre dışı: o sırada rakam metnin parçasıdır.
            if (!control && !state->typing && wParam >= '0' && wParam <= '9') {
                constexpr ToolKind kOrder[] = {
                    ToolKind::Crop,        ToolKind::Arrow,
                    ToolKind::Rectangle,   ToolKind::Ellipse,
                    ToolKind::Pen,         ToolKind::Highlighter,
                    ToolKind::Text,        ToolKind::StepNumber,
                    ToolKind::Blur,        ToolKind::Mosaic};
                state->tool = kOrder[wParam - '0'];
                state->ocr.active = false;
                RECT area{};
                ::GetClientRect(window, &area);
                LayoutButtons(*state, area);
                ::InvalidateRect(window, nullptr, FALSE);
                return 0;
            }

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
    state.instance = instance;
    state.image = &image;
    state.settings = settings;

    auto base = std::make_shared<Image>();
    if (!CropImage(image, 0, 0, image.Width(), image.Height(), *base)) {
        return result;
    }
    state.document.SetBase(base);

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
