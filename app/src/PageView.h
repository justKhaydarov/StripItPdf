// PageView.h — canvas that paints a rendered page plus keep/delete highlights.
#pragma once
#include <QWidget>
#include <QImage>
#include <QVector>
#include <QRectF>
#include <QString>

class PageView : public QWidget {
public:
    struct Highlight { QRectF rectPts; bool keep; QString tag; };

    explicit PageView(QWidget* parent = nullptr);

    void setPlaceholder(const QString& text);
    void setPage(const QImage& img, double pageWpts, double pageHpts,
                 const QVector<Highlight>& highlights, bool showHighlights);
    void clear();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QRectF pageDrawRect() const;   // where the page image lands, in widget coords

    QImage img_;
    double pageW_ = 0, pageH_ = 0;
    QVector<Highlight> highlights_;
    bool showHi_ = true;
    QString placeholder_ = "Open a PDF to begin";
};
