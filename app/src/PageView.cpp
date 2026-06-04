#include "PageView.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

PageView::PageView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(360, 360);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void PageView::setPlaceholder(const QString& text) { placeholder_ = text; update(); }

void PageView::clear() { img_ = QImage(); highlights_.clear(); update(); }

void PageView::setPage(const QImage& img, double w, double h,
                       const QVector<Highlight>& hl, bool show) {
    img_ = img; pageW_ = w; pageH_ = h; highlights_ = hl; showHi_ = show;
    update();
}

QRectF PageView::pageDrawRect() const {
    if (img_.isNull() || pageW_ <= 0 || pageH_ <= 0) return QRectF();
    const double margin = 28.0;
    double availW = width() - 2 * margin;
    double availH = height() - 2 * margin;
    double scale = std::min(availW / pageW_, availH / pageH_);
    if (scale <= 0) scale = 0.01;
    double w = pageW_ * scale, h = pageH_ * scale;
    double x = (width() - w) / 2.0, y = (height() - h) / 2.0;
    return QRectF(x, y, w, h);
}

void PageView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.fillRect(rect(), QColor("#F8FAFC"));

    if (img_.isNull()) {
        p.setPen(QColor("#94A3B8"));
        QFont f = p.font(); f.setPointSizeF(13); p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, placeholder_);
        return;
    }

    QRectF dr = pageDrawRect();

    // soft drop shadow under the page
    for (int i = 10; i >= 1; --i) {
        double spread = i * 1.6;
        QColor c(15, 23, 42, std::max(0, 9 - i));   // tinted, very translucent
        p.setPen(Qt::NoPen); p.setBrush(c);
        QRectF s = dr.adjusted(-spread, -spread + 3, spread, spread + 5);
        p.drawRoundedRect(s, 10 + spread, 10 + spread);
    }

    // page itself on a white card
    p.setBrush(Qt::white); p.setPen(QPen(QColor("#E2E8F0"), 1));
    p.drawRoundedRect(dr, 8, 8);
    p.drawImage(dr, img_);

    if (!showHi_) return;

    double sx = dr.width() / pageW_, sy = dr.height() / pageH_;
    for (const auto& h : highlights_) {
        QRectF r(dr.x() + h.rectPts.x() * sx, dr.y() + h.rectPts.y() * sy,
                 h.rectPts.width() * sx, h.rectPts.height() * sy);
        QColor col = h.keep ? QColor("#16A34A") : QColor("#EF4444");
        QColor fill = col; fill.setAlpha(28);
        p.setBrush(fill);
        p.setPen(QPen(col, 2));
        p.drawRoundedRect(r, 4, 4);

        if (!h.tag.isEmpty() && r.width() > 46 && r.height() > 16) {
            QFont f = p.font(); f.setPointSizeF(7.5); f.setBold(true); p.setFont(f);
            QFontMetricsF fm(f);
            QString t = h.tag;
            double tw = fm.horizontalAdvance(t) + 10, th = fm.height() + 2;
            QRectF tag(r.x(), r.y(), tw, th);
            p.setPen(Qt::NoPen); p.setBrush(col);
            p.drawRoundedRect(tag, 3, 3);
            p.setPen(Qt::white);
            p.drawText(tag, Qt::AlignCenter, t);
        }
    }
}
