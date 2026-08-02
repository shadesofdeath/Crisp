// EditorInternal.h — Düzenleyicinin İÇ paylaşımı. Dışarıya açık değildir;
// yalnızca düzenleyicinin kendi .cpp dosyaları arasında paylaşılır.
//
// AYRI DOSYA OLMASININ SEBEBİ boyut: yerleşim, çizim, girdi, dosya işlemleri ve
// pencere ömrü tek bir .cpp'de 400 satırı fazlasıyla aşardı (ev kuralı §9).
// Ayrım işlevsel: bir dosya NE göründüğünü, diğeri NE OLDUĞUNU anlatır.
#pragma once

#include "AlphaLayer.h"
#include "Annotation.h"
#include "Capture.h"
#include "EditorWindow.h"
#include "OcrLayout.h"
#include "Settings.h"

#include <memory>
#include <string>
#include <vector>

#include <windows.h>

namespace crisp {
namespace editor {

// Tasarım ölçüleri (96 DPI mantıksal piksel).
//
// ARAÇ ÇUBUĞU İKİ KATLI: üstte düğmeler, altında grup etiketi. Etiketsiz bir
// simge şeridinde "bu üç düğme neden yan yana" sorusunun cevabı yoktu;
// gruplama zaten vardı ama yalnızca boşlukla anlatılıyordu.
inline constexpr int kToolbarHeight = 64;
inline constexpr int kStatusHeight = 32;
inline constexpr int kButtonSide = 36;
inline constexpr int kButtonGap = 4;
inline constexpr int kGroupGap = 16;
inline constexpr int kToolbarPad = 14;
inline constexpr int kLabelHeight = 14;
inline constexpr int kDropdownWidth = 52;   // renk/kalınlık düğmesi + chevron

// Yakınlaştırma sınırları. Alt sınır büyük bir yakalamanın tümünü görmeye,
// üst sınır tek pikseli ayırt etmeye yeter; ötesi kullanışlı değil, yalnızca
// kaybolmayı kolaylaştırır.
inline constexpr double kMinZoom = 0.1;
inline constexpr double kMaxZoom = 8.0;

enum class ButtonKind { Tool, Color, Thickness, Action, ZoomSlider };

// Efekt menüsündeki komutlar. Menü TPM_RETURNCMD ile okunduğu için dışarı
// sızmazlar; yine de kimlikleri sıfırdan farklı olmalı (0 = iptal).
enum EffectCommand {
    kEffectFlipHorizontal = 1,
    kEffectFlipVertical,
    kEffectAutoCrop,
    kEffectPadding,
    kEffectGrayscale,
    kEffectInvert,
    kEffectSepia,
    kEffectSharpen,
    kEffectBrighter,
    kEffectDarker,
    kEffectMoreContrast,
    kEffectLessContrast,
    kEffectMoreSaturation,
    kEffectLessSaturation,
};

enum ActionId {
    kActionZoomOut = 1,
    kActionZoomIn,
    kActionZoomFit,
    kActionOcr,
    kActionRotateLeft,
    kActionRotateRight,
    kActionScale,
    kActionEffects,   // tek düğme, çok işlem: çevirme, kırpma, renk ayarları
    kActionFill,      // dikdörtgen/elips dolgusu aç-kapa
    kActionUndo,
    kActionRedo,
    kActionClear,
    kActionCopy,
    kActionUpload,
    kActionSave,
    kActionSaveAs,
    kActionClose,
};

// Araç çubuğu grupları; sıra araç çubuğundaki soldan sağa sırayla aynıdır.
enum GroupId {
    kGroupTools,
    kGroupStyle,
    kGroupImage,
    kGroupEdit,
    kGroupFile,
    kGroupCount,
};

struct ToolbarGroup {
    UINT labelId = 0;
    RECT bounds{};   // yalnızca düğme sırası; etiket bunun altına yazılır
};

// Metin tanıma kipinin durumu.
//
// AYRI BİR KİP, AYRI BİR ARAÇ DEĞİL: metin seçmek çizim yapmaya benzemez —
// sürükleme şekil üretmez, imleç I işaretine döner, sağ tık menüsü farklıdır.
// Araç listesine sıkıştırmak, her çizim yolunun içine "ama OCR açıksa" diye
// bir dal eklemek olurdu.
struct OcrMode {
    bool active = false;
    bool attempted = false;   // tanıma çalıştırıldı mı (boş sonuç da sayılır)
    OcrLayout layout;
    int anchor = -1;          // sürüklemenin başladığı kelime
    int cursor = -1;          // sürüklemenin bittiği kelime
    bool selecting = false;

