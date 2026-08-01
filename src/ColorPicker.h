// ColorPicker.h — Renk seçici panel.
//
// NEDEN ChooseColorW DEĞİL: Windows'un renk iletişim kutusu koyu temaya uymaz
// (MessageBoxW ile aynı sorun), 1990'ların özel renk ızgarasını gösterir ve
// hex girişi yoktur. Kendi panelimiz koyu/açık temayı izler, doygunluk-parlaklık
// karesi ile ton şeridini canlı gösterir, hex kabul eder ve ekrandan renk
// almak için zaten var olan kaplamayı kullanır.
//
// PANEL, İLETİŞİM KUTUSU DEĞİL: bir düğmenin ALTINA açılır ve kendi mesaj
// döngüsünü işletir. Hazır bir renk örneğine tıklamak paneli anında kapatır —
// tek tıkla biten bir seçim için "Tamam"a basmak fazladan bir adımdır.
#pragma once

#include "Settings.h"

#include <vector>

#include <windows.h>

namespace crisp {

// `anchor` EKRAN koordinatında, panelin altına açılacağı düğmedir.
// Kullanıcı bir renk seçtiyse true döner, `color` güncellenir ve renk
// `recent` listesinin başına alınır.
[[nodiscard]] bool PickColor(HINSTANCE instance, HWND owner, const RECT& anchor,
                             const Settings& settings, COLORREF& color,
                             std::vector<COLORREF>& recent);

}  // namespace crisp
