#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QImageReader>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFile stylesheetFile(QCoreApplication::applicationDirPath() + "/../../..//MyStylesheet.qss");
    stylesheetFile.open(QFile::ReadOnly);
    QString styleSheet = QString::fromUtf8(stylesheetFile.readAll());
    a.setStyleSheet(styleSheet);
    // stylesheetFile.close();

    MainWindow w;
    w.show();
    QList<QByteArray> formats = QImageReader::supportedImageFormats();

    qDebug() << "Supported formats: " << formats;
    return a.exec();
}
