// main.cpp — StripItPdf entry point.
#include <QApplication>
#include <QFontDatabase>
#include <QFile>
#include <QIcon>
#include <QTimer>
#include "MainWindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("StripItPdf");
    QApplication::setOrganizationName("StripItPdf");
    QApplication::setWindowIcon(QIcon(":/logo.png"));

    QFontDatabase::addApplicationFont(":/fonts/PlusJakartaSans.ttf");
    QFontDatabase::addApplicationFont(":/fonts/Outfit.ttf");
    QFont base("Plus Jakarta Sans", 10);
    app.setFont(base);

    QFile qss(":/style.qss");
    if (qss.open(QFile::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));

    MainWindow w;
    w.show();

    const QStringList args = app.arguments();
    if (args.size() > 1 && QFile::exists(args.at(1)))
        w.loadDocument(args.at(1));

    // Headless verification: STRIPIT_SHOT=path grabs the window then quits.
    const QString shot = qEnvironmentVariable("STRIPIT_SHOT");
    if (!shot.isEmpty()) {
        if (!qEnvironmentVariable("STRIPIT_CLEAN").isEmpty()) w.setPreviewCleaned(true);
        QTimer::singleShot(1500, &app, [&]{ w.grab().save(shot); app.quit(); });
    }
    return app.exec();
}
