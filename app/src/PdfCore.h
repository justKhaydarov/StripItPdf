// PdfCore.h — MuPDF-backed PDF engine (no Qt dependency).
//
// Opens a PDF, renders pages to RGBA buffers for the viewer, analyses each
// page into "elements" (the big CONTENT image vs. the junk stacked on top),
// and writes a cleaned copy with the overlay layers removed while keeping any
// real text.
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace pdfcore {

// A single rendered page bitmap (premultiplied RGBA, 8-bit).
struct Bitmap {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;   // size = width*height*4
    bool valid() const { return width > 0 && height > 0 && !rgba.empty(); }
};

// One drawable element detected on a page.
enum class Role { Content, Overlay };
struct Element {
    Role role = Role::Overlay;
    std::string kind;     // "image", "layer" (OCG) or "tilt" (diagonal text)
    std::string label;    // e.g. "Image 1842x962" / "Layer: Watermark"
    bool keep = false;    // current keep/delete decision (default from role)
    int imgW = 0, imgH = 0;
    int placements = 1;   // how many times this image is painted on the page
    double cover = 0.0;   // largest single placement / page area  (0..1)
    double opacity = 1.0; // mean alpha of the image (1 = opaque, low = watermark)
    // Bounding boxes of every placement, in PDF page points (top-left origin).
    struct Rect { double x0, y0, x1, y1; };
    std::vector<Rect> rects;
};

// Signature identifying one image element well enough to match it during the
// strip (size + coverage + opacity — opacity disambiguates an opaque content
// image from a same-size transparent watermark drawn over it).
struct ImgSig { int w = 0, h = 0; double cover = 0, opacity = 1; };

// Per-page selection of which images to keep, plus the global watermark toggles.
struct Selection {
    std::vector<std::vector<ImgSig>> keep;   // keep[pageNo] = images to KEEP
    bool removeOptionalContent = true;
    bool removeDiagonalText = true;
    int  version = 0;     // bump when anything changes (drives render cache)
};

struct PageInfo {
    int number = 0;
    double width = 0, height = 0;   // in points
    int textChars = 0;              // length of the page's real text layer
    std::vector<Element> elements;  // biggest-first
};

// Options controlling what is treated as a removable overlay.
struct StripOptions {
    int    minRepeats = 3;     // image tiled >= this many times => overlay
    double maxCover   = 0.20;  // image covering < this fraction => overlay
    double keepCover  = 0.50;  // image covering >= this fraction => kept content
    double opaqueThreshold = 0.50;  // full-cover image below this opacity = watermark
    bool   removeOptionalContent = true;  // empty OCG (watermark) form layers
    bool   removeDiagonalText = true;     // drop rotated (watermark) text glyphs
    double maxTextAngleDeg = 8.0;         // text tilted more than this => watermark
};

class Document {
public:
    Document();
    ~Document();
    Document(const Document&) = delete;
    Document& operator=(const Document&) = delete;

    // Open a PDF; returns false on failure (message in lastError()).
    bool open(const std::string& path);
    void close();
    bool isOpen() const;
    int  pageCount() const;
    const std::string& lastError() const { return error_; }
    const std::string& path() const { return path_; }

    // Render a page at the given zoom (1.0 = 72 dpi).  When cleaned is true the
    // overlays selected for removal are stripped first (cached by sel.version).
    // Pass sel = nullptr to render the raw original.
    Bitmap renderPage(int pageNo, double zoom, bool cleaned,
                      const StripOptions& opt, const Selection* sel = nullptr);

    // Inspect a page: classify its elements (content vs overlay).
    PageInfo analyzePage(int pageNo, const StripOptions& opt);

    // Strip overlays and write a cleaned PDF to outPath.  When sel is given the
    // per-page keep selection is honoured; otherwise the StripOptions
    // thresholds decide.  Returns false on failure; fills stats if provided.
    struct Stats { int pages = 0, imagesRemoved = 0, layersEmptied = 0; };
    bool saveStripped(const std::string& outPath, const StripOptions& opt,
                      const Selection* sel = nullptr, Stats* stats = nullptr);

private:
    struct Impl;
    Impl* d_;
    std::string path_;
    std::string error_;
};

}  // namespace pdfcore
