// Stitch.h — Üst üste binen kareleri tek bir uzun görüntüde birleştirir.
//
// KAYDIRMALI YAKALAMANIN ZOR YARISI BURASI ve tamamı ağ, pencere ya da ekran
// olmadan çalışır: iki bitmap alır, ikincisinin birincinin neresinden devam
// ettiğini bulur, ve alt alta ekler. Pencereyi kimin kaydırdığı, kaç kare
// alındığı, kullanıcının ne gördüğü — hiçbiri bu dosyanın meselesi değil.
//
// SINANABİLİR OLMASI TESADÜF DEĞİL, TASARIM. Bir hizalama hatası çıplak gözle
// "birleştirme biraz kaymış" diye görünür ve elle ayıklanması saatler alır;
// sentetik bir görüntüyü bilinen bir miktar kaydırıp aynı sayıyı geri
// isteyen bir sınama, aynı hatayı bir saniyede yakalar.
#pragma once

#include "Capture.h"

#include <vector>

namespace crisp {

// İki kare arasındaki DİKEY kaydırma miktarı.
//
// `next`in üst kısmı `previous`ın alt kısmıyla örtüşür; dönen değer, `next`in
// `previous`a göre kaç piksel AŞAĞI kaydığıdır. Bulunamazsa 0.
//
// YALNIZCA DİKEY. Yatay kaydırmalı bir sayfa da var ama ikisini birden aramak
// hem yanlış eşleşme ihtimalini artırıyor hem de kullanım oranı bunu haklı
// çıkarmıyor: kaydırarak yakalanan şey neredeyse her zaman uzun bir sayfa.
//
// `minOverlap` karşılaştırılacak şeridin satır sayısı. Küçük tutulursa
// rastgele benzeyen iki şerit eşleşebilir; büyük tutulursa hızlı kaydırmada
// hiç eşleşme bulunamaz.
//
// ŞERİT KARENİN ÜÇTE BİRİNDEN BAŞLAR, en üstünden değil: seçilen alanın
// üstünde kaydırılmayan bir başlık ya da araç çubuğu olabilir ve sabit bir
// bölgeyi karşılaştırmak yalnızca "hiç kaymamış" cevabını verir. Bu yüzden
// bulunabilecek en büyük kaydırma, karenin üçte ikisinden şerit boyu kadar
// azdır — bir tekerlek adımının çok üstünde.
[[nodiscard]] int FindVerticalShift(const Image& previous, const Image& next,
                                    int minOverlap) noexcept;

// Kareleri sırayla birleştirir. İlk kare tamamen, sonrakiler yalnızca yeni
// kısımları alınarak eklenir.
//
// HİÇBİR KARE ATLANMAZ, ama hiçbiri de İKİ KEZ EKLENMEZ: eşleşme
// bulunamayan bir kare, bir öncekinin devamı sayılamayacağı için birleştirmeyi
// orada BİTİRİR. Yanlış yere eklenmiş bir şerit, eksik bir şeritten çok daha
// kötüdür — ilki sessizce yanlış bir görüntü üretir.
//
// `stopped` doluysa: birleştirme kare bitmeden durdu ve kaçıncı karede
// durduğu yazılır. Çağıran bunu kullanıcıya söyleyebilir.
[[nodiscard]] bool StitchVertical(const std::vector<Image>& frames,
                                  int minOverlap, Image& out,
                                  size_t* stopped = nullptr);

// İki satırın ne kadar benzediği: 0 = birebir aynı, büyüdükçe farklı.
//
// Dışarıda, çünkü eşiğin ne anlama geldiğini sınamak için gerekiyor.
[[nodiscard]] uint64_t RowDifference(const Image& a, int rowA, const Image& b,
                                     int rowB) noexcept;

}  // namespace crisp
