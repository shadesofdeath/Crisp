// OcrScan.cpp — Tanınan kelimelerin yerleşimini çıkarır.
//
// AYRI DOSYA: metin çıkarmayla birlikte Ocr.cpp ev kuralının 400 satır
// sınırını aşıyordu (docs §9).
#include "Ocr.h"

#include "OcrInternal.h"
#include "OcrLayout.h"
#include "Util.h"

namespace crisp {


bool RecognizeLayout(const Image& image, OcrLayout& layout) {
    layout.words.clear();

    ComPtr<IOcrResult> result;
    if (!RunRecognition(image, result)) {
        return false;
    }

    ComPtr<IVectorView<OcrLine*>> lines;
    if (FAILED(result->get_Lines(lines.GetAddressOf())) || !lines) {
        return false;
    }

    unsigned lineCount = 0;
    if (FAILED(lines->get_Size(&lineCount))) {
        return false;
    }

    for (unsigned lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
        ComPtr<IOcrLine> line;
        if (FAILED(lines->GetAt(lineIndex, line.GetAddressOf())) || !line) {
            continue;
        }

        ComPtr<IVectorView<WinOcr::OcrWord*>> words;
        if (FAILED(line->get_Words(words.GetAddressOf())) || !words) {
            continue;
        }

        unsigned wordCount = 0;
        if (FAILED(words->get_Size(&wordCount))) {
            continue;
        }

        for (unsigned wordIndex = 0; wordIndex < wordCount; ++wordIndex) {
            ComPtr<IOcrWord> word;
            if (FAILED(words->GetAt(wordIndex, word.GetAddressOf())) || !word) {
                continue;
            }

            HStringOwner wordText;
            if (FAILED(word->get_Text(wordText.put()))) {
                continue;
            }

            Rect box{};
            if (FAILED(word->get_BoundingRect(&box))) {
                continue;
            }

            OcrWord entry;
            entry.text = wordText.str();
            entry.line = static_cast<int>(lineIndex);
            // Rect KAYAR NOKTA ve genişlik/yükseklik taşır; RECT tam sayı ve
            // sağ/alt kenar taşır. Sol/üst aşağı, sağ/alt yukarı yuvarlanır ki
            // kutu tanınan pikselleri asla KIRPMASIN — bir piksel eksik kutu,
            // kelimenin kenarında yapılan tıklamayı ıskalatır.
            entry.bounds.left = static_cast<LONG>(box.X);
            entry.bounds.top = static_cast<LONG>(box.Y);
            entry.bounds.right =
                static_cast<LONG>(box.X + box.Width + 0.999f);
            entry.bounds.bottom =
                static_cast<LONG>(box.Y + box.Height + 0.999f);

            if (!entry.text.empty()) {
                layout.words.push_back(std::move(entry));
            }
        }
    }

    // Motorun verdiği sıra tüm ekranda taranırken ekrandaki görsel sırayla
    // örtüşmez; birden çok pencere açıkken satırlar zıplar. Aralık seçimi
    // kelime sırasına dayandığı için bu düzeltme olmadan seçim ekranın dört
    // bir yanına dağılır.
    ocrsel::NormalizeReadingOrder(layout);
    return true;
}

}  // namespace crisp
