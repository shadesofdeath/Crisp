// History.h — Son yakalamaların diskteki geçmişi.
//
// DİZİN DOSYASI YOKTUR ve bu bilinçlidir. Geçmiş, bir klasördeki PNG'lerin
// kendisidir; sıra dosyaların yazılma zamanından okunur. Ayrı bir dizin tutmak,
// kullanıcı klasörden bir dosyayı sildiğinde ya da uygulama çökerken listeyi
// diskten farklı hâle getirirdi — sonra da "geçmişte duruyor ama açılmıyor"
// diye bir hata sınıfı doğardı.
//
// PENCERE YOK: bu başlık yalnızca dosya sistemine dokunur, bu yüzden konsol
// koşucusundan sınanabilir.
#pragma once

#include "Capture.h"

#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>

namespace crisp {

struct HistoryEntry {
    std::wstring path;
    std::wstring name;   // uzantısız dosya adı; listede etiket olarak kullanılır
    FILETIME written{};
    uint64_t bytes = 0;
};

class HistoryStore {
public:
    // Kaç yakalama saklanır. Küçük ve sabit: geçmiş bir arşiv değil, "az önce
    // ne almıştım" sorusunun cevabıdır. Sınırsız bırakmak, kullanıcının haberi
    // olmadan gigabaytlarca PNG biriktirmek olurdu.
    static constexpr size_t kDefaultLimit = 24;

    HistoryStore() noexcept = default;

    void SetFolder(std::wstring folder) { m_folder = std::move(folder); }
    [[nodiscard]] const std::wstring& Folder() const noexcept { return m_folder; }

    void SetLimit(size_t limit) noexcept { m_limit = limit; }
    [[nodiscard]] size_t Limit() const noexcept { return m_limit; }

    // %LOCALAPPDATA%\Crisp\History — kullanıcının kayıt klasörüne KARIŞMAZ.
    // Geçmiş, kullanıcının kendi dosyalarının arasına serpiştirilmiş bir
    // kopya yığını olsaydı, silmesi de bulması da onun işi olurdu.
    [[nodiscard]] static std::wstring DefaultFolder();

    // Görüntüyü klasöre PNG olarak yazar, sonra sınırın üstünü budar.
    // Yazılan dosyanın yolu `writtenPath`e konur.
    [[nodiscard]] bool Record(const Image& image, std::wstring& writtenPath);

    // En yeniden en eskiye. Klasör yoksa boş liste döner (hata değil).
    [[nodiscard]] std::vector<HistoryEntry> List() const;

    // En yeni `keep` taneyi bırakır, kalanını siler; silinen sayısını döndürür.
    size_t Prune(size_t keep);

    bool Remove(const std::wstring& path);

    // Tümünü siler; silinen dosya sayısını döndürür.
    size_t Clear();

private:
    std::wstring m_folder;
    size_t m_limit = kDefaultLimit;
};

}  // namespace crisp
