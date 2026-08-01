// AboutInternal.h — Hakkında penceresinin İÇ paylaşımı.
//
// AYRI BAŞLIK: durum yapısı hem pencere yordamının hem çizimin işine yarıyor
// ve ikisi ayrı dosyada (AboutWindow.cpp ev kuralının 400 satır sınırını
// aşıyordu, docs §9).
#pragma once

#include <windows.h>

namespace crisp {
namespace about {

// Tasarım ölçüleri (96 DPI mantıksal piksel).
inline constexpr int kWidth = 460;
inline constexpr int kHeight = 320;
inline constexpr int kPad = 24;
inline constexpr int kIconSide = 72;

inline constexpr const wchar_t* kRepositoryUrl =
    L"https://github.com/shadesofdeath/Crisp";
inline constexpr const wchar_t* kVersionText = L"0.3.0";

struct AboutState {
    HICON icon = nullptr;
    unsigned dpi = 96;
    RECT linkRect{};    // GitHub bağlantısının tıklanabilir alanı
    RECT closeRect{};
    bool linkHot = false;
    bool closeHot = false;
};

[[nodiscard]] int Scale(int value, unsigned dpi) noexcept;
void Paint(HWND window, AboutState& state);

}  // namespace about
}  // namespace crisp