    [[nodiscard]] bool HasSelection() const noexcept {
        return anchor >= 0 && cursor >= 0;
    }
    void Clear() noexcept {
        anchor = -1;
        cursor = -1;
        selecting = false;
    }
};

struct Button {
    ButtonKind kind = ButtonKind::Tool;
    ToolKind tool = ToolKind::Arrow;
    COLORREF color = 0;
    int thickness = 0;
    int action = 0;
    int group = kGroupTools;
    RECT bounds{};
    bool enabled = true;
    // Tıklayınca bir liste açılıyor mu? Chevron yalnızca bu düğmelere çizilir;
    // her düğmeye koymak, hangisinin ne yaptığını yine anlatmazdı.
    bool dropdown = false;
};

struct State {
    // İleti kutusu ve alt pencereler için gerekli; pencere sınıfını kaydeden
    // örnek tutamacı.
    HINSTANCE instance = nullptr;
    Image* image = nullptr;   // çağıranın görüntüsü; dönüşte sonuç burada
    Document document;
    Settings settings;

    ToolKind tool = ToolKind::Arrow;
    COLORREF color = RGB(255, 59, 48);
    int thickness = 3;
    bool fillShapes = false;
    // Bir yükleme sürüyor. İkinci bir tık ikinci bir istek başlatırdı ve hangi
    // bağlantının panoya düştüğü sıraya kalırdı.
    bool uploading = false;

    // SEÇİM ARACI: eklenmiş bir şekli seçip taşımak, silmek, rengini
    // değiştirmek. -1 = seçim yok.
    //
    // NEDEN GEREKLİ: şimdiye kadar bir ok yanlış yere çizildiğinde tek çare
    // geri almaktı — ve geri alma ondan sonra çizilen her şeyi de götürüyordu.
    int selected = -1;
    bool movingShape = false;
    POINT moveGrab{};   // sürüklemenin başladığı görüntü noktası

    // Renk seçicide gösterilen "son kullanılanlar". Oturum boyunca yaşar;
    // diske yazılmaz, çünkü tek bir düzenleme oturumundan sonrasına taşınacak
    // kadar önemli bir tercih değil.
    std::vector<COLORREF> recentColors;

    std::vector<Button> buttons;
    ToolbarGroup groups[kGroupCount]{};
    unsigned dpi = 96;

    bool dragging = false;
    Shape draft;

    bool typing = false;
    Shape textDraft;
    bool caretOn = true;   // metin imlecinin yanıp sönme evresi

    int hoverButton = -1;
    EditorResult result{};

    RECT canvas{};     // görüntünün istemci koordinatındaki yeri
    RECT viewport{};   // tuvalin görünebileceği alan (araç çubuğu ile durum
                       // çubuğu arasında kalan bölge)
    double scale = 1.0;

    // YAKINLAŞTIRMA. fitToWindow açıkken ölçek pencereye göre hesaplanır ve
    // pencere boyutlandıkça izler; kullanıcı bir kez yakınlaştırdığında kapanır,
    // çünkü o andan sonra ölçeği kullanıcı seçmiştir.
    bool fitToWindow = true;
    double zoom = 1.0;
    POINT pan{};       // ortalanmış konuma eklenen kaydırma (istemci pikseli)
    bool panning = false;
    POINT panGrab{};   // kaydırmanın başladığı fare noktası
    POINT panStart{};  // kaydırmanın başındaki pan değeri
    bool zoomDragging = false;   // durum çubuğundaki kaydırıcı tutuluyor

    // İmlecin görüntü koordinatı; durum çubuğunda gösterilir.
    POINT hoverImage{-1, -1};

    OcrMode ocr;

    // ÖLÇEKLEMENİN KAYNAĞI. %25'e indirip sonra %100'e dönen kullanıcı özgün
    // pikselleri geri almalı; küçültülmüş görüntüyü dört kat büyütmek onları
    // geri getirmez. Ölçekleme dışında bir işlem yapıldığı anda temizlenir.
    std::shared_ptr<const Image> scaleSource;
    int scalePercent = 100;

    // Araç çubuğunun yuvarlatılmış zeminleri tek bir alfa katmanına çizilip
    // bir kerede karıştırılır; şekil başına AlphaBlend on beş blit demek olurdu.
    //
    // DURUM ÇUBUĞU AYRI KATMAN: tek bir katman ikisini de kapsasaydı pencere
    // boyunca uzanan bir yüzey her boyamada temizlenip karıştırılırdı; iki
    // ince şerit, aralarındaki koca tuvali hiç dokunmadan bırakır.
    AlphaLayer chrome;
    AlphaLayer statusChrome;

    // İpucu balonu: imleç bir düğmede beklediğinde açılır.
    int tooltipButton = -1;
    bool tooltipVisible = false;

