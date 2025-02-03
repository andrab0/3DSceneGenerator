#include "myopenglwidget.h"
#include <QOpenGLShaderProgram>
#include <QVBoxLayout>
#include <Qt3DCore/QEntity>
#include <Qt3DCore/QTransform>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QMesh>
#include <Qt3DRender/QPointLight>
#include <Qt3DRender/QDirectionalLight>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QOrbitCameraController>
#include <Qt3DExtras/QForwardRenderer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonValue>

MyOpenGLWidget::MyOpenGLWidget(QWidget *parent)
    : QWidget(parent)
{
    // Configurare Qt3DWindow
    view = new Qt3DExtras::Qt3DWindow();
    view->defaultFrameGraph()->setClearColor(QColor(QRgb(0x4d4d4f)));

    // Creare rootEntity
    rootEntity = new Qt3DCore::QEntity();

    // Setarea entității root în Qt3DWindow
    view->setRootEntity(rootEntity);

    // Crearea containerului
    QWidget *container = QWidget::createWindowContainer(view, this);
    container->setMinimumSize(QSize(400, 300));

    // Layout pentru MyOpenGLWidget
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(container);
    setLayout(layout);

    // Crearea camerei
    Qt3DRender::QCamera *camera = view->camera();
    camera->lens()->setPerspectiveProjection(45.0f, 4.0f / 3.0f, 0.1f, 1000.0f);
    camera->setPosition(QVector3D(0, 0, 20.0f));
    camera->setViewCenter(QVector3D(0, 0, 0));

    // Controlul camerei
    Qt3DExtras::QOrbitCameraController *camController = new Qt3DExtras::QOrbitCameraController(rootEntity);
    camController->setLinearSpeed(50.0f);
    camController->setLookSpeed(180.0f);
    camController->setCamera(camera);
}

MyOpenGLWidget::~MyOpenGLWidget()
{
    delete view;
}

void MyOpenGLWidget::clearScene()
{
    if (rootEntity) {
        // Șterge toate entitățile copil ale rootEntity, dar păstrează camera și controller-ul camerei
        const auto children = rootEntity->findChildren<Qt3DCore::QEntity*>();
        for (Qt3DCore::QEntity* entity : children) {
            // Verifică dacă entitatea curentă este camera sau controller-ul camerei și le exclude de la ștergere
            auto camera = view->camera();
            auto camController = rootEntity->findChild<Qt3DExtras::QOrbitCameraController*>();

            if (entity != camera && entity != camController) {
                delete entity;  // Șterge entitatea
            }
        }
    }
}

void MyOpenGLWidget::loadModel(const QString &filePath)
{
    clearScene();
    // Aici poți adăuga cod pentru a încărca un model 3D folosind un QMesh și a-l atașa la rootEntity.
    Qt3DRender::QMesh *mesh = new Qt3DRender::QMesh();
    mesh->setSource(QUrl::fromLocalFile(filePath));

    // Creează un material simplu pentru a aplica pe model
    Qt3DExtras::QPhongMaterial *material = new Qt3DExtras::QPhongMaterial(rootEntity);

    // Creează o entitate care conține mesh-ul și materialul
    Qt3DCore::QEntity *modelEntity = new Qt3DCore::QEntity(rootEntity);
    modelEntity->addComponent(mesh);
    modelEntity->addComponent(material);
}

void MyOpenGLWidget::loadScene(const QString &filePath)
{
    QFile jsonFile(filePath);
    if (jsonFile.exists())
    {
        if (jsonFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QByteArray jsonData = jsonFile.readAll();
            jsonFile.close();

            // Parcurge fișierul JSON (opțional, doar dacă vrei să faci ceva cu datele)
            QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
            if (!jsonDoc.isNull())
            {
                QJsonObject jsonObject = jsonDoc.object();
                QJsonArray objectsArray = jsonObject["objects"].toArray();

                clearScene();

                for (const QJsonValue &value : objectsArray)
                {
                    QJsonObject obj = value.toObject();
                    QString objectType = obj["object"].toString();
                    QString color = obj["color"].toString();
                    QString texture = obj["texture"].toString();
                    QString size = obj["size"].toString();
                    QJsonObject position = obj["position"].toObject();

                    float x = position["x"].toDouble();
                    float y = position["y"].toDouble();
                    float z = position["z"].toDouble();

                    loadModelInScene(objectType, color, texture, size, x, y, z);
                }

                // Aici poți prelucra datele din JSON dacă dorești
                // QMessageBox::information(this, "Success", "Scene JSON generated successfully.");

            }
            else
            {
                QMessageBox::warning(this, "Error", "Failed to parse JSON.");
            }
        }
    }
    else
    {
        QMessageBox::warning(this, "Error", "JSON file not found.");
    }
}

void MyOpenGLWidget::loadModelInScene(const QString &objectType, const QString &color, const QString &texture, const QString &size, float x, float y, float z)
{
    // Construirea căii fișierului modelului 3D pe baza tipului obiectului
    QString modelPath = QCoreApplication::applicationDirPath() + "/../../../Models/primitives/" + objectType + ".obj";

    // Verific dacă fișierul modelului există
    QFile modelFile(modelPath);
    if (modelFile.exists())
    {
        // Încărcarea modelului folosind Qt3D
        Qt3DRender::QMesh *mesh = new Qt3DRender::QMesh();
        mesh->setSource(QUrl::fromLocalFile(modelPath));

        // Creează o entitate și adaugă componenta mesh
        Qt3DCore::QEntity *entity = new Qt3DCore::QEntity(rootEntity);
        entity->addComponent(mesh);

        // Creează materialul
        Qt3DExtras::QPhongMaterial *material = new Qt3DExtras::QPhongMaterial();
        material->setDiffuse(QColor(color));
        if (texture == "matte") {
            material->setSpecular(QColor(0, 0, 0));  // Fără reflexii, suprafață mată
            material->setShininess(0.0f);
        } else if (texture == "shiny") {
            material->setSpecular(QColor(255, 255, 255));  // Reflexii puternice
            material->setShininess(100.0f);
        } else if (texture == "metallic") {
            material->setSpecular(QColor(192, 192, 192));  // Reflexii metalice
            material->setShininess(80.0f);
        }

        entity->addComponent(material);

        // Setează poziția și dimensiunea obiectului
        Qt3DCore::QTransform *transform = new Qt3DCore::QTransform();
        transform->setTranslation(QVector3D(x, y, z));

        // Aplică dimensiunea dacă este specificată
        float scaleFactor = 1.0f;
        if (size == "small")
        {
            scaleFactor = 1.0f;
        }
        else if (size == "large")
        {
            scaleFactor = 2.0f;
        }
        else if (size == "tiny")
        {
            scaleFactor = 0.5f;
        }
        else if (size == "huge")
        {
            scaleFactor = 4.0f;
        }

        transform->setScale(scaleFactor);

        entity->addComponent(transform);
    }
    else
    {
        QMessageBox::warning(this, "Error", "Model file not found: " + modelPath);
    }
}

