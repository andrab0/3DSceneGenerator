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
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <QDirIterator>
#include <QDateTime>

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

    appSettings = new QSettings("YourCompany", "YourApp", this);
    currentLanguageCode = "en"; // Default

    setupSettingsTab();
    loadSettings();
}

MainWindow::~MainWindow()
{
    delete ui;
}

// void MainWindow::on_generate_clicked()
// {
//     QString inputTextFromUser = ui->inputText->toPlainText();

//     QProcess process;
//     QString scriptDirPath = QCoreApplication::applicationDirPath() + "/../../../NLPprocessing";
//     QString scriptPath = scriptDirPath + "/processInput.py";
//     QStringList arguments;
//     arguments << inputTextFromUser;
//     qDebug() << "Arguments:" << arguments;
//     QStringList parameters = QStringList() << scriptPath << arguments;
//     qDebug() << "Parameters" << parameters;

//     process.start("python", QStringList() << scriptPath << arguments);

//     if (!process.waitForFinished()) {
//         qDebug() << "Script failed to start";
//         qDebug() << "Error: " << process.errorString();
//     } else {
//         QByteArray  standardOutput  = process.readAllStandardOutput();
//         QByteArray  standardError  = process.readAllStandardError();

//         qDebug() << "Standard Output: " << QString(standardOutput );
//         qDebug() << "Standard Error: " << QString(standardError );
//     }

//     qDebug() << process.exitStatus();
//     if (process.exitStatus() == QProcess::NormalExit)
//     {
//         QString outputDir = QCoreApplication::applicationDirPath() + "/../temp";
//         QString jsonFilePath = outputDir + "/scene.json";

//         qDebug() << jsonFilePath;

//         sceneWidget->loadScene(jsonFilePath);
//     }
//     else
//     {
//         QMessageBox::warning(this, "Error", "Python script execution failed.");
//     }

// }

void MainWindow::on_generate_clicked()
{
    QString inputText = ui->inputText->toPlainText();
    QString scriptPath = QCoreApplication::applicationDirPath() + "/../../../NLPprocessing/inp.py";

    qDebug() << "Running Python script:" << scriptPath;
    qDebug() << "Input text:" << inputText;

    if (inputText.isEmpty())
    {
        QMessageBox::warning(this, "Error", "Please enter a description for the scene.");
        return;
    }

    progressDialog = new QProgressDialog("Generating scene, please wait...", nullptr, 0, 100, this);
    progressDialog->setWindowModality(Qt::ApplicationModal);
    progressDialog->setCancelButton(nullptr);
    progressDialog->setMinimumDuration(200);
    progressDialog->setValue(50); // Setează un progres intermediar

    ScriptRunner *scriptRunner = new ScriptRunner(scriptPath, inputText, this);
    connect(scriptRunner, &ScriptRunner::scriptFinished, this, &MainWindow::on_scriptFinished);
    connect(scriptRunner, &QThread::finished, scriptRunner, &QObject::deleteLater);

    scriptRunner->start();
}

// void MainWindow::on_scriptFinished(const QString &outputFile, bool success)
// {
//     progressDialog->setValue(100);  // Marchează finalizarea procesului
//     progressDialog->hide();
//     delete progressDialog;

//     if (!success)
//     {
//         QMessageBox::warning(this, "Error", "Failed to generate scene.");
//         return;
//     }

//     QString fixedOutputFile = QCoreApplication::applicationDirPath() + "/../../../temp/scene_output.json";
//     fixedOutputFile = fixedOutputFile.replace("\\", "/"); // Normalizează calea

//     qDebug() << "Script finished, JSON file path:" << fixedOutputFile;

//     QFile jsonFile(fixedOutputFile);
//     if (!jsonFile.exists())
//     {
//         QMessageBox::warning(this, "Error", "JSON file not found at: " + fixedOutputFile);
//         return;
//     }

//     sceneWidget->loadScene(fixedOutputFile);
// }
void MainWindow::on_scriptFinished(const QString &outputFile, bool success)
{
    progressDialog->setValue(100);
    progressDialog->hide();
    delete progressDialog;

    if (!success)
    {
        QMessageBox::warning(this, "Error", "Failed to generate scene.");
        return;
    }

    QString fixedOutputFile = QCoreApplication::applicationDirPath() + "/../../../temp/scene_output.json";
    fixedOutputFile = fixedOutputFile.replace("\\", "/");

    qDebug() << "Script finished, JSON file path:" << fixedOutputFile;

    QFile jsonFile(fixedOutputFile);
    if (!jsonFile.exists())
    {
        QMessageBox::warning(this, "Error", "JSON file not found at: " + fixedOutputFile);
        return;
    }

    // Citește și păstrează JSON-ul pentru salvare ulterioară
    if (jsonFile.open(QIODevice::ReadOnly))
    {
        currentSceneJson = jsonFile.readAll();
        jsonFile.close();
    }

    sceneWidget->loadScene(fixedOutputFile);
}

