// PdfCore.cpp — MuPDF-backed implementation.
#include "PdfCore.h"

#include <mupdf/fitz.h>
#include <mupdf/pdf.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <string>

namespace pdfcore {

// --------------------------------------------------------------------------
//  Internal helpers
// --------------------------------------------------------------------------
namespace {

struct ImgDraw { fz_image* img; fz_matrix ctm; int w, h; double area; };

// Mean opacity of an image (0 = fully transparent, 1 = fully opaque).  Watermark
// overlays are mostly transparent (thin marks on clear background); real content
// scans are opaque.  Computed cheaply from a subsampled copy of the soft mask.
double meanOpacity(fz_context* ctx, fz_image* img) {
    if (!img || !img->mask) return 1.0;          // no soft mask => opaque
    fz_image* mask = img->mask;
    double result = 1.0;
    fz_pixmap* pm = nullptr;
    fz_var(pm);
    fz_try(ctx) {
        int tw = mask->w, th = mask->h;
        float sc = 64.0f / (float)fz_maxi(1, fz_maxi(tw, th));
        if (sc > 1.0f) sc = 1.0f;
        fz_matrix ctm = fz_scale(tw * sc, th * sc);   // on-page size => subsample
        int ow = 0, oh = 0;
        pm = fz_get_pixmap_from_image(ctx, mask, nullptr, &ctm, &ow, &oh);
        if (pm && pm->w > 0 && pm->h > 0) {
            int n = pm->n;
            long long sum = 0, cnt = 0;
            for (int y = 0; y < pm->h; y++) {
                const uint8_t* row = pm->samples + (size_t)y * pm->stride;
                for (int x = 0; x < pm->w; x++) { sum += row[x * n]; cnt++; }
            }
            if (cnt) result = (double)sum / (cnt * 255.0);
        }
    }
    fz_always(ctx) { if (pm) fz_drop_pixmap(ctx, pm); }
    fz_catch(ctx) { result = 1.0; }
    return result;
}

// cached opacity lookup keyed on the fz_image pointer (stable within a run)
double cachedOpacity(fz_context* ctx, fz_image* img,
                     std::map<fz_image*, double>* cache) {
    if (!cache) return meanOpacity(ctx, img);
    auto it = cache->find(img);
    if (it != cache->end()) return it->second;
    double v = meanOpacity(ctx, img);
    (*cache)[img] = v;
    return v;
}

// Device that records every image paint on a page.
struct CaptureDevice { fz_device base; std::vector<ImgDraw>* out; };

void capImage(fz_context*, fz_device* dev, fz_image* img, fz_matrix ctm,
              float, fz_color_params) {
    auto* cd = reinterpret_cast<CaptureDevice*>(dev);
    double area = std::fabs(ctm.a * ctm.d - ctm.b * ctm.c);
    cd->out->push_back({img, ctm, img->w, img->h, area});
}
void capImageMask(fz_context* c, fz_device* d, fz_image* img, fz_matrix ctm,
                  fz_colorspace*, const float*, float a, fz_color_params cp) {
    capImage(c, d, img, ctm, a, cp);
}

// Per-page context handed to the filters while stripping.
struct FilterCtx {
    double pageArea;
    double keepCover;            // fallback cover threshold (no explicit selection)
    double opaqueThreshold;      // full-cover images below this opacity = watermark
    bool dropDiagonal;
    double maxAngle;
    const std::vector<ImgSig>* keepSigs;        // explicit selection (or nullptr)
    std::map<fz_image*, double>* opCache;       // memoised opacity per image
};

// image_filter: keep the big OPAQUE content image, drop overlays — including
// full-cover but transparent watermark images (stripes / logos / @handles).
fz_image* imgFilter(fz_context* ctx, void* opaque, fz_matrix ctm,
                    const char* /*name*/, fz_image* image) {
    auto* fc = reinterpret_cast<FilterCtx*>(opaque);
    double area = std::fabs(ctm.a * ctm.d - ctm.b * ctm.c);
    double cover = fc->pageArea > 0 ? area / fc->pageArea : 0.0;
    int w = image ? image->w : 0, h = image ? image->h : 0;

    if (fc->keepSigs) {                       // honour explicit per-page selection
        for (const ImgSig& s : *fc->keepSigs) {
            double tol = 0.02 * std::max(cover, s.cover) + 0.01;
            if (s.w == w && s.h == h && std::fabs(cover - s.cover) <= tol) {
                double op = cachedOpacity(ctx, image, fc->opCache);
                if (std::fabs(op - s.opacity) <= 0.25) return image;   // keep
            }
        }
        return nullptr;
    }
    // threshold mode: keep only full-cover AND opaque images
    if (cover < fc->keepCover) return nullptr;
    double op = cachedOpacity(ctx, image, fc->opCache);
    return op >= fc->opaqueThreshold ? image : nullptr;
}

// text_filter: drop rotated/diagonal glyphs (watermarks); keep horizontal text.
// Returns non-zero to remove the character.
int textFilter(fz_context*, void* opaque, int* /*ucs*/, int /*len*/,
               fz_matrix trm, fz_matrix ctm, fz_rect /*bbox*/) {
    auto* fc = reinterpret_cast<FilterCtx*>(opaque);
    if (!fc->dropDiagonal) return 0;
    fz_matrix m = fz_concat(trm, ctm);              // glyph orientation on page
    double deg = std::atan2((double)m.b, (double)m.a) * 180.0 / 3.14159265358979;
    // distance from horizontal (0 or 180 degrees)
    double a = std::fabs(deg);
    double off = std::min(a, std::fabs(180.0 - a));
    return (off > fc->maxAngle) ? 1 : 0;
}

}  // namespace

// --------------------------------------------------------------------------
//  Impl
// --------------------------------------------------------------------------
struct Document::Impl {
    fz_context* ctx = nullptr;
    fz_document* doc = nullptr;      // pristine
    pdf_document* pdf = nullptr;

