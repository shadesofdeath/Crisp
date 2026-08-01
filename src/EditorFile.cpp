// EditorFile.cpp — Panoya kopyalama, kaydetme, farklı kaydetme ve durum
// çubuğundaki kısa onay.
//
// NEDEN AYRI DOSYA VE NEDEN PENCERE KAPANMIYOR: eskiden "Kopyala" yalnızca bir
// bayrak koyup pencereyi yok ediyordu; kullanıcının gördüğü tek şey
// düzenleyicinin aniden kaybolmasıydı. Kopyalandı mı, kaydedildiyse nereye —
// hiçbiri söylenmiyordu ve geçmişten düzenleme yolunda bildirim bile yoktu.
// Artık iş burada, pencere AÇIKKEN yapılır ve sonucu durum çubuğunda birkaç
// saniye duran bir satır anlatır.
#include "EditorInternal.h"

#include "ClipboardImage.h"
#include "ImageCodec.h"
#include "Localization.h"
#include "MessageWindow.h"
#include "NameFormat.h"
#include "Util.h"
#include "resource.h"

#include <commdlg.h>

#include <string>
#include <utility>

namespace crisp {
namespace editor {
namespace {

// Onay şeridinin ekranda kalma süresi. Daha kısası okunmadan kaybolur, daha
// uzunu görüntü ölçüsünü gereksiz yere gizler.
constexpr UINT kFlashMs = 2600;

[[nodiscard]] std::wstring FileNameOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L'\\');
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

// "%s: %s" biçiminde birleştirir; kaynak dizeler çeviriden geldiği için
// biçimlendirme dizesi yerine düz birleştirme kullanılır.
[[nodiscard]] std::wstring WithDetail(UINT titleId, const std::wstring& detail) {
    std::wstring text = Loc::Str(titleId);
    if (!detail.empty()) {
        text += L" — ";
        text += detail;
    }
    return text;
}

void WriteAndReport(HWND window, State& state, const std::wstring& path,
                    ImageFormat format) {
    Image flattened;
    if (!CurrentFlattened(state, flattened)) {
        return;
    }
    if (!SaveImage(flattened, path, format, state.settings.saveQuality)) {
        LogV(L"Düzenleyiciden kaydedilemedi: %s", path.c_str());
        ShowMessage(state.instance, window, Loc::Str(IDS_SAVE_FAILED),
                    MessageIcon::Error);
        return;
    }
    state.result.accepted = true;
    state.result.savedPath = path;
    ShowFlash(window, state, WithDetail(IDS_EDITOR_SAVED, FileNameOf(path)));
}

}  // namespace

void ShowFlash(HWND window, State& state, std::wstring text) {
    state.flashText = std::move(text);
    ::SetTimer(window, kFlashTimer, kFlashMs, nullptr);
    ::InvalidateRect(window, nullptr, FALSE);
}

void CopyToClipboard(HWND window, State& state) {
    Image flattened;
    if (!CurrentFlattened(state, flattened)) {
        return;
    }
    if (!CopyImageToClipboard(flattened, window)) {
        LogV(L"Düzenleyiciden panoya kopyalanamadı");
        ShowMessage(state.instance, window, Loc::Str(IDS_COPY_FAILED),
                    MessageIcon::Error);
        return;
    }
    state.result.accepted = true;
    state.result.copied = true;
    ShowFlash(window, state, Loc::Str(IDS_EDITOR_COPIED));
}

void SaveToFolder(HWND window, State& state) {
    const std::wstring folder = state.settings.EffectiveSaveFolder();
    ImageFormat format = FormatFromString(state.settings.saveFormat.c_str());

    // DÜZENLEYİCİ DE AYNI ŞABLONU KULLANIR: "kaydet" düğmesinin ürettiği ad
    // ile yakalama akışının ürettiği ad farklı olsaydı, aynı klasörde iki ayrı
    // adlandırma düzeni birikirdi. Pencere başlığı yok — düzenlenen görüntünün
    // kaynağı artık bilinmiyor.
    NameContext context;
    ::GetLocalTime(&context.time);
    context.width = state.image != nullptr ? state.image->Width() : 0;
    context.height = state.image != nullptr ? state.image->Height() : 0;
    context.random = static_cast<unsigned>(context.time.wMilliseconds);

    const std::wstring baseName = SanitizeFileName(
        ExpandNameFormat(state.settings.fileNameFormat, context));
    const std::wstring subFolder = SanitizeRelativePath(
        ExpandNameFormat(state.settings.subFolderFormat, context));

    const std::wstring path =
        BuildCapturePath(folder, subFolder, baseName, format);
    if (path.empty()) {
        ShowMessage(state.instance, window, Loc::Str(IDS_SAVE_FAILED),
                    MessageIcon::Error);
        return;
    }
    WriteAndReport(window, state, path, format);
}

void SaveAsDialog(HWND window, State& state) {
    // SÜZGEÇLER BİÇİM SIRASINI BELİRLER ve WebP yalnızca kodlayıcısı VARSA
    // sunulur: olmayan bir biçimi listelemek, kullanıcının kaydet dedikten
    // sonra hata görmesi demekti.
    std::wstring filter = L"PNG (*.png)";
    filter.push_back(L'\0');
    filter += L"*.png";
    filter.push_back(L'\0');
    filter += L"JPEG (*.jpg)";
    filter.push_back(L'\0');
    filter += L"*.jpg";
    filter.push_back(L'\0');
    if (IsFormatAvailable(ImageFormat::WebP)) {
        filter += L"WebP (*.webp)";
        filter.push_back(L'\0');
        filter += L"*.webp";
        filter.push_back(L'\0');
    }
    filter.push_back(L'\0');

    wchar_t path[MAX_PATH] = L"";
    const std::wstring suggestion = L"Crisp " + TimestampForFileName() + L".png";
    ::wcscpy_s(path, suggestion.c_str());

    const std::wstring folder = state.settings.EffectiveSaveFolder();

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = window;
    dialog.lpstrFilter = filter.c_str();
    dialog.nFilterIndex = 1;
    dialog.lpstrFile = path;
    dialog.nMaxFile = static_cast<DWORD>(std::size(path));
    dialog.lpstrInitialDir = folder.empty() ? nullptr : folder.c_str();
    dialog.lpstrDefExt = L"png";
    dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER |
                   OFN_NOCHANGEDIR;

    if (!::GetSaveFileNameW(&dialog)) {
        return;   // kullanıcı iptal etti; hata değil
    }

    // BİÇİM UZANTIDAN OKUNUR, süzgeç indeksinden değil: kullanıcı adı elle
    // "not.jpg" yazıp süzgeci PNG'de bıraktığında dosyanın içi uzantısıyla
    // uyuşmazdı.
    ImageFormat format = FormatFromPath(path);
    if (format != ImageFormat::Png && !IsFormatAvailable(format)) {
        format = ImageFormat::Png;
    }
    WriteAndReport(window, state, path, format);
}

}  // namespace editor
}  // namespace crisp
