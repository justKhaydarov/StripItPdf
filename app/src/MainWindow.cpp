#include "MainWindow.h"
#include "PageView.h"

#include <QtWidgets>
#include <functional>

namespace {
QLabel* mk(const QString& text, const QString& objName = QString()) {
    auto* l = new QLabel(text);
    if (!objName.isEmpty()) l->setObjectName(objName);
    return l;
}

// A card the user can click anywhere to toggle (not just the checkbox).
class ClickCard : public QFrame {
public:
    std::function<void()> onClick;
    explicit ClickCard(QWidget* p = nullptr) : QFrame(p) {
        setAttribute(Qt::WA_Hover, true);
        setCursor(Qt::PointingHandCursor);
    }
protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && onClick) onClick();
        QFrame::mousePressEvent(e);
    }
};
QImage toQImage(const pdfcore::Bitmap& b) {
    if (!b.valid()) return QImage();
    return QImage(b.rgba.data(), b.width, b.height, b.width * 4,
                  QImage::Format_RGBA8888).copy();
}
}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("StripItPdf — Layer Editor");
    auto* root = new QWidget;
    root->setObjectName("root");
    auto* lay = new QHBoxLayout(root);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(buildSidebar());
    lay->addWidget(buildCenter(), 1);
    lay->addWidget(buildInspector());
    setCentralWidget(root);
    resize(1300, 840);
    updateNav();
}