// void MainWindow::on_save_clicked()
// {

// }
void MainWindow::on_save_clicked()
{
    if (currentSceneJson.isEmpty())
    {
        QMessageBox::warning(this, "Error", "No scene to save. Please generate a scene first.");
        return;
    }

    // Deschide dialog pentru salvarea fișierului
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Save Scene"),
        QDir::homePath() + "/scene.json",
        tr("JSON Files (*.json);;All Files (*)")
    );

    if (fileName.isEmpty())
    {
        return; // Utilizatorul a anulat
    }

    // Parsează JSON-ul existent
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(currentSceneJson.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        QMessageBox::critical(this, "Error", "Failed to parse current scene data: " + parseError.errorString());
        return;
    }

    QJsonObject sceneObject = doc.object();

    // Adaugă textul din input box
    QString inputText = ui->inputText->toPlainText();
    sceneObject["user_input"] = inputText;

    // Adaugă metadate pentru salvare
    sceneObject["saved_timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    sceneObject["app_version"] = "1.0"; // sau versiunea aplicației tale

    // Creează noul document JSON
    QJsonDocument saveDoc(sceneObject);

    // Salvează fișierul
    QFile saveFile(fileName);
    if (!saveFile.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(this, "Error", "Could not open file for writing: " + saveFile.errorString());
        return;
    }

    saveFile.write(saveDoc.toJson());
    saveFile.close();

    QMessageBox::information(this, "Success", "Scene saved successfully to:\n" + fileName);
}


void MainWindow::on_clear_clicked()
{
    sceneWidget->clearScene();
}


// void MainWindow::on_load_clicked()
// {

// }

void MainWindow::on_load_clicked()
{
    // Deschide dialog pentru încărcarea fișierului
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Load Scene"),
        QDir::homePath(),
        tr("JSON Files (*.json);;All Files (*)")
    );

    if (fileName.isEmpty())
    {
        return; // Utilizatorul a anulat
    }

    // Citește fișierul
    QFile loadFile(fileName);
    if (!loadFile.open(QIODevice::ReadOnly))
    {
        QMessageBox::critical(this, "Error", "Could not open file for reading: " + loadFile.errorString());
        return;
    }

    QByteArray jsonData = loadFile.readAll();
    loadFile.close();

    // Parsează JSON-ul
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        QMessageBox::critical(this, "Error", "Failed to parse JSON file: " + parseError.errorString());
        return;
    }

    QJsonObject sceneObject = doc.object();

    // Verifică dacă este un fișier de scenă valid
    if (!sceneObject.contains("objects") && !sceneObject.contains("scene"))
    {
        QMessageBox::warning(this, "Error", "The selected file does not appear to be a valid scene file.");
        return;
    }

    // Extrage și setează textul de input dacă există
    if (sceneObject.contains("user_input"))
    {
        QString userInput = sceneObject["user_input"].toString();
        ui->inputText->setPlainText(userInput);
    }
    else
    {
        // Dacă nu există user_input, golește text box-ul
        ui->inputText->clear();
        QMessageBox::information(this, "Info", "Scene loaded successfully, but no input text was found in the file.");
    }

    // Creează un fișier temporar pentru încărcarea scenei
    QString tempSceneFile = QCoreApplication::applicationDirPath() + "/../../../temp/loaded_scene.json";

    // Creează directorul temp dacă nu există
    QDir tempDir = QFileInfo(tempSceneFile).absoluteDir();
    if (!tempDir.exists())
    {
        tempDir.mkpath(".");
    }

    // Salvează JSON-ul pentru încărcare (fără user_input pentru renderer)
    QJsonObject renderObject = sceneObject;
    renderObject.remove("user_input");
    renderObject.remove("saved_timestamp");
    renderObject.remove("app_version");

    QJsonDocument renderDoc(renderObject);
    QFile tempFile(tempSceneFile);
    if (!tempFile.open(QIODevice::WriteOnly))
    {
        QMessageBox::critical(this, "Error", "Could not create temporary file for scene rendering.");
        return;
    }

    tempFile.write(renderDoc.toJson());
    tempFile.close();

    // Păstrează JSON-ul curent pentru salvări ulterioare
    currentSceneJson = renderDoc.toJson();

    // Încarcă scena în renderer
    sceneWidget->loadScene(tempSceneFile);

    QMessageBox::information(this, "Success", "Scene loaded successfully!");
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
    // Întreabă utilizatorul ce tip de import dorește
    QMessageBox::StandardButton choice = QMessageBox::question(
        this, tr("Import Type"),
        tr("What would you like to import?\n\nYes = Individual Files\nNo = Directory (with all contents)"),
        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
    );

    if (choice == QMessageBox::Cancel) {
        return;
    }

    if (choice == QMessageBox::Yes) {
        // Import fișiere individuale
        QStringList selectedFiles = QFileDialog::getOpenFileNames(
            this,
            tr("Select 3D Model Files"),
            QDir::homePath(),
            tr("3D Model Files (*.obj *.fbx *.gltf *.glb *.3ds *.dae *.ply *.stl);;All Files (*)")
        );

        if (!selectedFiles.isEmpty()) {
            importFiles(selectedFiles);
        }
    } else {
        // Import director
        QString selectedDir = QFileDialog::getExistingDirectory(
            this,
            tr("Select Directory to Import"),
            QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
        );

        if (!selectedDir.isEmpty()) {
            importDirectory(selectedDir);
        }
    }
}

