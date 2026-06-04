// MainWindow.h — StripItPdf layer editor main window.
#pragma once
#include <QMainWindow>
#include <QVector>
#include <vector>
#include "PdfCore.h"

class QListWidget;
class QLabel;
class QPushButton;
class QCheckBox;
class QVBoxLayout;
class QWidget;
class PageView;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // exposed so a headless screenshot harness can drive the UI
    bool loadDocument(const QString& path);
    void setPreviewCleaned(bool on);
    PageView* canvas() const { return view_; }

private:
    QWidget* buildSidebar();
    QWidget* buildCenter();
    QWidget* buildInspector();

    void openDialog();
    void showPage(int i);
    void renderCurrent();
    void rebuildElements();
    QString elementMeta(const pdfcore::Element& e) const;
    void rebuildHighlights();
    void buildSelection();
    void propagateFrom(int srcPage);
    void autoDetect();
    void saveDialog();
    void updateNav();
    void enqueueThumbnails();
    void loadNextThumbnail();

    // model
    pdfcore::Document doc_;
    pdfcore::StripOptions opt_;
    pdfcore::Selection sel_;
    std::vector<pdfcore::PageInfo> pages_;
    int cur_ = 0;
    bool cleaned_ = false;
    bool applyAll_ = true;
    bool building_ = false;
    QString docName_;
    QVector<int> thumbQueue_;

    // widgets
    QListWidget* pageList_ = nullptr;
    QLabel* pageTitle_ = nullptr;
    QLabel* pageSubtitle_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* inspectorHint_ = nullptr;
    QLabel* footer_ = nullptr;
    QPushButton* prevBtn_ = nullptr;
    QPushButton* nextBtn_ = nullptr;
    QPushButton* segOrig_ = nullptr;
    QPushButton* segClean_ = nullptr;
    QPushButton* saveBtn_ = nullptr;
    QPushButton* autoBtn_ = nullptr;
    QCheckBox* applyAllChk_ = nullptr;
    PageView* view_ = nullptr;
    QWidget* elemContainer_ = nullptr;
    QVBoxLayout* elemLayout_ = nullptr;
};