// --------------------------------------------------------------------------
//  Sidebar
// --------------------------------------------------------------------------
QWidget* MainWindow::buildSidebar() {
    auto* side = new QFrame;
    side->setObjectName("sidebar");
    side->setFixedWidth(256);
    auto* v = new QVBoxLayout(side);
    v->setContentsMargins(20, 22, 20, 18);
    v->setSpacing(14);

    // brand
    auto* brand = new QHBoxLayout; brand->setSpacing(11);
    auto* logo = new QLabel;
    QPixmap lp(":/logo.png");
    logo->setPixmap(lp.scaled(40, 40, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    brand->addWidget(logo);
    auto* bt = new QVBoxLayout; bt->setSpacing(0);
    bt->addWidget(mk("StripItPdf", "brandTitle"));
    bt->addWidget(mk("Layer Editor", "brandSubtitle"));
    brand->addLayout(bt); brand->addStretch();
    v->addLayout(brand);

    auto* open = new QPushButton("  Open PDF");
    open->setObjectName("primary");
    open->setCursor(Qt::PointingHandCursor);
    auto* glow = new QGraphicsDropShadowEffect(open);
    glow->setBlurRadius(22); glow->setOffset(0, 6);
    glow->setColor(QColor(37, 99, 235, 90));
    open->setGraphicsEffect(glow);
    connect(open, &QPushButton::clicked, this, [this]{ openDialog(); });
    v->addWidget(open);

    auto* kick = mk("PAGES"); kick->setProperty("class", "kicker");
    kick->setStyleSheet("color:#64748B;font-size:11px;font-weight:700;");
    v->addWidget(kick);

    pageList_ = new QListWidget;
    pageList_->setObjectName("pages");
    pageList_->setIconSize(QSize(116, 150));
    pageList_->setSpacing(0);
    pageList_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    connect(pageList_, &QListWidget::currentRowChanged, this, [this](int r){
        if (!building_ && r >= 0) showPage(r);
    });
    v->addWidget(pageList_, 1);

    footer_ = mk("No document open", "footer");
    footer_->setWordWrap(true);
    v->addWidget(footer_);
    return side;
}

// --------------------------------------------------------------------------
//  Center
// --------------------------------------------------------------------------
QWidget* MainWindow::buildCenter() {
    auto* center = new QWidget;
    auto* v = new QVBoxLayout(center);
    v->setContentsMargins(0, 0, 0, 0); v->setSpacing(0);

    auto* bar = new QFrame; bar->setObjectName("topbar");
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(26, 16, 26, 16);
    auto* titles = new QVBoxLayout; titles->setSpacing(1);
    pageTitle_ = mk("StripItPdf", "pageTitle");
    pageSubtitle_ = mk("Open a PDF to strip its watermark layers", "pageSubtitle");
    titles->addWidget(pageTitle_); titles->addWidget(pageSubtitle_);
    h->addLayout(titles); h->addStretch();

    // segmented Original | Cleaned
    segOrig_ = new QPushButton("Original"); segOrig_->setObjectName("segLeft");
    segClean_ = new QPushButton("Cleaned"); segClean_->setObjectName("segRight");
    for (auto* b : {segOrig_, segClean_}) { b->setCheckable(true); b->setCursor(Qt::PointingHandCursor); }
    segOrig_->setChecked(true);
    connect(segOrig_, &QPushButton::clicked, this, [this]{ setPreviewCleaned(false); });
    connect(segClean_, &QPushButton::clicked, this, [this]{ setPreviewCleaned(true); });
    auto* seg = new QHBoxLayout; seg->setSpacing(0);
    seg->addWidget(segOrig_); seg->addWidget(segClean_);
    h->addLayout(seg);
    h->addSpacing(16);

    prevBtn_ = new QPushButton("‹"); prevBtn_->setObjectName("nav");
    nextBtn_ = new QPushButton("›"); nextBtn_->setObjectName("nav");
    prevBtn_->setCursor(Qt::PointingHandCursor); nextBtn_->setCursor(Qt::PointingHandCursor);
    connect(prevBtn_, &QPushButton::clicked, this, [this]{ if (cur_ > 0) showPage(cur_ - 1); });
    connect(nextBtn_, &QPushButton::clicked, this, [this]{ if (cur_ + 1 < (int)pages_.size()) showPage(cur_ + 1); });
    h->addWidget(prevBtn_); h->addWidget(nextBtn_);
    v->addWidget(bar);

    view_ = new PageView;
    v->addWidget(view_, 1);
    return center;
}

// --------------------------------------------------------------------------
//  Inspector
// --------------------------------------------------------------------------
QWidget* MainWindow::buildInspector() {
    auto* insp = new QFrame; insp->setObjectName("inspector");
    insp->setFixedWidth(330);
    auto* v = new QVBoxLayout(insp);
    v->setContentsMargins(20, 22, 20, 18); v->setSpacing(8);

    v->addWidget(mk("Elements on this page", "sectionTitle"));
    inspectorHint_ = mk("Untick anything to delete it. The biggest image is "
                        "kept by default.", "hint");
    inspectorHint_->setWordWrap(true);
    v->addWidget(inspectorHint_);
    v->addSpacing(4);

    auto* scroll = new QScrollArea; scroll->setWidgetResizable(true);
    scroll->setObjectName("elemScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->viewport()->setStyleSheet("background: transparent;");
    elemContainer_ = new QWidget;
    elemContainer_->setObjectName("elemHost");
    elemContainer_->setStyleSheet("background: transparent;");
    elemLayout_ = new QVBoxLayout(elemContainer_);
    elemLayout_->setContentsMargins(0, 0, 6, 0); elemLayout_->setSpacing(8);
    elemLayout_->addStretch();
    scroll->setWidget(elemContainer_);
    v->addWidget(scroll, 1);

    applyAllChk_ = new QCheckBox("Apply my choices to every page");
    applyAllChk_->setChecked(true);
    connect(applyAllChk_, &QCheckBox::toggled, this, [this](bool on){
        applyAll_ = on;
        if (on && !pages_.empty()) { propagateFrom(cur_); buildSelection(); }
    });
    v->addWidget(applyAllChk_);

    autoBtn_ = new QPushButton("Reset to auto-detect");
    autoBtn_->setObjectName("ghost");
    autoBtn_->setCursor(Qt::PointingHandCursor);
    connect(autoBtn_, &QPushButton::clicked, this, [this]{ autoDetect(); });
    v->addWidget(autoBtn_);

    saveBtn_ = new QPushButton("Save cleaned PDF");
    saveBtn_->setObjectName("primary");
    saveBtn_->setCursor(Qt::PointingHandCursor);
    auto* glow = new QGraphicsDropShadowEffect(saveBtn_);
    glow->setBlurRadius(22); glow->setOffset(0, 6);
    glow->setColor(QColor(37, 99, 235, 90));
    saveBtn_->setGraphicsEffect(glow);
    connect(saveBtn_, &QPushButton::clicked, this, [this]{ saveDialog(); });
    v->addWidget(saveBtn_);

    status_ = mk("", "status"); status_->setWordWrap(true);
    v->addWidget(status_);
    return insp;
}

// --------------------------------------------------------------------------
//  Loading
// --------------------------------------------------------------------------
void MainWindow::openDialog() {
    QString f = QFileDialog::getOpenFileName(this, "Open PDF", QString(),
                                             "PDF files (*.pdf);;All files (*)");
    if (!f.isEmpty()) loadDocument(f);
}

bool MainWindow::loadDocument(const QString& path) {
    if (!doc_.open(path.toStdString())) {
        QMessageBox::critical(this, "Could not open PDF",
                              QString::fromStdString(doc_.lastError()));
        return false;
    }
    status_->setText("Analysing pages…");
    QApplication::processEvents();

    int n = doc_.pageCount();
    pages_.clear(); pages_.reserve(n);
    for (int i = 0; i < n; i++) pages_.push_back(doc_.analyzePage(i, opt_));

    docName_ = QFileInfo(path).fileName();
    cur_ = 0; cleaned_ = false;
    segOrig_->setChecked(true); segClean_->setChecked(false);

    building_ = true;
    pageList_->clear();
    QPixmap blank(116, 150); blank.fill(QColor("#EEF2F7"));
    for (int i = 0; i < n; i++) {
        auto* it = new QListWidgetItem(QIcon(blank), QString("Page %1").arg(i + 1));
        it->setTextAlignment(Qt::AlignHCenter | Qt::AlignBottom);
        pageList_->addItem(it);
    }
    pageList_->setCurrentRow(0);
    building_ = false;

    buildSelection();
    showPage(0);
    enqueueThumbnails();
    footer_->setText(docName_ + QString("  ·  %1 pages").arg(n));
    status_->setText("Ready · auto-detected the overlay layers to remove.");
    return true;
}

// --------------------------------------------------------------------------
//  Page display
// --------------------------------------------------------------------------
void MainWindow::showPage(int i) {
    if (i < 0 || i >= (int)pages_.size()) return;
    cur_ = i;
    building_ = true;
    if (pageList_->currentRow() != i) pageList_->setCurrentRow(i);
    building_ = false;

    pageTitle_->setText(docName_);
    const auto& pg = pages_[i];
    int overlays = 0; for (auto& e : pg.elements) if (!e.keep) overlays++;
    pageSubtitle_->setText(QString("Page %1 of %2  ·  %3 overlay element%4  ·  %5")
        .arg(i + 1).arg(pages_.size()).arg(overlays).arg(overlays == 1 ? "" : "s")
        .arg(pg.textChars > 0 ? QString("%1 chars of real text").arg(pg.textChars)
                              : QString("image-only page")));

    rebuildElements();
    renderCurrent();
    updateNav();
}

void MainWindow::renderCurrent() {
    if (pages_.empty()) return;
    const auto& pg = pages_[cur_];
    double zoom = 1500.0 / std::max(1.0, pg.width);
    zoom = std::min(std::max(zoom, 0.4), 3.0);
    pdfcore::Bitmap bmp = doc_.renderPage(cur_, zoom, cleaned_, opt_,
                                          cleaned_ ? &sel_ : nullptr);
    QImage img = toQImage(bmp);

    QVector<PageView::Highlight> hl;
    if (!cleaned_) {
        for (auto& e : pg.elements) {
            if (e.kind != "image") continue;
            bool first = true;
            for (auto& r : e.rects) {
                hl.push_back({ QRectF(r.x0, r.y0, r.x1 - r.x0, r.y1 - r.y0),
                               e.keep, first ? (e.keep ? "KEEP" : "DELETE") : QString() });
                first = false;
            }
        }
    }
    view_->setPage(img, pg.width, pg.height, hl, !cleaned_);
}

QString MainWindow::elementMeta(const pdfcore::Element& e) const {
    if (e.kind == "layer") return "optional-content layer";
    if (e.kind == "tilt")
        return QString("≈%1 rotated glyphs  ·  watermark text").arg(e.placements);
    // image
    QStringList parts;
    parts << (e.cover >= 0.01 ? QString("covers %1%").arg(qRound(e.cover * 100))
                              : QString("covers <1%"));
    if (e.placements > 1) parts << QString("tiled ×%1").arg(e.placements);
    if (e.cover >= 0.5)   // opacity is the deciding signal for full-cover images
        parts << (e.opacity >= 0.85 ? QString("opaque")
                  : QString("%1% opaque overlay").arg(qRound(e.opacity * 100)));
    return parts.join("  ·  ");
}

void MainWindow::rebuildHighlights() { renderCurrent(); }

void MainWindow::updateNav() {
    bool has = !pages_.empty();
    prevBtn_->setEnabled(has && cur_ > 0);
    nextBtn_->setEnabled(has && cur_ + 1 < (int)pages_.size());
    saveBtn_->setEnabled(has);
    segOrig_->setEnabled(has); segClean_->setEnabled(has);
}

// --------------------------------------------------------------------------
//  Element list
// --------------------------------------------------------------------------
void MainWindow::rebuildElements() {
    building_ = true;
    // clear (keep the trailing stretch)
    while (elemLayout_->count() > 1) {
        QLayoutItem* it = elemLayout_->takeAt(0);
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    if (pages_.empty()) { building_ = false; return; }

    auto& els = pages_[cur_].elements;
    for (int idx = 0; idx < (int)els.size(); ++idx) {
        auto& e = els[idx];
        auto* card = new ClickCard; card->setObjectName("elcard");
        card->setProperty("state", e.keep ? "keep" : "drop");
        auto* row = new QHBoxLayout(card);
        row->setContentsMargins(12, 10, 12, 10); row->setSpacing(11);

        auto* chk = new QCheckBox; chk->setChecked(e.keep);
        chk->setFocusPolicy(Qt::NoFocus);
        row->addWidget(chk, 0, Qt::AlignVCenter);

        auto* txt = new QVBoxLayout; txt->setSpacing(2);
        txt->addWidget(mk(QString::fromStdString(e.label), "elLabel"));
        txt->addWidget(mk(elementMeta(e), "elMeta"));
        row->addLayout(txt); row->addStretch();

        auto* badge = mk(e.keep ? "KEEP" : "DELETE", e.keep ? "badgeKeep" : "badgeDrop");
        row->addWidget(badge, 0, Qt::AlignVCenter);

        auto apply = [this, idx, badge, card, chk](bool on){
            if (building_) return;
            pages_[cur_].elements[idx].keep = on;
            badge->setText(on ? "KEEP" : "DELETE");
            badge->setObjectName(on ? "badgeKeep" : "badgeDrop");
            badge->style()->unpolish(badge); badge->style()->polish(badge);
            card->setProperty("state", on ? "keep" : "drop");
            card->style()->unpolish(card); card->style()->polish(card);
            if (applyAll_) propagateFrom(cur_);
            buildSelection();
            renderCurrent();
        };
        connect(chk, &QCheckBox::toggled, this, apply);
        // clicking anywhere on the card flips the checkbox (which runs `apply`)
        card->onClick = [chk]{ chk->toggle(); };

        elemLayout_->insertWidget(elemLayout_->count() - 1, card);
    }
    building_ = false;
}

// --------------------------------------------------------------------------
//  Selection model
// --------------------------------------------------------------------------
void MainWindow::buildSelection() {
    sel_.keep.assign(pages_.size(), {});
    for (size_t p = 0; p < pages_.size(); ++p)
        for (auto& e : pages_[p].elements)
            if (e.kind == "image" && e.keep)
                sel_.keep[p].push_back({e.imgW, e.imgH, e.cover, e.opacity});
    // OC + diagonal-text are global toggles; derive from the current page
    bool oc = true, diag = true;
    if (cur_ < (int)pages_.size())
        for (auto& e : pages_[cur_].elements) {
            if (e.kind == "layer" && e.keep) oc = false;
            if (e.kind == "tilt"  && e.keep) diag = false;
        }
    sel_.removeOptionalContent = oc;
    sel_.removeDiagonalText = diag;
    sel_.version++;
}

void MainWindow::propagateFrom(int src) {
    if (src < 0 || src >= (int)pages_.size()) return;
    auto& ref = pages_[src].elements;
    for (size_t q = 0; q < pages_.size(); ++q) {
        if ((int)q == src) continue;
        auto& es = pages_[q].elements;
        for (size_t i = 0; i < es.size() && i < ref.size(); ++i)
            if (es[i].kind == ref[i].kind) es[i].keep = ref[i].keep;
    }
}

void MainWindow::autoDetect() {
    if (pages_.empty()) return;
    auto reset = [](pdfcore::PageInfo& pg){
        for (auto& e : pg.elements) e.keep = (e.role == pdfcore::Role::Content);
    };
    if (applyAll_) for (auto& pg : pages_) reset(pg);
    else reset(pages_[cur_]);
    buildSelection();
    showPage(cur_);
}

void MainWindow::setPreviewCleaned(bool on) {
    cleaned_ = on;
    segOrig_->setChecked(!on); segClean_->setChecked(on);
    if (!pages_.empty()) { buildSelection(); renderCurrent(); }
}

// --------------------------------------------------------------------------
//  Save
// --------------------------------------------------------------------------
void MainWindow::saveDialog() {
    if (pages_.empty()) return;
    QString def = QFileInfo(docName_).completeBaseName() + "_cleaned.pdf";
    QString out = QFileDialog::getSaveFileName(this, "Save cleaned PDF", def,
                                               "PDF files (*.pdf)");
    if (out.isEmpty()) return;
    buildSelection();
    status_->setText("Saving…"); QApplication::processEvents();
    pdfcore::Document::Stats st;
    if (!doc_.saveStripped(out.toStdString(), opt_, &sel_, &st)) {
        QMessageBox::critical(this, "Save failed",
                              QString::fromStdString(doc_.lastError()));
        status_->setText("Save failed.");
        return;
    }
    status_->setText(QString("Saved · %1 pages cleaned · %2 layers removed")
                         .arg(st.pages).arg(st.layersEmptied));
    QMessageBox::information(this, "Done", "Cleaned PDF saved to:\n" + out);
}

// --------------------------------------------------------------------------
//  Thumbnails (lazy, off the event loop)
// --------------------------------------------------------------------------
void MainWindow::enqueueThumbnails() {
    thumbQueue_.clear();
    for (int i = 0; i < (int)pages_.size(); ++i) thumbQueue_.push_back(i);
    QTimer::singleShot(0, this, [this]{ loadNextThumbnail(); });
}

void MainWindow::loadNextThumbnail() {
    if (thumbQueue_.isEmpty()) return;
    int i = thumbQueue_.front(); thumbQueue_.pop_front();
    if (i < (int)pages_.size()) {
        double zoom = 116.0 / std::max(1.0, pages_[i].width);
        pdfcore::Bitmap b = doc_.renderPage(i, zoom, false, opt_, nullptr);
        QImage img = toQImage(b);
        if (!img.isNull() && i < pageList_->count())
            pageList_->item(i)->setIcon(QPixmap::fromImage(img));
    }
    if (!thumbQueue_.isEmpty())
        QTimer::singleShot(0, this, [this]{ loadNextThumbnail(); });
}
