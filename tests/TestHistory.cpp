// TestHistory.cpp — Yakalama geçmişi: yazma, sıralama, budama.
#include "TestFramework.h"

#include "History.h"
#include "ImageCodec.h"
#include "Util.h"

#include <string>

using namespace crisp;

namespace {

[[nodiscard]] std::wstring Folder(const wchar_t* leaf) {
    std::wstring path = test::TempDirectory();
    path += L"\\history-";
    path += leaf;
    return path;
}

[[nodiscard]] HistoryStore Fresh(const wchar_t* leaf) {
    HistoryStore store;
    store.SetFolder(Folder(leaf));
    store.Clear();   // önceki koşudan kalan varsa temizle
    return store;
}

[[nodiscard]] Image Solid(int width, int height, uint32_t color) {
    Image image;
    if (image.Create(width, height)) {
        image.Fill(color);
    }
    return image;
}

// Dosyanın yazılma zamanını elle ayarlar.
//
// GEREKLİ: Record() ardışık çağrıldığında dosyalar aynı FILETIME değerini
// alabilir ve sıralamayı ölçemezdik. Zamanı kendimiz koyunca test, saatin
// çözünürlüğüne değil kodun sıralamasına bakar.
void StampTime(const std::wstring& path, uint64_t ticks) {
    const HANDLE file =
        ::CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    ULARGE_INTEGER value;
    value.QuadPart = ticks;
    FILETIME stamp;
    stamp.dwLowDateTime = value.LowPart;
    stamp.dwHighDateTime = value.HighPart;
    ::SetFileTime(file, nullptr, nullptr, &stamp);
    ::CloseHandle(file);
}

// 2020-01-01 civarı, saniye aralıklı sahte zamanlar.
constexpr uint64_t kBaseTicks = 132230000000000000ull;
constexpr uint64_t kSecond = 10000000ull;

}  // namespace

CRISP_TEST(History, Bos_klasor_bos_liste) {
    HistoryStore store = Fresh(L"empty");
    CHECK(store.List().empty());
    CHECK_EQ(store.Prune(5), 0);
}

CRISP_TEST(History, Klasor_belirtilmezse_yazmaz) {
    HistoryStore store;
    std::wstring written;
    CHECK(!store.Record(Solid(4, 4, 0xFF000000u), written));
    CHECK(written.empty());
}

CRISP_TEST(History, Gecersiz_goruntu_yazmaz) {
    HistoryStore store = Fresh(L"invalid");
    const Image empty;
    std::wstring written;
    CHECK(!store.Record(empty, written));
}

CRISP_TEST(History, Yazilan_dosya_geri_okunabilir) {
    HistoryStore store = Fresh(L"roundtrip");
    std::wstring written;
    CHECK(store.Record(Solid(9, 7, 0xFF1E90FFu), written));
    CHECK(!written.empty());

    Image loaded;
    CHECK(LoadPng(written, loaded));
    CHECK_EQ(loaded.Width(), 9);
    CHECK_EQ(loaded.Height(), 7);
    CHECK_EQ(loaded.Pixel(4, 3), 0xFF1E90FFu);

    CHECK_EQ(store.List().size(), 1);
    store.Clear();
}

CRISP_TEST(History, En_yeni_once_siralanir) {
    HistoryStore store = Fresh(L"order");
    std::wstring paths[4];
    for (int i = 0; i < 4; ++i) {
        CHECK(store.Record(Solid(4, 4, 0xFF000000u), paths[i]));
        StampTime(paths[i], kBaseTicks + static_cast<uint64_t>(i) * kSecond);
    }

    const std::vector<HistoryEntry> list = store.List();
    CHECK_EQ(list.size(), 4);
    if (list.size() == 4) {
        // 3 en yeni damgayı taşıyor, 0 en eskisini.
        CHECK_STR(list[0].path, paths[3]);
        CHECK_STR(list[3].path, paths[0]);
    }
    store.Clear();
}

CRISP_TEST(History, Budama_en_yenileri_birakir) {
    HistoryStore store = Fresh(L"prune");
    std::wstring paths[5];
    for (int i = 0; i < 5; ++i) {
        CHECK(store.Record(Solid(4, 4, 0xFF000000u), paths[i]));
        StampTime(paths[i], kBaseTicks + static_cast<uint64_t>(i) * kSecond);
    }

    CHECK_EQ(store.Prune(2), 3);
    const std::vector<HistoryEntry> list = store.List();
    CHECK_EQ(list.size(), 2);
    if (list.size() == 2) {
        CHECK_STR(list[0].path, paths[4]);
        CHECK_STR(list[1].path, paths[3]);
    }
    // En eski dosyalar gerçekten diskten gitmiş olmalı; listeden düşmesi yetmez.
    CHECK_EQ(::GetFileAttributesW(paths[0].c_str()), INVALID_FILE_ATTRIBUTES);
    store.Clear();
}

CRISP_TEST(History, Sinir_asilinca_kendiliginden_budanir) {
    HistoryStore store = Fresh(L"limit");
    store.SetLimit(3);
    std::wstring written;
    for (int i = 0; i < 6; ++i) {
        CHECK(store.Record(Solid(4, 4, 0xFF000000u), written));
        StampTime(written, kBaseTicks + static_cast<uint64_t>(i) * kSecond);
    }
    CHECK_EQ(store.List().size(), 3);
    store.Clear();
}

CRISP_TEST(History, Tek_kayit_silinir) {
    HistoryStore store = Fresh(L"remove");
    std::wstring first;
    std::wstring second;
    CHECK(store.Record(Solid(4, 4, 0xFF000000u), first));
    StampTime(first, kBaseTicks);
    CHECK(store.Record(Solid(4, 4, 0xFFFFFFFFu), second));
    StampTime(second, kBaseTicks + kSecond);

    CHECK(store.Remove(first));
    CHECK(!store.Remove(first));   // ikinci kez başarısız olmalı
    CHECK(!store.Remove(std::wstring()));

    const std::vector<HistoryEntry> list = store.List();
    CHECK_EQ(list.size(), 1);
    if (!list.empty()) {
        CHECK_STR(list[0].path, second);
    }
    store.Clear();
}

CRISP_TEST(History, Temizleme_hepsini_siler) {
    HistoryStore store = Fresh(L"clear");
    std::wstring written;
    for (int i = 0; i < 3; ++i) {
        CHECK(store.Record(Solid(4, 4, 0xFF000000u), written));
    }
    CHECK_EQ(store.Clear(), 3);
    CHECK(store.List().empty());
}

CRISP_TEST(History, Girdi_adinda_uzanti_olmaz) {
    HistoryStore store = Fresh(L"names");
    std::wstring written;
    CHECK(store.Record(Solid(4, 4, 0xFF000000u), written));

    const std::vector<HistoryEntry> list = store.List();
    CHECK_EQ(list.size(), 1);
    if (!list.empty()) {
        CHECK(list[0].name.find(L".png") == std::wstring::npos);
        CHECK(list[0].bytes > 0);
    }
    store.Clear();
}

CRISP_TEST(History, Varsayilan_klasor_yerel_uygulama_verisinde) {
    const std::wstring folder = HistoryStore::DefaultFolder();
    CHECK(!folder.empty());
    CHECK(folder.find(L"\\Crisp\\History") != std::wstring::npos);
}