// Funcție pentru importul de fișiere
void MainWindow::importFiles(const QStringList &filePaths)
{
    QString primitivesDirPath = QCoreApplication::applicationDirPath() + "/../../../Models/primitives/";

    QDir primitivesDir(primitivesDirPath);
    if (!primitivesDir.exists()) {
        primitivesDir.mkpath(".");
    }

    int successCount = 0;
    int totalCount = filePaths.size();
    QStringList failedFiles;
    bool overwriteAll = false;
    bool skipAll = false;

    for (const QString &filePath : filePaths) {
        QFileInfo fileInfo(filePath);
        if (!fileInfo.isFile()) continue;

        QString destinationPath = primitivesDirPath + fileInfo.fileName();

        if (QFile::exists(destinationPath) && !overwriteAll && !skipAll) {
            QMessageBox msgBox;
            msgBox.setWindowTitle(tr("File Exists"));
            msgBox.setText(tr("File '%1' already exists in primitives.").arg(fileInfo.fileName()));
            msgBox.setInformativeText(tr("What would you like to do?"));

            QPushButton *overwriteBtn = msgBox.addButton(tr("Overwrite"), QMessageBox::AcceptRole);
            QPushButton *overwriteAllBtn = msgBox.addButton(tr("Overwrite All"), QMessageBox::AcceptRole);
            QPushButton *skipBtn = msgBox.addButton(tr("Skip"), QMessageBox::RejectRole);
            QPushButton *skipAllBtn = msgBox.addButton(tr("Skip All"), QMessageBox::RejectRole);
            msgBox.addButton(QMessageBox::Cancel);

            msgBox.exec();

            if (msgBox.clickedButton() == overwriteAllBtn) {
                overwriteAll = true;
            } else if (msgBox.clickedButton() == skipBtn) {
                continue;
            } else if (msgBox.clickedButton() == skipAllBtn) {
                skipAll = true;
                continue;
            } else if (msgBox.clickedButton() != overwriteBtn) {
                break; // Cancel
            }
        }

        if (skipAll) continue;

        if (QFile::exists(destinationPath)) {
            QFile::remove(destinationPath);
        }

        if (QFile::copy(filePath, destinationPath)) {
            successCount++;
        } else {
            failedFiles.append(fileInfo.fileName());
        }
    }

    // Afișează rezultatul
    if (successCount == totalCount) {
        QMessageBox::information(this, tr("Success"),
            tr("Successfully imported %1 file(s) to primitives.").arg(successCount));
    } else {
        QString message = tr("Imported %1 of %2 files.").arg(successCount).arg(totalCount);
        if (!failedFiles.isEmpty()) {
            message += tr("\nFailed files: %1").arg(failedFiles.join(", "));
        }
        QMessageBox::warning(this, tr("Import Result"), message);
    }
}

// Funcție pentru importul de directoare
void MainWindow::importDirectory(const QString &dirPath)
{
    QString primitivesDirPath = QCoreApplication::applicationDirPath() + "/../../../Models/primitives/";

    QDir primitivesDir(primitivesDirPath);
    if (!primitivesDir.exists()) {
        primitivesDir.mkpath(".");
    }

    // Colectează toate fișierele de model din director și subdirectoare
    QStringList modelFiles = collectModelFiles(dirPath);

    if (modelFiles.isEmpty()) {
        QMessageBox::information(this, tr("No Models Found"),
            tr("No supported 3D model files found in the selected directory."));
        return;
    }

    int successCount = 0;
    QStringList failedFiles;

    for (const QString &filePath : modelFiles) {
        QFileInfo fileInfo(filePath);
        QString destinationPath = primitivesDirPath + fileInfo.fileName();

        // Pentru import masiv, renumerotează automat fișierele duplicate
        if (QFile::exists(destinationPath)) {
            QString baseName = fileInfo.completeBaseName();
            QString extension = fileInfo.suffix();
            int counter = 1;

            do {
                destinationPath = primitivesDirPath + QString("%1_%2.%3")
                    .arg(baseName).arg(counter).arg(extension);
                counter++;
            } while (QFile::exists(destinationPath));
        }

        if (QFile::copy(filePath, destinationPath)) {
            successCount++;
        } else {
            failedFiles.append(fileInfo.fileName());
        }
    }

    // Afișează rezultatul
    QString message = tr("Successfully imported %1 of %2 model files from directory to primitives.")
        .arg(successCount).arg(modelFiles.size());

    if (!failedFiles.isEmpty()) {
        message += tr("\nFailed files: %1").arg(failedFiles.join(", "));
        QMessageBox::warning(this, tr("Import Complete"), message);
    } else {
        QMessageBox::information(this, tr("Import Complete"), message);
    }
}

