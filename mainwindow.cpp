#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QFileSystemModel>
#include <QDebug>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QSphereMesh>
#include <Qt3DRender/QCamera>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QMesh>
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QProcess>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Creare obiect MyOpenGLWidget
    viewerWidget = new MyOpenGLWidget(this);

    // Înlocuiește widget-ul placeholder din layout cu viewerWidget
    QVBoxLayout *layoutViewer = new QVBoxLayout(ui->viewer);  // ui->viewerPlaceholder este widgetul placeholder din UI
    layoutViewer->addWidget(viewerWidget);
    viewerWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layoutViewer->setContentsMargins(0, 0, 0, 0);
    ui->viewer->setLayout(layoutViewer);

    // Create object MyOpenGLWidget
    sceneWidget = new MyOpenGLWidget(this);

    // inlocuire placeholder din layout cu sceneWidget
    QVBoxLayout *layoutScene = new QVBoxLayout(ui->scene);
    layoutScene->addWidget(sceneWidget);
    sceneWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layoutScene->setContentsMargins(0, 0, 0, 0);
    ui->scene->setLayout(layoutScene);

    // Create a filesystem viewer for the Models tab
    QString rootPath = QCoreApplication::applicationDirPath() + "/../../../Models";

    QFileSystemModel *fs = new QFileSystemModel();
    fs->setRootPath(rootPath);
    ui->treeView->setModel(fs);
    ui->treeView->setRootIndex(fs->index(rootPath));

    fs->setNameFilters(QStringList() << "*.obj" << "*.fbx");
    fs->setNameFilterDisables(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_generate_clicked()
{
    QString inputTextFromUser = ui->inputText->toPlainText();

    QProcess process;
    QString scriptDirPath = QCoreApplication::applicationDirPath() + "/../../../NLPprocessing";
    QString scriptPath = scriptDirPath + "/processInput.py";
    QStringList arguments;
    arguments << inputTextFromUser;
    qDebug() << "Arguments:" << arguments;
    QStringList parameters = QStringList() << scriptPath << arguments;
    qDebug() << "Parameters" << parameters;

    process.start("python", QStringList() << scriptPath << arguments);

    if (!process.waitForFinished()) {
        qDebug() << "Script failed to start";
        qDebug() << "Error: " << process.errorString();
    } else {
        QByteArray  standardOutput  = process.readAllStandardOutput();
        QByteArray  standardError  = process.readAllStandardError();

        qDebug() << "Standard Output: " << QString(standardOutput );
        qDebug() << "Standard Error: " << QString(standardError );
    }

    qDebug() << process.exitStatus();
    if (process.exitStatus() == QProcess::NormalExit)
    {
        QString outputDir = QCoreApplication::applicationDirPath() + "/../temp";
        QString jsonFilePath = outputDir + "/scene.json";

        qDebug() << jsonFilePath;

        sceneWidget->loadScene(jsonFilePath);
    }
    else
    {
        QMessageBox::warning(this, "Error", "Python script execution failed.");
    }

}


void MainWindow::on_save_clicked()
{

}


void MainWindow::on_clear_clicked()
{
    sceneWidget->clearScene();
}


void MainWindow::on_load_clicked()
{

}


void MainWindow::on_viewModel_clicked()
{
    QModelIndex index = ui->treeView->currentIndex();
    if (!index.isValid())
        return;

    QString filePath = ui->treeView->model()->data(index, QFileSystemModel::FilePathRole).toString();

    QFileInfo fileInfo(filePath);
    if (fileInfo.exists() && fileInfo.isFile())
    {
        viewerWidget->loadModel(filePath);
        qDebug() << "Loading the selected file.";
    }
    else
    {
        qDebug() << "Please select a valid file.";
    }
}


void MainWindow::on_deleteModel_clicked()
{
    QModelIndex index = ui->treeView->currentIndex();
    if (!index.isValid())
        return;

    QString filePath = ui->treeView->model()->data(index, QFileSystemModel::FilePathRole).toString();

    QFileInfo fileInfo(filePath);
    if (fileInfo.exists())
    {
        // Confirmare pentru ștergerea fișierului
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Delete resource", "Are you sure you want to delete this resource?",
                                      QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::Yes) {

            if (fileInfo.isFile())
            {
                if (QFile::remove(filePath)) {
                    QMessageBox::information(this, "Success", "File deleted successfully.");

                    // Dacă fișierul șters este cel încărcat în scenă, șterge modelul din scenă
                    viewerWidget->clearScene();
                } else {
                    qDebug() << "Couldn't detele this file.";
                    QMessageBox::critical(this, "Error", "The file couldn't be deleted.");
                }
            }
            else if (fileInfo.isDir())
            {
                QDir dir(filePath);

                if  (dir.removeRecursively())
                {
                    QMessageBox::information(this, "Success", "Directory deleted successfully.");

                    // Dacă fișierul șters este cel încărcat în scenă, șterge modelul din scenă
                    viewerWidget->clearScene();
                }
                else
                {
                    QMessageBox::critical(this, "Error", "The directory couldn't be deleted.");
                }
            }
        }
    }
}


