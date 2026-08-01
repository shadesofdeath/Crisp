// NameFormat.h — Dosya adı şablonu.
//
// NEDEN ŞABLON: dosya adı bugüne kadar kodda sabitti ("Crisp <zaman damgası>")
// ve her yakalama tek bir klasöre yığılıyordu. Bir ay çalıştıktan sonra o
// klasörde binlerce dosya olur ve hiçbiri neyin ekran görüntüsü olduğunu
// söylemez. Şablon, adı ve alt klasörü kullanıcının kararına bırakır.
//
// PENCERE YOK, AYAR YOK: saf dize dönüşümü. "%y-%mo doğru ayı veriyor mu"
// sorusu bir ekran görüntüsü alarak değil, bir testle yanıtlanmalı.
#pragma once

#include <string>

#include <windows.h>

namespace crisp {

// Şablonun çözüleceği bağlam. Zaman DIŞARIDAN verilir: fonksiyonun içinde
// GetLocalTime çağırmak testi saate bağımlı kılardı.
struct NameContext {
    SYSTEMTIME time{};
    std::wstring windowTitle;    // %pn — yakalanan pencerenin başlığı
    int width = 0;               // %w
    int height = 0;              // %h
    unsigned counter = 1;        // %i — oturum sayacı
    unsigned random = 0;         // %ra — çağıranın ürettiği tohum
};

// Desteklenen belirteçler:
//   %y  yıl (4 hane)      %yy yıl (2 hane)
//   %mo ay                %d  gün
//   %h  saat              %mi dakika        %s  saniye
//   %w  genişlik          %h... (dikkat: %h saat, %px genişlik değil)
//   %pn pencere başlığı   %i  sayaç (4 hane)  %ra rastgele 6 karakter
//   %un kullanıcı adı     %cn bilgisayar adı
//   %%  tek yüzde işareti
//
// BİLİNMEYEN BELİRTEÇ OLDUĞU GİBİ KALIR: "%q" yazan kullanıcı adında "%q"
// görür ve yazım hatasını fark eder. Sessizce silmek, kaybolan bir parçayı
// aramasına yol açardı.
[[nodiscard]] std::wstring ExpandNameFormat(const std::wstring& format,
                                            const NameContext& context);

// Windows dosya adlarında yasak karakterleri '-' ile değiştirir, sondaki
// nokta ve boşlukları atar. Ters bölü KORUNMAZ: alt klasör ayracı olarak
// kullanılabilmesi için çağıran onu ayrı ele alır.
[[nodiscard]] std::wstring SanitizeFileName(const std::wstring& name);

// Alt klasör şablonu için: her yol parçası ayrı ayrı temizlenir, boş parçalar
// atılır ve sonuç ".." içeremez — bir şablonun kayıt klasörünün dışına
// yazabilmesi olurdu.
[[nodiscard]] std::wstring SanitizeRelativePath(const std::wstring& path);

}  // namespace crisp
