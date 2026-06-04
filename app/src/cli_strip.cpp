// cli_strip.cpp — headless test/driver for the PDF core.
#include "PdfCore.h"
#include <cstdio>

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: %s in.pdf [out.pdf]\n", argv[0]); return 2; }
    std::string in = argv[1];
    std::string out = argc > 2 ? argv[2] : (in.substr(0, in.rfind('.')) + "_stripped.pdf");

    pdfcore::Document doc;
    if (!doc.open(in)) { std::fprintf(stderr, "open failed: %s\n", doc.lastError().c_str()); return 1; }
    std::printf("opened %s : %d pages\n", in.c_str(), doc.pageCount());

    pdfcore::StripOptions opt;
    auto info = doc.analyzePage(0, opt);
    std::printf("page1 %.0fx%.0f  textChars=%d  elements:\n", info.width, info.height, info.textChars);
    for (auto& e : info.elements) {
        std::printf("  [%s] %-22s placements=%d cover=%.3f opacity=%.2f\n",
                    e.role == pdfcore::Role::Content ? "KEEP" : "drop",
                    e.label.c_str(), e.placements, e.cover, e.opacity);
    }

    pdfcore::Document::Stats st;
    if (!doc.saveStripped(out, opt, nullptr, &st)) {
        std::fprintf(stderr, "save failed: %s\n", doc.lastError().c_str()); return 1;
    }
    std::printf("stripped -> %s  (pages=%d, layersEmptied=%d)\n",
                out.c_str(), st.pages, st.layersEmptied);
    return 0;
}
