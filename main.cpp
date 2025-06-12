#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QImageReader>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    QList<QByteArray> formats = QImageReader::supportedImageFormats();

    qDebug() << "Supported formats: " << formats;
    return a.exec();
}
