// EditorInput.cpp — Düzenleyicinin mesaj işleme. Yerleşim EditorLayout.cpp'de,
// çizim EditorWindow.cpp'de.
#include "EditorInternal.h"

#include "Geometry.h"
#include "Messages.h"

#include <shellapi.h>
#include <shellscalingapi.h>
#include <windowsx.h>

#include <algorithm>
#include <iterator>

namespace crisp {
namespace editor {
namespace {

void OnTimer(HWND window, State& state, UINT_PTR id) {
    switch (id) {
        case kTooltipTimer:
            ShowTooltipNow(window, state);
            return;
        case kFlashTimer:
            ::KillTimer(window, kFlashTimer);
            state.flashText.clear();
            ::InvalidateRect(window, nullptr, FALSE);
            return;
        case kCaretTimer:
            if (!state.typing) {
                ::KillTimer(window, kCaretTimer);
                state.caretOn = true;
                return;
            }
            state.caretOn = !state.caretOn;
            ::InvalidateRect(window, nullptr, FALSE);
            return;
        default:
            return;
    }
}

void OnButtonClick(HWND window, State& state, int index, POINT client) {
    const Button button = state.buttons[static_cast<size_t>(index)];
    if (!button.enabled) {
        return;
    }
    switch (button.kind) {
        case ButtonKind::Tool:
            PickTool(window, state, button.tool);
            return;
        case ButtonKind::Color:
            OpenColorPicker(window, state, button);
            return;
        case ButtonKind::Thickness:
            OpenThicknessPicker(window, state, button);
            return;
        case ButtonKind::Action:
            ApplyAction(window, state, button.action);
            return;
        case ButtonKind::ZoomSlider:
            state.zoomDragging = true;
            ::SetCapture(window);
            ZoomFromSlider(window, state, client.x);
            return;
    }
    ::InvalidateRect(window, nullptr, FALSE);
}

}  // namespace

// ADSIZ AD ALANINDA DEĞİL: pencere sınıfını kaydeden EditorSession.cpp bu
// yordamı adresiyle kullanıyor.
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
            if (state != nullptr) {
                OnTimer(window, *state, wParam);
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

        // Tutamağın üstünde boyutlandırma imleci; gerekçesi ve eşlemesi
        // EditorSelect.cpp icinde, tutamaklarin kendisiyle birlikte.
        case WM_SETCURSOR: {
            if (state == nullptr || LOWORD(lParam) != HTCLIENT) {
                break;
            }
            const HCURSOR shape = SelectCursor(window, *state);
            if (shape == nullptr) {
                break;
            }
            ::SetCursor(shape);
            return TRUE;
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

            if (state->zoomDragging) {
                ZoomFromSlider(window, *state, client.x);
                return 0;
            }

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

            if (SelectMouseMove(window, *state, client)) {
                return 0;
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
                OnButtonClick(window, *state, index, client);
                return 0;
            }
            if (OcrMouseDown(window, *state, client)) {
                return 0;
            }
            if (SelectMouseDown(window, *state, client)) {
                return 0;
            }
            BeginDraw(window, *state, client);
            return 0;
        }

        case WM_LBUTTONUP:
            if (state != nullptr) {
                if (state->zoomDragging) {
                    state->zoomDragging = false;
                    if (::GetCapture() == window) {
                        ::ReleaseCapture();
                    }
                    return 0;
                }
                if (OcrMouseUp(window, *state)) {
                    return 0;
                }
                if (SelectMouseUp(window, *state)) {
                    return 0;
                }
                EndDraw(window, *state);
            }
            return 0;

        case WM_CHAR:
            if (state != nullptr && TextTypingChar(window, *state,
                                                   static_cast<wchar_t>(wParam))) {
                return 0;
            }
            break;

        case WM_KEYDOWN:
            if (state != nullptr && OnKeyDown(window, *state, wParam)) {
                return 0;
            }
            break;

        // SÜRÜKLE-BIRAK: bir görüntü dosyası düzenleyiciye bırakıldığında
        // taban olur. Geri alınabilir bir işlem: kullanıcı yanlış dosyayı
        // bıraktıysa Ctrl+Z eski yakalamayı geri getirir.
        case WM_DROPFILES: {
            if (state == nullptr) {
                break;
            }
            const auto drop = reinterpret_cast<HDROP>(wParam);
            wchar_t path[MAX_PATH] = L"";
            if (::DragQueryFileW(drop, 0, path, static_cast<UINT>(std::size(path))) > 0) {
                OpenDroppedImage(window, *state, path);
            }
            ::DragFinish(drop);
            return 0;
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
                info->ptMinTrackSize.x = RequiredToolbarWidth(state->dpi);
                info->ptMinTrackSize.y =
                    Scale(kToolbarHeight + kStatusHeight + 160, state->dpi);
            }
            return 0;
        }

        // Arka plandaki yükleme bitti. `lParam` sonucu taşıyan bir yükün adresi
        // ve sahipliği burada devralınır; ayrıntısı EditorUpload.cpp'de.
        case WM_CRISP_UPLOAD_DONE:
            if (state != nullptr) {
                FinishUpload(window, *state, lParam);
            }
            return 0;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;

        default:
            break;
    }

    return ::DefWindowProcW(window, message, wParam, lParam);
}

}  // namespace editor
}  // namespace crisp