    fz_document* cleanDoc = nullptr; // cached stripped clone
    int cleanVersion = -1;
    bool cleanValid = false;

    std::string path;
    std::recursive_mutex mu;

    ~Impl() { reset(); if (ctx) fz_drop_context(ctx); }

    void dropClean() {
        if (cleanDoc) { fz_drop_document(ctx, cleanDoc); cleanDoc = nullptr; }
        cleanValid = false;
    }
    void reset() {
        dropClean();
        if (doc) { fz_drop_document(ctx, doc); doc = nullptr; }
        pdf = nullptr;
    }

    // Empty every form XObject that is marked as optional content (a watermark
    // layer).  Removes logo/diagonal-text watermark groups in one pass.
    int emptyOCForms(pdf_document* p) {
        int emptied = 0;
        int xl = pdf_xref_len(ctx, p);
        fz_buffer* empty = fz_new_buffer(ctx, 1);
        for (int i = 1; i < xl; i++) {
            pdf_obj* ref = pdf_new_indirect(ctx, p, i, 0);
            fz_try(ctx) {
                pdf_obj* o = pdf_resolve_indirect(ctx, ref);
                if (pdf_is_dict(ctx, o)) {
                    pdf_obj* sub = pdf_dict_get(ctx, o, PDF_NAME(Subtype));
                    pdf_obj* oc  = pdf_dict_get(ctx, o, PDF_NAME(OC));
                    if (oc && pdf_name_eq(ctx, sub, PDF_NAME(Form))) {
                        pdf_update_stream(ctx, p, ref, empty, 0);
                        emptied++;
                    }
                }
            }
            fz_always(ctx) { pdf_drop_obj(ctx, ref); }
            fz_catch(ctx) { /* skip unreadable object */ }
        }
        fz_drop_buffer(ctx, empty);
        return emptied;
    }