    // Durum çubuğundaki kısa onay şeridi ("Panoya kopyalandı", dosya adı...).
    // KOPYALAMA VE KAYDETME ARTIK PENCEREYİ KAPATMIYOR; kapanma tek geri
    // bildirimken kullanıcı işlemin olup olmadığını hiç öğrenemiyordu.
    std::wstring flashText;
};

// Zamanlayıcı kimlikleri.
inline constexpr UINT_PTR kTooltipTimer = 1;
inline constexpr UINT_PTR kFlashTimer = 2;
inline constexpr UINT_PTR kCaretTimer = 3;

[[nodiscard]] int Scale(int value, unsigned dpi) noexcept;

// Araç çubuğunun tüm düğmeleri için gereken en küçük genişlik.
// SABİT BİR SAYI DEĞİL: araç ya da grup eklendiğinde elle güncellenmesi
// gereken bir tahmin, bir sonraki eklemede unutulur ve düğmeler üst üste
// biner — nitekim ilk denemede tam olarak bu oldu.
[[nodiscard]] int RequiredToolbarWidth(unsigned dpi) noexcept;

void LayoutButtons(State& state, const RECT& client);
void LayoutCanvas(State& state, const RECT& client);
void Paint(HWND window, State& state);

// Pencere yordamı; sınıfı kaydeden EditorSession.cpp'den görünmesi gerekir.
LRESULT CALLBACK EditorProc(HWND window, UINT message, WPARAM wParam,
                            LPARAM lParam);

// --- Araç çubuğu çizimi (EditorChrome.cpp) ----------------------------------
void FillRectColor(HDC dc, const RECT& r, COLORREF color);
void FrameRectColor(HDC dc, const RECT& r, int thickness, COLORREF color);
[[nodiscard]] HFONT CreateUiFont(unsigned dpi, int points, int weight);
[[nodiscard]] bool IsSelected(const State& state, const Button& button) noexcept;
void DrawButtonBackground(AlphaLayer& layer, const State& state,
                          const Button& button, bool selected, bool hovered);
void DrawButtonGlyph(HDC dc, const State& state, const Button& button,
                     bool selected);
void DrawGroupChrome(HDC dc, const State& state);

// --- Simgeler (EditorGlyphs.cpp) --------------------------------------------
// Kodda çizilirler, kaynak olarak gömülmezler: on araç × iki tema × dört DPI
// ölçeği seksen varlık demek olurdu ve hepsi birkaç çizgiden ibaret.
void DrawToolGlyph(HDC dc, const RECT& box, ToolKind tool, COLORREF color,
                   unsigned dpi);
void DrawActionGlyph(HDC dc, const RECT& box, int action, COLORREF color,
                     unsigned dpi);
// Açılır düğmelerin sağ kenarındaki küçük aşağı ok.
void DrawChevron(HDC dc, POINT centre, COLORREF color, unsigned dpi);

// --- Durum çubuğu (EditorStatus.cpp) ----------------------------------------
void DrawStatusBar(HDC dc, const State& state, const RECT& client);
// Kaydırıcının oluk dikdörtgeni; sürükleme hesabı da buradan okur.
[[nodiscard]] RECT ZoomTrack(const State& state, const RECT& slider) noexcept;
void ZoomFromSlider(HWND window, State& state, int x);

// --- Eylemler ve çizim adımları (EditorActions.cpp) -------------------------
void CommitTextDraft(State& state);
void ApplyAction(HWND window, State& state, int action);
void OpenColorPicker(HWND window, State& state, const Button& button);
void OpenThicknessPicker(HWND window, State& state, const Button& button);

// Şekilleri tabana pişirip yeni tabanı belgeye verir; kırpma, döndürme,
// çevirme ve renk ayarları bundan geçer.
void BakeAndReplace(State& state, Image&& newBase);
void DropScaleSource(State& state) noexcept;

// --- Görüntü efektleri (EditorEffects.cpp) -----------------------------------
// TEK DÜĞME, ÇOK İŞLEM: çevirme, otomatik kırpma, kenar boşluğu ve sekiz renk
// ayarı için ayrı ayrı düğme koymak araç çubuğunu iki katına çıkarırdı ve
// hiçbiri günde birden fazla kullanılmıyor.
void ShowEffectsMenu(HWND window, State& state);

// Sürüklenip bırakılan görüntüyü yeni taban yapar (geri alınabilir).
void OpenDroppedImage(HWND window, State& state, const std::wstring& path);

// --- Seçim aracı (EditorSelect.cpp) -----------------------------------------
// Noktanın altındaki şeklin indeksi; en ÜSTTEKİ kazanır (liste sonu = üst).
[[nodiscard]] int ShapeAtPoint(const State& state, POINT image) noexcept;
// Seçim aracının fare olayları. İşlendiyse true döner.
[[nodiscard]] bool SelectMouseDown(HWND window, State& state, POINT client);
[[nodiscard]] bool SelectMouseMove(HWND window, State& state, POINT client);
[[nodiscard]] bool SelectMouseUp(HWND window, State& state);
// Seçili şekli siler; sildiyse true döner.
bool DeleteSelectedShape(HWND window, State& state);
// Seçili şeklin rengini/kalınlığını geçerli ayara çeker.
void RestyleSelectedShape(State& state);
// Seçili şeklin çerçevesini ve tutamaklarını çizer.
void DrawSelectionFrame(HDC dc, const State& state);
void BeginDraw(HWND window, State& state, POINT client);
void UpdateDraw(HWND window, State& state, POINT client);
void EndDraw(HWND window, State& state);

// Şekilleri tabana uygulayıp state.image'i tazeler. Çizim her seferinde
// ORİJİNALDEN başlar: üst üste boyamak, geri alınan bir şeklin izini bırakırdı.
void Rebuild(State& state);

// Geçerli görüntüyü (şekiller pişmiş hâliyle) verir.
[[nodiscard]] bool CurrentFlattened(const State& state, Image& out);

// Yüklemeyi başlatır (arka planda) ve biten yüklemeyi karşılar.
void BeginUpload(HWND window, State& state);
void FinishUpload(HWND window, State& state, LPARAM lParam);

// --- Dosya ve pano (EditorFile.cpp) -----------------------------------------
// Üçü de PENCEREYİ KAPATMAZ ve durum çubuğunda kısa bir onay gösterir.
void CopyToClipboard(HWND window, State& state);
void SaveToFolder(HWND window, State& state);
void SaveAsDialog(HWND window, State& state);
void ShowFlash(HWND window, State& state, std::wstring text);

// --- Yakınlaştırma (EditorLayout.cpp) ---------------------------------------
// İmlecin ALTINDAKİ PİKSELİ SABİT TUTAR: ekranın ortasına yakınlaştırmak,
// kullanıcının baktığı yeri her adımda kaybetmesi demek olurdu.
void ZoomAt(HWND window, State& state, double factor, POINT anchor);
void ZoomToFit(HWND window, State& state);
void ZoomToActual(HWND window, State& state);
void ApplyZoom(HWND window, State& state, double zoom);

// Görüntüyü görünür alandan tamamen çıkaracak kaydırmaları engeller.
void ClampPan(State& state);

[[nodiscard]] POINT ToImage(const State& state, POINT client) noexcept;
[[nodiscard]] POINT ToClient(const State& state, POINT image) noexcept;
[[nodiscard]] RECT ToClientRect(const State& state, const RECT& image) noexcept;
[[nodiscard]] int ButtonAt(const State& state, POINT client) noexcept;

// --- Metin tanıma (EditorOcr.cpp) -------------------------------------------
// Kipi açar; ilk açılışta tanımayı çalıştırır. Tanıma yoksa uyarır ve kip
// açılmaz.
void ToggleOcrMode(HWND window, State& state);

// OCR kipindeki fare ve klavye. İşlendiyse true döner ve çağıran normal
// çizim yolunu ATLAR.
[[nodiscard]] bool OcrMouseDown(HWND window, State& state, POINT client);
[[nodiscard]] bool OcrMouseMove(HWND window, State& state, POINT client);
[[nodiscard]] bool OcrMouseUp(HWND window, State& state);
[[nodiscard]] bool OcrKeyDown(HWND window, State& state, unsigned key, bool control);
void OcrShowMenu(HWND window, State& state, POINT screen);
void OcrCopySelection(HWND window, const State& state);

// Tanınan kelimeleri ve seçimi tuvalin üstüne çizer.
void DrawOcrOverlay(HDC dc, const State& state);

// --- İpuçları (EditorTooltip.cpp) -------------------------------------------
[[nodiscard]] std::wstring TooltipText(const Button& button);
void UpdateTooltipHover(HWND window, State& state, int button);
void ShowTooltipNow(HWND window, State& state);
void HideTooltip(HWND window, State& state);
void DrawTooltip(HDC dc, const State& state, const RECT& client);

// --- Metin aracı (EditorText.cpp) -------------------------------------------
// Yazılmakta olan metnin kutusunu, imlecini ve boşken ipucunu çizer.
//
// NEDEN AYRI: metin aracı tıklandığında EKRANDA HİÇBİR ŞEY OLMUYORDU — ne
// imleç, ne kutu, ne ipucu — ve kullanıcı düğmenin bozuk olduğunu sanıyordu.
// Yazılan metnin önizlemesi tek başına yetmiyor, çünkü ilk harf yazılana
// kadar önizlenecek bir şey yok.
void DrawTextDraft(HDC dc, const State& state);
// Yazma sırasındaki tuşlar. İşlendiyse true döner.
[[nodiscard]] bool TextTypingChar(HWND window, State& state, wchar_t ch);

}  // namespace editor
}  // namespace crisp