void MainWindow::on_importModel_clicked()
{
    // Deschide fereastra de dialog pentru selectarea unui fișier sau folder
    QString selectedPath = QFileDialog::getExistingDirectory(this, tr("Select a file or a directory for import"),
                                                             QDir::homePath(),
                                                             QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (selectedPath.isEmpty()) {
        // Dacă utilizatorul nu a selectat nimic, ieși din metodă
        return;
    }

    // Directorul unde importurile vor fi stocate
    QString importDirPath = QCoreApplication::applicationDirPath() + "/../../../Models/imports/";

    QDir importDir(importDirPath);
    if (!importDir.exists()) {
        // Crează directorul "imports" dacă nu există deja
        importDir.mkpath(".");
    }

    QFileInfo selectedInfo(selectedPath);
    if (selectedInfo.isDir()) {
        // Dacă utilizatorul a selectat un folder, copiază tot folderul și conținutul acestuia
        QString destinationPath = importDirPath + selectedInfo.fileName();
        if (QDir(destinationPath).exists()) {
            QMessageBox::critical(this, tr("Error"), tr("There already is a resource with the same name in imports."));
            return;
        }
        if (!copyDirectoryRecursively(selectedPath, destinationPath)) {
            QMessageBox::critical(this, tr("Error"), tr("Could not import resource."));
        } else {
            QMessageBox::information(this, tr("Success"), tr("Resource imported successfuly."));
        }
    } else if (selectedInfo.isFile()) {
        // Dacă utilizatorul a selectat un fișier, copiază fișierul în directorul "imports"
        QString destinationFilePath = importDirPath + selectedInfo.fileName();
        if (QFile::exists(destinationFilePath)) {
            QMessageBox::critical(this, tr("Error"), tr("There already is a resource with the same name in imports."));
            return;
        }
        if (QFile::copy(selectedPath, destinationFilePath)) {
            QMessageBox::information(this, tr("Success"), tr("Resource imported successfuly."));
        } else {
            QMessageBox::critical(this, tr("Error"), tr("Could not import resource."));
        }
    }
}

// Funcție auxiliară pentru copierea recursivă a unui director
bool MainWindow::copyDirectoryRecursively(const QString &sourceDirPath, const QString &destDirPath)
{
    QDir sourceDir(sourceDirPath);
    QDir destDir(destDirPath);

    if (!sourceDir.exists()) {
        return false;
    }

    destDir.mkpath(destDirPath);

    QFileInfoList fileList = sourceDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fileInfo : fileList) {
        QString destFilePath = destDir.filePath(fileInfo.fileName());
        if (!QFile::copy(fileInfo.filePath(), destFilePath)) {
            return false;
        }
    }

    QFileInfoList dirList = sourceDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &dirInfo : dirList) {
        QString subDirPath = destDir.filePath(dirInfo.fileName());
        if (!copyDirectoryRecursively(dirInfo.filePath(), subDirPath)) {
            return false;
        }
    }

    return true;
}
