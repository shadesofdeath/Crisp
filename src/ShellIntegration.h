// ShellIntegration.h — Explorer'ın sağ tık menüsüne "Crisp ile düzenle".
//
// KABUK UZANTISI DEĞİL, KAYIT DEFTERİ FİİLİ. Bir IExplorerCommand işleyicisi
// yazmak bir COM DLL'i, kayıt ve ömür yönetimi demek; üstelik Explorer o DLL'i
// süreç içine yükler ve bir çökme dosya gezginini de götürür. Bir "fiil" ise
// yalnızca birkaç kayıt defteri değeridir: Explorer exe'yi dosya yoluyla
// başlatır, biz de onu zaten komut satırından açabiliyoruz.
//
// HKEY_CURRENT_USER ALTINA: yönetici hakkı istemez ve taşınabilir kip için de
// doğru olan budur — bir kullanıcının kaydı diğerini etkilemez.
//
// SystemFileAssociations\image, Windows'un "algılanan tür"üdür: PNG, JPEG,
// BMP, GIF, TIFF, WebP ve HEIF tek bir anahtarla kapsanır. Uzantı uzantı
// kaydetmek, listeyi Windows'un desteklediği biçimlerle senkron tutma
// sorumluluğunu bize yıkardı.
#pragma once

#include <string>

#include <windows.h>

namespace crisp {

// Fiil adı. Testler kendi adlarını verip gerçek kaydı bozmadan çalışır.
inline constexpr const wchar_t* kEditVerb = L"Crisp.Edit";

[[nodiscard]] bool IsShellMenuRegistered(const wchar_t* verb = kEditVerb);

// Kayıtlı komut BU exe'yi mi gösteriyor? Taşınabilir sürüm başka bir klasöre
// kopyalandığında eski kayıt sessizce bozuk bir menü öğesi bırakırdı.
[[nodiscard]] bool ShellMenuPathIsCurrent(const wchar_t* verb = kEditVerb);

// `label` menüde görünecek metindir; çağıran onu kullanıcının dilinde verir.
[[nodiscard]] bool RegisterShellMenu(const std::wstring& label,
                                     const wchar_t* verb = kEditVerb);

[[nodiscard]] bool UnregisterShellMenu(const wchar_t* verb = kEditVerb);

}  // namespace crisp