// Funcție helper pentru colectarea fișierelor de model recursiv
QStringList MainWindow::collectModelFiles(const QString &dirPath)
{
    QStringList modelFiles;
    QStringList nameFilters;
    nameFilters << "*.obj" << "*.fbx" << "*.gltf" << "*.glb"
                << "*.3ds" << "*.dae" << "*.ply" << "*.stl";

    QDirIterator iterator(dirPath, nameFilters, QDir::Files, QDirIterator::Subdirectories);

    while (iterator.hasNext()) {
        modelFiles.append(iterator.next());
    }

    return modelFiles;
}

ScriptRunner::ScriptRunner(const QString &scriptPath, const QString &inputText, QObject *parent)
    : QThread(parent), scriptPath(scriptPath), inputText(inputText) {}

void ScriptRunner::run()
{
    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl("http://127.0.0.1:5000/process"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    json["text"] = inputText;

    MainWindow *mainWindow = qobject_cast<MainWindow*>(parent());
    if (mainWindow) {
        json["lang"] = mainWindow->getCurrentLanguageCode();
    } else {
        json["lang"] = "en"; // Fallback
    }

    QJsonDocument jsonDoc(json);
    QByteArray jsonData = jsonDoc.toJson();

    QNetworkReply *reply = manager.post(request, jsonData);
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError)
    {
        qDebug() << "HTTP request failed:" << reply->errorString();
        emit scriptFinished("", false);
        return;
    }

    QByteArray responseData = reply->readAll();
    qDebug() << "Received JSON response:" << responseData;

    QFile jsonFile("temp/scene.json");
    if (jsonFile.open(QIODevice::WriteOnly))
    {
        jsonFile.write(responseData);
        jsonFile.close();
    }

    emit scriptFinished("temp/scene.json", true);
}

void MainWindow::setupSettingsTab()
{
    // Adaugă limbile în combo box
    ui->lLanguageComboBox->addItem("English", "en");
    ui->lLanguageComboBox->addItem("Română", "ro");
    ui->lLanguageComboBox->addItem("Français", "fr");
    ui->lLanguageComboBox->addItem("Español", "es");
    ui->lLanguageComboBox->addItem("Deutsch", "de");
    ui->lLanguageComboBox->addItem("Italiano", "it");
    ui->lLanguageComboBox->addItem("Português", "pt");
    ui->lLanguageComboBox->addItem("中文", "zh");
    ui->lLanguageComboBox->addItem("日本語", "ja");
    ui->lLanguageComboBox->addItem("한국어", "ko");

    // Conectează signal-ul
    connect(ui->lLanguageComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onLanguageChanged);
}

void MainWindow::loadSettings()
{
    // Încarcă limba salvată sau folosește default-ul
    currentLanguageCode = appSettings->value("language", "en").toString();

    // Setează combo box-ul la limba corectă
    for (int i = 0; i < ui->lLanguageComboBox->count(); ++i) {
        if (ui->lLanguageComboBox->itemData(i).toString() == currentLanguageCode) {
            ui->lLanguageComboBox->setCurrentIndex(i);
            break;
        }
    }

    qDebug() << "Loaded language setting:" << currentLanguageCode;
}

void MainWindow::saveSettings()
{
    appSettings->setValue("language", currentLanguageCode);
    appSettings->sync();
    qDebug() << "Saved language setting:" << currentLanguageCode;
}

void MainWindow::onLanguageChanged(int index)
{
    QString newLanguageCode = ui->lLanguageComboBox->itemData(index).toString();

    if (newLanguageCode != currentLanguageCode) {
        currentLanguageCode = newLanguageCode;
        saveSettings();

        QString languageName = ui->lLanguageComboBox->itemText(index);
        qDebug() << "Language changed to:" << languageName << "(" << currentLanguageCode << ")";

        // Opțional: mesaj de confirmare
        QMessageBox::information(this, "Language Changed",
            QString("Language set to: %1").arg(languageName));
    }
}

QString MainWindow::getCurrentLanguageCode() const
{
    return currentLanguageCode.isEmpty() ? "en" : currentLanguageCode;
}
