// MessageWindow.h — Uygulamanın kendi ileti kutusu.
//
// NEDEN MessageBoxW DEĞİL: Windows'un ileti kutusu KOYU TEMAYA UYMAZ. Uygulama
// koyu bir tepsi menüsü, koyu bir düzenleyici ve koyu bir ayarlar penceresi
// gösterip ardından bembeyaz bir sistem kutusu açtığında, o kutu başka bir
// programdan fırlamış gibi duruyor. SetWindowTheme ile de düzelmez: kutuyu
// çizen comctl32'nin kendi kodu ve dışarıdan verilebilecek bir renk yok.
//
// Kapsam bilinçli olarak dar: bir simge, sarılabilen bir metin ve en fazla iki
// düğme. Sistem kutusunun geri kalanı (yardım düğmesi, üç düğmeli biçimler,
// zaman aşımı) bu uygulamada hiç kullanılmıyordu.
#pragma once

#include <string>

#include <windows.h>

namespace crisp {

enum class MessageIcon {
    Information,
    Warning,
    Error,
    Question,
};

enum class MessageButtons {
    Ok,
    YesNo,
};

enum class MessageResult {
    Ok,
    Yes,
    No,
};

// Kutuyu açar ve kullanıcı kapatana kadar döner.
//
// `owner` verilirse kutu açıkken devre dışı bırakılır ve kutu onun üstünde
// ortalanır — sistem kutusunun kipsel davranışı budur ve arkadaki pencereyle
// etkileşime izin vermek, kullanıcının aynı işlemi iki kez başlatmasına yol
// açardı.
//
// YesNo biçiminde varsayılan düğme HAYIR'dır: bu biçim yalnızca geri
// alınamayan işlemler için kullanılıyor ve Enter'a refleksle basmak veri
// silmemeli.
MessageResult ShowMessage(HINSTANCE instance, HWND owner,
                          const std::wstring& text, MessageIcon icon,
                          MessageButtons buttons = MessageButtons::Ok);

}  // namespace crisp
