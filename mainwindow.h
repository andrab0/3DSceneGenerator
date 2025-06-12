#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QProgressDialog>
#include "myopenglwidget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class ScriptRunner : public QThread
{
    Q_OBJECT

public:
    explicit ScriptRunner(const QString &scriptPath, const QString &inputText, QObject *parent = nullptr);
    void run() override;

signals:
    void scriptFinished(const QString &outputFile, bool success);

private:
    QString scriptPath;
    QString inputText;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    QString getCurrentLanguageCode() const;


private slots:
    // bool copyDirectoryRecursively(const QString &sourceDirPath, const QString &destDirPath);
    void on_generate_clicked();

    void on_save_clicked();

    void on_clear_clicked();

    void on_load_clicked();

    void on_viewModel_clicked();

    void on_deleteModel_clicked();

    void on_importModel_clicked();

    void on_scriptFinished(const QString &outputFile, bool success);

    void onLanguageChanged(int index);

private:
    void importFiles(const QStringList &filePaths);
    void importDirectory(const QString &dirPath);

    QStringList collectModelFiles(const QString &dirPath);
    Ui::MainWindow *ui;
    MyOpenGLWidget *viewerWidget;
    MyOpenGLWidget *sceneWidget;
    QProgressDialog *progressDialog;
    QString currentSceneJson;

    QSettings *appSettings;
    QString currentLanguageCode;

    void setupSettingsTab();
    void loadSettings();
    void saveSettings();
};
#endif // MAINWINDOW_H
