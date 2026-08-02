// EditorKeys.cpp — Düzenleyicinin KLAVYESİ: araç tuşları ve kısayollar.
//
// AYRI DOSYA: EditorInput.cpp 411 satıra çıkmıştı ve ev kuralı 400 (docs §9).
// Ayrım işlevsel de — bu dosya bir TUŞ EŞLEMESİ, komşusu ise pencere mesajı
// yönlendirmesi. Yeni bir kısayol eklemek artık fare, tekerlek ve boyutlanma
// mantığının arasında yer aramayı gerektirmiyor.
#include "EditorInternal.h"

namespace crisp {
namespace editor {

// SAYI TUŞLARI ARAÇ SEÇER. On iki aracın hepsine fareyle gitmek, işaretlerken
// elin sürekli araç çubuğuna dönmesi demekti. Sıra araç çubuğundakiyle aynı,
// '1' oktan başlar ve '0' mozaikte biter; seçim ile kırpma harf tuşundadır.
constexpr ToolKind kToolOrder[] = {
    ToolKind::Mosaic,      ToolKind::Arrow,       ToolKind::Line,
    ToolKind::Rectangle,   ToolKind::Ellipse,     ToolKind::Pen,
    ToolKind::Highlighter, ToolKind::Text,        ToolKind::StepNumber,
    ToolKind::Blur};

// Seçim ve kırpma HARF TUŞUYLA. On iki araç, on rakama sığmıyor ve seçim
// aracının 'V' olması her çizim programında aynı; kırpmanın 'C'si de öyle.
void PickTool(HWND window, State& state, ToolKind tool) {
    CommitTextDraft(state);
    state.tool = tool;
    state.ocr.active = false;
    if (!ToolIsSelect(tool)) {
        state.selected = -1;
    }
    RECT area{};
    ::GetClientRect(window, &area);
    LayoutButtons(state, area);
    ::InvalidateRect(window, nullptr, FALSE);
}

[[nodiscard]] bool OnKeyDown(HWND window, State& state, WPARAM key) {
    const bool control = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool shift = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;

    // OCR kipi tuşları ÖNCE görür: açıkken Ctrl+C seçili METNİ kopyalamalı,
    // görüntüyü değil.
    if (OcrKeyDown(window, state, static_cast<unsigned>(key), control)) {
        return true;
    }

    if (control && (key == VK_OEM_PLUS || key == VK_ADD)) {
        ApplyAction(window, state, kActionZoomIn);
        return true;
    }
    if (control && (key == VK_OEM_MINUS || key == VK_SUBTRACT)) {
        ApplyAction(window, state, kActionZoomOut);
        return true;
    }
    if (control && key == '0') {
        ZoomToFit(window, state);
        return true;
    }

    // Metin yazılırken devre dışı: o sırada rakam metnin parçasıdır.
    if (!control && !state.typing && key >= '0' && key <= '9') {
        PickTool(window, state, kToolOrder[key - '0']);
        return true;
    }
    if (!control && !state.typing && key == 'V') {
        PickTool(window, state, ToolKind::Select);
        return true;
    }
    if (!control && !state.typing && key == 'C') {
        PickTool(window, state, ToolKind::Crop);
        return true;
    }

    // Delete: seçili şekli siler. Geri almadan farkı, ARADAKİ şekilleri
    // bırakmasıdır — şikâyetin tamamı buydu.
    if ((key == VK_DELETE || key == VK_BACK) && !state.typing &&
        state.selected >= 0) {
        (void)DeleteSelectedShape(window, state);
        return true;
    }

    if (key == VK_ESCAPE) {
        if (state.typing) {
            // İlk Esc yazmayı iptal eder, pencereyi kapatmaz: kullanıcı bir
            // harfi yanlış yazdı diye tüm düzenlemeyi kaybetmemeli.
            state.typing = false;
            state.textDraft = Shape{};
            ::KillTimer(window, kCaretTimer);
            ::InvalidateRect(window, nullptr, FALSE);
            return true;
        }
        ::DestroyWindow(window);
        return true;
    }
    if (control && key == 'Z') {
        ApplyAction(window, state, shift ? kActionRedo : kActionUndo);
        return true;
    }
    if (control && key == 'Y') {
        ApplyAction(window, state, kActionRedo);
        return true;
    }
    if (control && key == 'C') {
        ApplyAction(window, state, kActionCopy);
        return true;
    }
    if (control && key == 'S') {
        // Ctrl+Shift+S "farklı kaydet": her düzenleyicide aynı anlamda.
        ApplyAction(window, state, shift ? kActionSaveAs : kActionSave);
        return true;
    }
    return false;
}

}  // namespace editor
}  // namespace crisp