    // Strip overlays from one already-open pdf_document, in place.
    // When sel != nullptr its per-page keep selection and watermark toggles win.
    void stripInPlace(pdf_document* p, const StripOptions& opt,
                      const Selection* sel, Stats* st) {
        bool dropOC = sel ? sel->removeOptionalContent : opt.removeOptionalContent;
        bool dropDiag = sel ? sel->removeDiagonalText : opt.removeDiagonalText;
        if (dropOC) {
            int e = emptyOCForms(p);
            if (st) st->layersEmptied += e;
        }
        int n = fz_count_pages(ctx, (fz_document*)p);
        if (st) st->pages = n;
        for (int i = 0; i < n; i++) {
            pdf_page* page = pdf_load_page(ctx, p, i);
            fz_rect b = fz_bound_page(ctx, (fz_page*)page);
            const std::vector<ImgSig>* keepSigs = nullptr;
            if (sel && i < (int)sel->keep.size())
                keepSigs = &sel->keep[i];
            std::map<fz_image*, double> opCache;
            FilterCtx fc{ (double)(b.x1 - b.x0) * (b.y1 - b.y0), opt.keepCover,
                          opt.opaqueThreshold, dropDiag, opt.maxTextAngleDeg,
                          keepSigs, &opCache };

            pdf_sanitize_filter_options sopts; memset(&sopts, 0, sizeof(sopts));
            sopts.opaque = &fc;
            sopts.image_filter = imgFilter;
            sopts.text_filter = textFilter;

            pdf_filter_factory facts[2];
            memset(facts, 0, sizeof(facts));
            facts[0].filter = pdf_new_sanitize_filter;
            facts[0].options = &sopts;

            pdf_filter_options fopts; memset(&fopts, 0, sizeof(fopts));
            fopts.recurse = 1;
            fopts.instance_forms = 1;
            fopts.filters = facts;

            fz_try(ctx) { pdf_filter_page_contents(ctx, p, page, &fopts); }
            fz_always(ctx) { fz_drop_page(ctx, (fz_page*)page); }
            fz_catch(ctx) { /* leave page as-is on error */ }
        }
    }

