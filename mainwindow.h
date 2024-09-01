#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "myopenglwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_generate_clicked();

    void on_save_clicked();

    void on_clear_clicked();

    void on_load_clicked();

    void on_viewModel_clicked();

    void on_deleteModel_clicked();

    void on_importModel_clicked();

private:
    bool copyDirectoryRecursively(const QString &sourceDirPath, const QString &destDirPath);

    Ui::MainWindow *ui;
    MyOpenGLWidget *viewerWidget;
    MyOpenGLWidget *sceneWidget;
};
#endif // MAINWINDOW_H
