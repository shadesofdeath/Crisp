// MessageInternal.h — İleti kutusunun İÇ paylaşımı.
//
// AYRI BAŞLIK: durum yapısı hem pencere yordamının hem çizimin işine yarıyor
// ve ikisi ayrı dosyada (MessageWindow.cpp ev kuralının 400 satır sınırını
// aşıyordu, docs §9).
#pragma once

#include "MessageWindow.h"

#include <string>

#include <windows.h>

namespace crisp {

// Tasarım ölçüleri (96 DPI mantıksal piksel).
inline constexpr int kMsgWidth = 420;
inline constexpr int kMsgPad = 22;
inline constexpr int kMsgIconSide = 34;
inline constexpr int kMsgIconGap = 18;
inline constexpr int kMsgButtonWidth = 104;
inline constexpr int kMsgButtonHeight = 32;
inline constexpr int kMsgButtonGap = 10;
inline constexpr int kMsgMinTextHeight = 34;

enum MessageControlId { kIdPrimary = 100, kIdSecondary };

struct MessageState {
    std::wstring text;
    MessageIcon icon = MessageIcon::Information;
    MessageButtons buttons = MessageButtons::Ok;
    MessageResult result = MessageResult::Ok;
    unsigned dpi = 96;
    int textHeight = 0;
    HFONT font = nullptr;
    HBRUSH background = nullptr;
};

[[nodiscard]] int MsgScale(int value, unsigned dpi) noexcept;
[[nodiscard]] HFONT MsgCreateUiFont(unsigned dpi, int pointSize, int weight);
[[nodiscard]] int MsgMeasureText(const std::wstring& text, int width,
                                 HFONT font);
void MsgPaint(HWND window, const MessageState& state);
void MsgBuildButtons(HWND window, MessageState& state);

}  // namespace crisp