    fz_document* ensureClean(const StripOptions& opt, const Selection* sel) {
        int ver = sel ? sel->version : 0;
        if (cleanValid && cleanDoc && cleanVersion == ver) return cleanDoc;
        dropClean();
        fz_document* c = fz_open_document(ctx, path.c_str());
        pdf_document* cp = pdf_specifics(ctx, c);
        if (cp) stripInPlace(cp, opt, sel, nullptr);
        cleanDoc = c; cleanVersion = ver; cleanValid = true;
        return cleanDoc;
    }
};

// --------------------------------------------------------------------------
//  Document
// --------------------------------------------------------------------------
Document::Document() : d_(new Impl) {
    d_->ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    fz_register_document_handlers(d_->ctx);
}
Document::~Document() { delete d_; }

bool Document::open(const std::string& path) {
    std::lock_guard<std::recursive_mutex> lk(d_->mu);
    close();
    error_.clear();
    fz_try(d_->ctx) {
        d_->doc = fz_open_document(d_->ctx, path.c_str());
        d_->pdf = pdf_specifics(d_->ctx, d_->doc);
        d_->path = path;
        path_ = path;
    }
    fz_catch(d_->ctx) {
        error_ = fz_caught_message(d_->ctx);
        if (d_->doc) { fz_drop_document(d_->ctx, d_->doc); d_->doc = nullptr; }
        d_->pdf = nullptr;
        return false;
    }
    if (!d_->pdf) { error_ = "Not a PDF file."; close(); return false; }
    return true;
}

void Document::close() {
    std::lock_guard<std::recursive_mutex> lk(d_->mu);
    d_->reset();
    d_->path.clear();
}
bool Document::isOpen() const { return d_->doc != nullptr; }
int  Document::pageCount() const {
    if (!d_->doc) return 0;
    return fz_count_pages(d_->ctx, d_->doc);
}

Bitmap Document::renderPage(int pageNo, double zoom, bool cleaned,
                            const StripOptions& opt, const Selection* sel) {
    std::lock_guard<std::recursive_mutex> lk(d_->mu);
    Bitmap out;
    if (!d_->doc) return out;
    fz_context* ctx = d_->ctx;
    fz_document* src = cleaned ? d_->ensureClean(opt, sel) : d_->doc;
    fz_pixmap* pix = nullptr;
    fz_try(ctx) {
        fz_matrix m = fz_scale((float)zoom, (float)zoom);
        pix = fz_new_pixmap_from_page_number(ctx, src, pageNo, m,
                                             fz_device_rgb(ctx), 0);
        out.width = pix->w;
        out.height = pix->h;
        out.rgba.resize((size_t)pix->w * pix->h * 4);
        int n = pix->n;                    // 3 (rgb, no alpha)
        const uint8_t* s = pix->samples;
        uint8_t* dst = out.rgba.data();
        for (int y = 0; y < pix->h; y++) {
            const uint8_t* row = s + (size_t)y * pix->stride;
            for (int x = 0; x < pix->w; x++) {
                const uint8_t* px = row + x * n;
                *dst++ = px[0]; *dst++ = px[1]; *dst++ = px[2]; *dst++ = 255;
            }
        }
    }
    fz_always(ctx) { if (pix) fz_drop_pixmap(ctx, pix); }
    fz_catch(ctx) { out = Bitmap{}; }
    return out;
}

PageInfo Document::analyzePage(int pageNo, const StripOptions& opt) {
    std::lock_guard<std::recursive_mutex> lk(d_->mu);
    PageInfo info;
    info.number = pageNo;
    if (!d_->doc) return info;
    fz_context* ctx = d_->ctx;

    fz_page* page = nullptr;
    fz_stext_page* stext = nullptr;
    fz_try(ctx) {
        page = fz_load_page(ctx, d_->doc, pageNo);
        fz_rect b = fz_bound_page(ctx, page);
        info.width = b.x1 - b.x0;
        info.height = b.y1 - b.y0;
        double pageArea = info.width * info.height;
        if (pageArea <= 0) pageArea = 1;

        // ---- capture image draws ----
        std::vector<ImgDraw> draws;
        CaptureDevice cd; memset(&cd, 0, sizeof(cd));
        cd.base.fill_image = capImage;
        cd.base.fill_image_mask = capImageMask;
        cd.out = &draws;
        fz_run_page(ctx, page, (fz_device*)&cd, fz_identity, nullptr);

        // ---- group draws by underlying image (MuPDF caches => same ptr) ----
        struct Group { int w, h; double maxArea; fz_image* img;
                       std::vector<Element::Rect> rects; };
        std::map<fz_image*, Group> groups;
        for (auto& dr : draws) {
            auto& g = groups[dr.img];
            g.w = dr.w; g.h = dr.h; g.img = dr.img;
            fz_rect r = fz_transform_rect(fz_unit_rect, dr.ctm);
            g.rects.push_back({r.x0, r.y0, r.x1, r.y1});
            g.maxArea = std::max(g.maxArea, dr.area);
        }

        for (auto& kv : groups) {
            Group& g = kv.second;
            Element e;
            e.kind = "image";
            e.imgW = g.w; e.imgH = g.h;
            e.placements = (int)g.rects.size();
            e.cover = g.maxArea / pageArea;
            e.rects = g.rects;
            // opacity only matters (and is only worth decoding) for big images
            e.opacity = e.cover >= opt.keepCover ? meanOpacity(ctx, g.img) : 1.0;
            bool overlay = e.placements >= opt.minRepeats
                        || e.cover < opt.keepCover
                        || e.opacity < opt.opaqueThreshold;
            e.role = overlay ? Role::Overlay : Role::Content;
            e.keep = (e.role == Role::Content);
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Image %dx%d", g.w, g.h);
            e.label = buf;
            if (e.placements > 1) e.label += "  (x" + std::to_string(e.placements) + ")";
            info.elements.push_back(std::move(e));
        }
        std::sort(info.elements.begin(), info.elements.end(),
                  [](const Element& a, const Element& b){ return a.cover > b.cover; });

        // ---- optional-content (watermark) layer, informational ----
        if (d_->pdf) {
            pdf_obj* root = pdf_dict_get(ctx, pdf_trailer(ctx, d_->pdf), PDF_NAME(Root));
            pdf_obj* ocp = pdf_dict_get(ctx, root, PDF_NAME(OCProperties));
            pdf_obj* ocgs = pdf_dict_get(ctx, ocp, PDF_NAME(OCGs));
            int nc = pdf_array_len(ctx, ocgs);
            for (int i = 0; i < nc; i++) {
                pdf_obj* g = pdf_array_get(ctx, ocgs, i);
                const char* nm = pdf_to_text_string(ctx, pdf_dict_get(ctx, g, PDF_NAME(Name)));
                Element e; e.kind = "layer"; e.role = Role::Overlay;
                e.label = std::string("Layer: ") + (nm && *nm ? nm : "Optional content");
                info.elements.push_back(std::move(e));
            }
        }

        // ---- real text length + diagonal (watermark) text detection ----
        stext = fz_new_stext_page_from_page(ctx, page, nullptr);
        int chars = 0, tilted = 0;
        for (fz_stext_block* blk = stext->first_block; blk; blk = blk->next) {
            if (blk->type != FZ_STEXT_BLOCK_TEXT) continue;
            for (fz_stext_line* ln = blk->u.t.first_line; ln; ln = ln->next)
                for (fz_stext_char* ch = ln->first_char; ch; ch = ch->next) {
                    if (ch->c == ' ') continue;
                    chars++;
                    // baseline direction from the glyph quad (ul -> ur)
                    double dx = ch->quad.ur.x - ch->quad.ul.x;
                    double dy = ch->quad.ur.y - ch->quad.ul.y;
                    double deg = std::atan2(dy, dx) * 180.0 / 3.14159265358979;
                    double a = std::fabs(deg);
                    if (std::min(a, std::fabs(180.0 - a)) > opt.maxTextAngleDeg) tilted++;
                }
        }
        info.textChars = chars;
        if (tilted > 0 && tilted * 5 >= chars) {   // a real diagonal watermark
            Element e; e.kind = "tilt"; e.role = Role::Overlay; e.keep = false;
            e.placements = tilted;
            e.label = "Diagonal text watermark";
            info.elements.push_back(std::move(e));
        }
    }
    fz_always(ctx) {
        if (stext) fz_drop_stext_page(ctx, stext);
        if (page) fz_drop_page(ctx, page);
    }
    fz_catch(ctx) {}
    return info;
}

bool Document::saveStripped(const std::string& outPath, const StripOptions& opt,
                            const Selection* sel, Stats* stats) {
    std::lock_guard<std::recursive_mutex> lk(d_->mu);
    if (!d_->doc) { error_ = "No document open."; return false; }
    fz_context* ctx = d_->ctx;
    fz_document* fresh = nullptr;
    bool ok = false;
    Stats local;
    fz_try(ctx) {
        fresh = fz_open_document(ctx, d_->path.c_str());
        pdf_document* fp = pdf_specifics(ctx, fresh);
        if (!fp) fz_throw(ctx, FZ_ERROR_GENERIC, "not a pdf");
        d_->stripInPlace(fp, opt, sel, &local);

        pdf_write_options pwo = pdf_default_write_options;
        pwo.do_garbage = 4;
        pwo.do_compress = 1;
        pwo.do_compress_images = 1;
        pwo.do_compress_fonts = 1;
        pdf_save_document(ctx, fp, outPath.c_str(), &pwo);
        ok = true;
    }
    fz_always(ctx) { if (fresh) fz_drop_document(ctx, fresh); }
    fz_catch(ctx) { error_ = fz_caught_message(ctx); ok = false; }
    if (ok && stats) *stats = local;
    return ok;
}

}  // namespace pdfcore
