// HotkeyEdit.h — Tuş bileşimi yakalayan metin kutusu.
//
// NEDEN msctls_hotkey32 DEĞİL: Windows'un hazır kısayol denetimi zeminini ve
// metnini sistem renkleriyle KENDİSİ çizer; SetWindowTheme ve WM_CTLCOLOR*
// ona işlemez. Koyu bir pencerenin ortasında bembeyaz dört kutu kalıyordu.
// Sıradan bir EDIT'i alt sınıflayınca hem tema kendiliğinden doğru olur hem
// de "Ctrl + Shift + S" gibi okunur bir metin gösterebiliriz — hazır denetim
// bunu kendi biçiminde yazar ve çeviremeyiz.
#pragma once

#include "Settings.h"

#include <string>

#include <windows.h>

namespace crisp {

// Var olan bir EDIT'i kısayol kutusuna dönüştürür.
void MakeHotkeyEdit(HWND control);

void SetHotkeyValue(HWND control, const Hotkey& hotkey);
[[nodiscard]] Hotkey GetHotkeyValue(HWND control) noexcept;

// "Ctrl + Shift + S"; atanmamışsa uzun tire.
[[nodiscard]] std::wstring HotkeyText(const Hotkey& hotkey);

}  // namespace crisp
