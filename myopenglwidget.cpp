#include "myopenglwidget.h"
#include "PBRMaterial.h"
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
#include <Qt3DExtras/QPlaneMesh>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonValue>
#include <QRandomGenerator>
#include <QDebug>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>

MyOpenGLWidget::MyOpenGLWidget(QWidget *parent)
    : QWidget(parent), m_floorLevel(-2.0f), m_floorSize(20.0f), m_language("en")
{
    // Configurare Qt3DWindow
    view = new Qt3DExtras::Qt3DWindow();
    view->defaultFrameGraph()->setClearColor(QColor(QRgb(0x4d4d4f)));

    // Creare rootEntity
    rootEntity = new Qt3DCore::QEntity();
    view->setRootEntity(rootEntity);

    // Crearea containerului
    QWidget *container = QWidget::createWindowContainer(view, this);
    container->setMinimumSize(QSize(400, 300));

    // Layout pentru MyOpenGLWidget
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(container);
    setLayout(layout);

    // Configurare cameră
    Qt3DRender::QCamera *camera = view->camera();
    camera->lens()->setPerspectiveProjection(45.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
    camera->setPosition(QVector3D(0, 110, 20));
    camera->setViewCenter(QVector3D(-5, 0, -5));
    camera->setUpVector(QVector3D(0, 1, 0));

    // Controlul camerei
    Qt3DExtras::QOrbitCameraController *camController = new Qt3DExtras::QOrbitCameraController(rootEntity);
    camController->setLinearSpeed(50.0f);
    camController->setLookSpeed(180.0f);
    camController->setCamera(camera);

    // Crearea podelei îmbunătățite
    setupFloor();

    // Configurare iluminare
    setupLighting();

    // Configurare timere pentru animații și fizică
    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout, this, &MyOpenGLWidget::updateAnimations);
    m_animationTimer->start(16); // ~60 FPS

    m_physicsTimer = new QTimer(this);
    connect(m_physicsTimer, &QTimer::timeout, this, &MyOpenGLWidget::updatePhysics);
    m_physicsTimer->start(16); // ~60 FPS

    // Initialize animation time
    m_animationTime = 0.0f;

    // Load settings
    m_settings = new QSettings(this);
    loadSettings();
}

MyOpenGLWidget::~MyOpenGLWidget()
{
    saveSettings();
    delete view;
}

void MyOpenGLWidget::setupFloor()
{
    floorEntity = new Qt3DCore::QEntity(rootEntity);

    // Mesh pentru podea (plan mare)
    Qt3DExtras::QPlaneMesh *floorMesh = new Qt3DExtras::QPlaneMesh();
    floorMesh->setWidth(m_floorSize);
    floorMesh->setHeight(m_floorSize);
    floorMesh->setMeshResolution(QSize(10, 10));

    // Material pentru podea
    Qt3DExtras::QPhongMaterial *floorMaterial = new Qt3DExtras::QPhongMaterial();
    floorMaterial->setDiffuse(QColor(100, 100, 100));
    floorMaterial->setSpecular(QColor(30, 30, 30));
    floorMaterial->setShininess(10.0f);

    // Transform pentru podea
    Qt3DCore::QTransform *floorTransform = new Qt3DCore::QTransform();
    floorTransform->setTranslation(QVector3D(0, m_floorLevel, 0));
    floorTransform->setRotationX(0); // Rotește pentru a fi orizontală

    floorEntity->addComponent(floorMesh);
    floorEntity->addComponent(floorMaterial);
    floorEntity->addComponent(floorTransform);
}

void MyOpenGLWidget::setupLighting()
{
    // Lumina principală
    Qt3DCore::QEntity *lightEntity = new Qt3DCore::QEntity(rootEntity);
    Qt3DRender::QPointLight *pointLight = new Qt3DRender::QPointLight();
    pointLight->setColor(Qt::white);
    pointLight->setIntensity(1.5f);
    lightEntity->addComponent(pointLight);

    Qt3DCore::QTransform *lightTransform = new Qt3DCore::QTransform();
    lightTransform->setTranslation(QVector3D(0, 20, 15));
    lightEntity->addComponent(lightTransform);

    // Iluminare ambientală
    Qt3DCore::QEntity *ambientEntity = new Qt3DCore::QEntity(rootEntity);
    Qt3DRender::QPointLight *ambientLight = new Qt3DRender::QPointLight();
    ambientLight->setColor(QColor(180, 180, 180));
    ambientLight->setIntensity(0.2f);
    ambientEntity->addComponent(ambientLight);

    Qt3DCore::QTransform *ambientTransform = new Qt3DCore::QTransform();
    ambientTransform->setTranslation(QVector3D(0, 50, 0));
    ambientEntity->addComponent(ambientTransform);
}

void MyOpenGLWidget::clearScene()
{
    if (rootEntity) {
        // Șterge obiectele din scene
        for (auto it = m_sceneObjects.begin(); it != m_sceneObjects.end(); ++it) {
            if (it.value().entity) {
                delete it.value().entity;
            }
        }
        m_sceneObjects.clear();
        m_orbitalAnimations.clear();
    }
}

void MyOpenGLWidget::loadModel(const QString &filePath)
{
    clearScene();
    QString modelPath = getModelPath(QFileInfo(filePath).baseName());
    if (modelPath.isEmpty()) {
        qDebug() << "Model not found for:" << filePath;
        return;
    }

    Qt3DRender::QMesh *mesh = new Qt3DRender::QMesh();
    mesh->setSource(QUrl::fromLocalFile(modelPath));

    Qt3DExtras::QPhongMaterial *material = new Qt3DExtras::QPhongMaterial(rootEntity);
    material->setDiffuse(QColor(150, 150, 150));

    Qt3DCore::QEntity *modelEntity = new Qt3DCore::QEntity(rootEntity);
    modelEntity->addComponent(mesh);
    modelEntity->addComponent(material);

    Qt3DCore::QTransform *transform = new Qt3DCore::QTransform();
    transform->setTranslation(QVector3D(0, 0, 0));
    modelEntity->addComponent(transform);

    // ADAUGĂ: Trackează obiectul în sistemul de scene objects
    SceneObject sceneObj;
    sceneObj.id = "preview_model"; // ID fix pentru modelul de preview
    sceneObj.type = QFileInfo(filePath).baseName();
    sceneObj.color = "#969696";
    sceneObj.size = "medium";
    sceneObj.position = QVector3D(0, 0, 0);
    sceneObj.originalPosition = QVector3D(0, 0, 0);
    sceneObj.entity = modelEntity;
    sceneObj.transform = transform;
    sceneObj.boundingSphereRadius = 1.0f;

    m_sceneObjects["preview_model"] = sceneObj;

    qDebug() << "Loaded preview model:" << sceneObj.type << "with entity:" << modelEntity;
}

QJsonObject MyOpenGLWidget::parseSceneFile(const QString &filePath)
{
    QFile jsonFile(filePath);
    if (!jsonFile.exists()) {
        qDebug() << "JSON file not found:" << filePath;
        return QJsonObject();
    }

    if (!jsonFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Could not open JSON file:" << filePath;
        return QJsonObject();
    }

    QByteArray jsonData = jsonFile.readAll();
    jsonFile.close();

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qDebug() << "JSON parse error:" << parseError.errorString();
        return QJsonObject();
    }

    QJsonObject jsonObject = jsonDoc.object();

    if (!validateJsonStructure(jsonObject)) {
        qDebug() << "Invalid JSON structure";
        return QJsonObject();
    }

    return jsonObject;
}

bool MyOpenGLWidget::validateJsonStructure(const QJsonObject &jsonObject)
{
    // Verifică structura obligatorie
    if (!jsonObject.contains("objects") || !jsonObject["objects"].isArray()) {
        qDebug() << "Missing or invalid 'objects' array";
        return false;
    }

    if (!jsonObject.contains("relations") || !jsonObject["relations"].isArray()) {
        qDebug() << "Missing or invalid 'relations' array";
        return false;
}

    QJsonArray objects = jsonObject["objects"].toArray();
    for (const QJsonValue &value : objects) {
        QJsonObject obj = value.toObject();
        if (!obj.contains("object") || !obj.contains("id")) {
            qDebug() << "Object missing required fields (object, id)";
            return false;
        }
    }

    return true;
}

void MyOpenGLWidget::loadScene(const QString &filePath)
{
    QJsonObject jsonObject = parseSceneFile(filePath);
    if (jsonObject.isEmpty()) {
        return;
    }

    clearScene();

    QJsonArray objectsArray = jsonObject["objects"].toArray();
    QJsonArray relationsArray = jsonObject["relations"].toArray();
    QJsonArray animationCouplesArray = jsonObject.value("animation_couples").toArray();

    // Generare culori pentru obiecte
    QMap<QString, QString> objectColors;
    for (const QJsonValue &value : objectsArray) {
        QJsonObject obj = value.toObject();
        QString objectType = obj["object"].toString();
        QString color = obj["attributes"].toObject()["color"].toString();

        if (color.isEmpty()) {
            // Generare culoare aleatorie
            QColor randomColor(QRandomGenerator::global()->bounded(50, 255),
                             QRandomGenerator::global()->bounded(50, 255),
                             QRandomGenerator::global()->bounded(50, 255));
            color = randomColor.name();
        }
        objectColors[objectType] = color;
    }

    // Generare poziții
    QMap<QString, QVector3D> objectPositions = generateObjectPositions(objectsArray, relationsArray);

    // Rezolvare coliziuni
    resolveCollisions(objectPositions);

    // Spawn obiecte în scenă
    spawnObjectsInScene(objectPositions, objectColors, objectsArray);

    // Configurare animații
    setupAnimations(objectsArray, animationCouplesArray);
}

QMap<QString, QVector3D> MyOpenGLWidget::generateObjectPositions(const QJsonArray &objects, const QJsonArray &relations)
{
    QMap<QString, QVector3D> objectPositions;
    float initialX = -5.0f;
    float initialZ = -5.0f;

    // Poziționare inițială
    for (const QJsonValue &value : objects) {
        QJsonObject obj = value.toObject();
        QString objectId = obj["id"].toString();
        QString objectType = obj["object"].toString();

        QVector3D position(initialX,  m_floorLevel + 2.0f, initialZ);

        // Verifică dacă modelul există
        QString modelPath = getModelPath(objectType);
        if (!modelPath.isEmpty()) {
            // Calculează înălțimea reală a obiectului
            QString size = obj["attributes"].toObject()["size"].toString();

            // FIX: Use proper object height calculation
            QVector3D minBounds, maxBounds;
            QVector3D dimensions = calculateBoundingBox(objectType, size, minBounds, maxBounds);
            float objectHeight = dimensions.y(); // Actual height, not radius

            // ALWAYS apply floor constraint
            position = getFloorConstrainedPosition(position, objectHeight);

            // SAFETY CHECK: Ensure Y is never below floor
            if (position.y() < m_floorLevel + 0.1f) {
                position.setY(m_floorLevel + objectHeight/2.0f + 0.5f);
                qDebug() << "SAFETY: Forced object" << objectId << "above floor to Y=" << position.y();
            }

            objectPositions[objectId] = position;
            qDebug() << "Placed object" << objectId << "at position:" << position << "with height:" << objectHeight;
        } else {
            qDebug() << "Model not found for object type:" << objectType << "- skipping";
        }

        initialX += DEFAULT_SPACING;
        if (initialX > 15.0f) {
            initialX = -5.0f;
            initialZ += DEFAULT_SPACING;
        }
    }

    // Aplicare relații
    for (const QJsonValue &value : relations) {
        QJsonObject relationObj = value.toObject();
        QString obj1Id = relationObj["object_1"].toString();
        QString obj2Id = relationObj["object_2"].toString();
        QString relation = relationObj["relation"].toString();

        if (!objectPositions.contains(obj1Id) || !objectPositions.contains(obj2Id)) {
            continue; // Skip relația dacă obiectele nu există
        }

        QVector3D offset(0, 0, 0);

        if (relation == "left") offset.setX(-DEFAULT_SPACING);
        else if (relation == "right") offset.setX(DEFAULT_SPACING);
        else if (relation == "behind") offset.setZ(-DEFAULT_SPACING);
        else if (relation == "front") offset.setZ(DEFAULT_SPACING);
        else if (relation == "above" || relation == "on top of" || relation == "on") {
            offset.setY(DEFAULT_SPACING);
        }
        else if (relation == "below" || relation == "under") offset.setY(-DEFAULT_SPACING * 0.5f);
        else if (relation == "near") {
            offset.setX(DEFAULT_SPACING * 0.5f);
        }

        QVector3D newPosition = objectPositions[obj2Id] + offset;

        // Aplică constrângerea podelei pentru noua poziție
        QVector3D minBounds, maxBounds;
        QVector3D dimensions = calculateBoundingBox("cube", "medium", minBounds, maxBounds); // default
        float objectHeight = dimensions.y();
        newPosition = getFloorConstrainedPosition(newPosition, objectHeight);

        // DOUBLE CHECK: Never below floor
        if (newPosition.y() < m_floorLevel + 0.1f) {
            newPosition.setY(m_floorLevel + objectHeight/2.0f + 0.5f);
            qDebug() << "RELATION SAFETY: Forced object" << obj1Id << "above floor to Y=" << newPosition.y();
        }

        objectPositions[obj1Id] = newPosition;
    }

    return objectPositions;
}

void MyOpenGLWidget::resolveCollisions(QMap<QString, QVector3D> &positions)
{
    QVector<QVector3D> usedPositions;

    for (auto it = positions.begin(); it != positions.end(); ++it) {
        QVector3D pos = it.value();
        bool hasCollision = true;
        int attempts = 0;
        const int maxAttempts = 10;

        while (hasCollision && attempts < maxAttempts) {
            hasCollision = false;

            for (const QVector3D &existingPos : usedPositions) {
                float distance = pos.distanceToPoint(existingPos);
                if (distance < (DEFAULT_SPACING - COLLISION_TOLERANCE)) {
                    // Coliziune detectată, mută obiectul
                    pos.setX(pos.x() + DEFAULT_SPACING);
                    hasCollision = true;
                    break;
                }
            }
            attempts++;
        }

        // Aplică constrângerea podelei
        pos = getFloorConstrainedPosition(pos, 1.0f);

        usedPositions.append(pos);
        it.value() = pos;
    }
}

bool MyOpenGLWidget::checkAABBCollision(const SceneObject &obj1, const SceneObject &obj2)
{
    return (obj1.boundingBoxMin.x() <= obj2.boundingBoxMax.x() &&
            obj1.boundingBoxMax.x() >= obj2.boundingBoxMin.x() &&
            obj1.boundingBoxMin.y() <= obj2.boundingBoxMax.y() &&
            obj1.boundingBoxMax.y() >= obj2.boundingBoxMin.y() &&
            obj1.boundingBoxMin.z() <= obj2.boundingBoxMax.z() &&
            obj1.boundingBoxMax.z() >= obj2.boundingBoxMin.z());
    }

bool MyOpenGLWidget::checkSphereCollision(const SceneObject &obj1, const SceneObject &obj2)
{
    float distance = obj1.position.distanceToPoint(obj2.position);
    return distance < (obj1.boundingSphereRadius + obj2.boundingSphereRadius);
    }

void MyOpenGLWidget::applyImpulse(SceneObject &staticObj, const SceneObject &dynamicObj)
{
    QVector3D direction = staticObj.position - dynamicObj.position;
    direction.normalize();

    staticObj.velocity += direction * IMPULSE_STRENGTH;
    staticObj.isDynamic = true; // Devine dinamic temporar
}

void MyOpenGLWidget::spawnObjectsInScene(const QMap<QString, QVector3D> &positions,
                                       const QMap<QString, QString> &colors,
                                       const QJsonArray &objects)
{
    // QMap<QString, QJsonObject> objectsById;

    // // Creează un map pentru acces rapid la obiecte prin ID
    // for (const QJsonValue &value : objects) {
    //     QJsonObject obj = value.toObject();
    //     QString id = obj["id"].toString();
    //     objectsById[id] = obj;
    // }

    // for (auto it = positions.begin(); it != positions.end(); ++it) {
    //     QString objectId = it.key();
    //     QVector3D position = it.value();

    //     if (!objectsById.contains(objectId)) {
    //         continue;
    //     }

    //     QJsonObject obj = objectsById[objectId];
    //     QString objectType = obj["object"].toString();
    //     QString color = colors.value(objectType, "#888888");
    //     QString size = obj["attributes"].toObject()["size"].toString();

    //     QJsonArray animationsArray = obj["attributes"].toObject()["animations"].toArray();
    //     QStringList animations;
    //     for (const QJsonValue &animValue : animationsArray) {
    //         animations << animValue.toString();
    //     }

    //     loadModelInScene(objectType, color, size, position.x(), position.y(), position.z(), animations, objectId);
    // }

    QMap<QString, QJsonObject> objectsById;

    // Indexare rapidă a obiectelor după ID
    for (const QJsonValue &value : objects) {
        QJsonObject obj = value.toObject();
        QString id = obj["id"].toString();
        objectsById[id] = obj;
    }

    for (auto it = positions.begin(); it != positions.end(); ++it) {
        QString objectId = it.key();
        QVector3D position = it.value();

        if (!objectsById.contains(objectId))
            continue;

        QJsonObject obj = objectsById[objectId];
        QString objectType = obj["object"].toString();

        // Verifică dacă modelul e cunoscut (există în primitives)
        if (getModelPath(objectType).isEmpty()) {
            qDebug() << "Skipping unknown model type:" << objectType << "for object" << objectId;
            continue;
        }

        QString color;
        if (obj["attributes"].toObject().contains("color") &&
            obj["attributes"].toObject()["color"].isString()) {
            color = obj["attributes"].toObject()["color"].toString();
        } else {
            color = "#888888"; // fallback
        }

        QString size = obj["attributes"].toObject()["size"].toString();

        QJsonArray animationsArray = obj["attributes"].toObject()["animations"].toArray();
        QStringList animations;
        for (const QJsonValue &animValue : animationsArray) {
            animations << animValue.toString();
        }

        loadModelInScene(objectType, color, size,
                         position.x(), position.y(), position.z(),
                         animations, objectId);
    }
}

QString MyOpenGLWidget::getModelPath(const QString &objectType)
{
    QString basePath = QCoreApplication::applicationDirPath() + "/../../../Models/primitives/";

    // Support for multiple formats in priority order
    QStringList extensions = {".fbx", ".obj", ".gltf", ".glb", ".3ds", ".dae", ".ply", ".stl"};

    // MODIFICARE: Adaugă căutare case-insensitive
    for (const QString &ext : extensions) {
        QString modelPath = basePath + objectType + ext;
        if (QFile::exists(modelPath)) {
            qDebug() << "Found model (exact match):" << modelPath;
            return modelPath;
        }

        // NOUĂ: Căutare case-insensitive în directorul primitives
        QDir primitivesDir(basePath);
        QStringList allFiles = primitivesDir.entryList(QDir::Files);

        QString targetFileName = objectType + ext;
        for (const QString &fileName : allFiles) {
            if (fileName.compare(targetFileName, Qt::CaseInsensitive) == 0) {
                QString foundPath = basePath + fileName;
                qDebug() << "Found model (case-insensitive):" << foundPath << "for requested:" << objectType;
                return foundPath;
            }
        }
    }

    // Also check in subdirectories (common for complex models) - MODIFICAT pentru case-insensitive
    QDir primitivesDir(basePath);
    QStringList subdirs = primitivesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &subdir : subdirs) {
        // MODIFICAT: Comparație case-insensitive pentru subdirectoare
        if (subdir.compare(objectType, Qt::CaseInsensitive) == 0 ||
            subdir.toLower().contains(objectType.toLower())) {

            QString subdirPath = basePath + subdir + "/";
            QDir subdirObj(subdirPath);
            QStringList subdirFiles = subdirObj.entryList(QDir::Files);

            for (const QString &ext : extensions) {
                QString targetFileName = objectType + ext;

                // NOUĂ: Căutare case-insensitive în subdirectoare
                for (const QString &fileName : subdirFiles) {
                    if (fileName.compare(targetFileName, Qt::CaseInsensitive) == 0) {
                        QString foundPath = subdirPath + fileName;
                        qDebug() << "Found model in subdirectory (case-insensitive):" << foundPath;
                        return foundPath;
                    }

                    // Also try with subdir name (case-insensitive)
                    QString subdirFileName = subdir + ext;
                    if (fileName.compare(subdirFileName, Qt::CaseInsensitive) == 0) {
                        QString foundPath = subdirPath + fileName;
                        qDebug() << "Found model with subdir name (case-insensitive):" << foundPath;
                        return foundPath;
                    }
                }
            }
        }
    }

    qDebug() << "No model found for object type:" << objectType;
    return QString(); // Not found
}
    // // Support for multiple formats in priority order
    // QStringList extensions = {".fbx", ".obj", ".gltf", ".glb", ".3ds", ".dae", ".ply", ".stl"};

    // for (const QString &ext : extensions) {
    //     QString modelPath = basePath + objectType + ext;
    //     if (QFile::exists(modelPath)) {
    //         qDebug() << "Found model:" << modelPath;
    //         return modelPath;
    //     }
    // }

    // // Also check in subdirectories (common for complex models)
    // QDir primitivesDir(basePath);
    // QStringList subdirs = primitivesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    // for (const QString &subdir : subdirs) {
    //     if (subdir.toLower().contains(objectType.toLower())) {
    //         QString subdirPath = basePath + subdir + "/";
    //         for (const QString &ext : extensions) {
    //             QString modelPath = subdirPath + objectType + ext;
    //             if (QFile::exists(modelPath)) {
    //                 qDebug() << "Found model in subdirectory:" << modelPath;
    //                 return modelPath;
    //             }
    //             // Also try with subdir name
    //             modelPath = subdirPath + subdir + ext;
    //             if (QFile::exists(modelPath)) {
    //                 qDebug() << "Found model with subdir name:" << modelPath;
    //                 return modelPath;
    //             }
    //         }
    //     }
    // }

    // qDebug() << "No model found for object type:" << objectType;
    // return QString(); // Not found
// }

QColor MyOpenGLWidget::parseColor(const QString &colorString)
{
    if (colorString.startsWith("#")) {
        return QColor(colorString);
    }

    // Mapare culori cunoscute
    static QMap<QString, QColor> colorMap = {
        {"red", QColor(255, 0, 0)},
        {"green", QColor(0, 255, 0)},
        {"blue", QColor(0, 0, 255)},
        {"yellow", QColor(255, 255, 0)},
        {"orange", QColor(255, 165, 0)},
        {"purple", QColor(128, 0, 128)},
        {"pink", QColor(255, 192, 203)},
        {"white", QColor(255, 255, 255)},
        {"black", QColor(0, 0, 0)},
        {"gray", QColor(128, 128, 128)},
        {"grey", QColor(128, 128, 128)},
        {"brown", QColor(165, 42, 42)},
        {"golden", QColor(255, 215, 0)},
        {"silver", QColor(192, 192, 192)},
        {"metal", QColor(169, 169, 169)},
        {"glass", QColor(173, 216, 230, 100)}
    };

    QString lowerColor = colorString.toLower();
    if (colorMap.contains(lowerColor)) {
        return colorMap[lowerColor];
    }

    // Culoare implicită
    return QColor(128, 128, 128);
}

float MyOpenGLWidget::getSizeMultiplier(const QString &size)
{
    if (size == "small") return 0.7f;
    if (size == "large" || size == "big") return 1.5f;
    if (size == "huge") return 2.0f;
    if (size == "tiny") return 0.4f;
    return 1.0f; // medium/default
}

QVector3D MyOpenGLWidget::getFloorConstrainedPosition(const QVector3D &position, float objectHeight)
{
    // qDebug() << "getFloorConstrainedPosition() CALLED!";
    // qDebug() << "Input position:" << position;
    // qDebug() << "Object height:" << objectHeight;
    // qDebug() << "Floor level:" << m_floorLevel;
    QVector3D constrainedPos = position;
    float minY = m_floorLevel + (objectHeight / 2.0f) + 0.2f; // Puțin deasupra podelei

    // qDebug() << "Calculated minY:" << minY;
    // qDebug() << "Current Y:" << constrainedPos.y();

    if (constrainedPos.y() < minY) {
        // qDebug() << "APPLYING CONSTRAINT: Moving from Y=" << constrainedPos.y() << "to Y=" << minY;

        constrainedPos.setY(minY);
        // qDebug() << "FLOOR CONSTRAINT: Moved object from Y=" << position.y() << "to Y=" << minY;
    }
    // else
    // {
    //     // qDebug() << "NO CONSTRAINT NEEDED - object already above minY";
    // }

    return constrainedPos;
}

QVector3D MyOpenGLWidget::calculateBoundingBox(const QString &objectType, const QString &size,
                                             QVector3D &minBounds, QVector3D &maxBounds)
{
    float sizeMultiplier = getSizeMultiplier(size);

    // Dimensiuni aproximative pentru diferite tipuri de obiecte
    QVector3D dimensions(1.0f, 1.0f, 1.0f);

    if (objectType == "cube" || objectType == "box") {
        dimensions = QVector3D(1.0f, 1.0f, 1.0f);
    } else if (objectType == "sphere" || objectType == "ball") {
        dimensions = QVector3D(1.0f, 1.0f, 1.0f);
    } else if (objectType == "chair") {
        dimensions = QVector3D(1.0f, 2.0f, 1.0f);
    } else if (objectType == "table") {
        dimensions = QVector3D(2.0f, 1.5f, 1.0f);
    } else if (objectType == "teapot") {
        dimensions = QVector3D(1.2f, 1.0f, 1.2f);
    }

    dimensions *= sizeMultiplier;

    minBounds = QVector3D(-dimensions.x()/2, -dimensions.y()/2, -dimensions.z()/2);
    maxBounds = QVector3D(dimensions.x()/2, dimensions.y()/2, dimensions.z()/2);

    return dimensions;
}

float MyOpenGLWidget::calculateBoundingSphere(const QString &objectType, const QString &size)
{
    QVector3D minBounds, maxBounds;
    QVector3D dimensions = calculateBoundingBox(objectType, size, minBounds, maxBounds);

    return dimensions.length() / 2.0f;
}

bool MyOpenGLWidget::hasPBRTextures(const QString &objectType)
{
    QString basePath = QCoreApplication::applicationDirPath() + "/../../../Models/textures/";

    // Common PBR texture naming conventions with multiple format support
    QStringList pbrPrefixes = {
        "_diff", "_albedo", "_diffuse", "_basecolor",    // Albedo/Diffuse
        "_nor", "_nor_gl", "_normal", "_norm",           // Normal maps
        "_metallic", "_metal",                           // Metallic
        "_roughness", "_rough"                           // Roughness
    };

    // Supported texture formats
    QStringList supportedFormats = {"png", "jpg", "jpeg", "tga", "bmp", "exr", "hdr"};

    // Check if any PBR texture exists in any supported format
    for (const QString &prefix : pbrPrefixes) {
        for (const QString &format : supportedFormats) {
            QString texturePath = basePath + objectType + prefix + "." + format;
            if (QFile::exists(texturePath)) {
                qDebug() << "Found PBR texture for" << objectType << ":" << texturePath;
                return true;
            }
        }
    }

    return false;
}

void MyOpenGLWidget::loadModelInScene(const QString &objectType, const QString &color,
                                    const QString &size, float x, float y, float z,
                                    const QStringList &animations, const QString &id)
{
    qDebug() << "\n=== LOADING OBJECT ===" << id << "===";

    QString modelPath = getModelPath(objectType);
    if (modelPath.isEmpty()) {
        qDebug() << "Model not found for object type:" << objectType;
        return;
    }

    // Create mesh
    Qt3DRender::QMesh *mesh = new Qt3DRender::QMesh();
    mesh->setSource(QUrl::fromLocalFile(modelPath));

    // Create entity
    Qt3DCore::QEntity *entity = new Qt3DCore::QEntity(rootEntity);
    entity->addComponent(mesh);

    // Parse color
    QColor objColor = parseColor(color);

    // Try PBR material first
    Qt3DRender::QMaterial *material = nullptr;

    // try {
    //     if (hasPBRTextures(objectType)) {
    //         qDebug() << "🎨 Using PBR material with textures for:" << objectType;
    //         material = new PBRMaterial(entity, objectType, objColor);
    //     } else {
    //         qDebug() << "🎨 Using PBR material with color only for:" << objectType;
    //         material = new PBRMaterial(entity, QString(), objColor);
    //     }
    // } catch (const std::exception &e) {
    //     qDebug() << "❌ PBR material failed:" << e.what();
    //     material = nullptr;
    // }

    // Fallback to Qt3DExtras materials if PBR fails
    if (!material) {
        qDebug() << "Using Phong fallback material for:" << objectType;
        Qt3DExtras::QPhongMaterial *phongMaterial = new Qt3DExtras::QPhongMaterial();
        phongMaterial->setDiffuse(objColor);
        phongMaterial->setSpecular(objColor.lighter(110));
        phongMaterial->setShininess(50.0f);
        phongMaterial->setAmbient(objColor.darker(150));
        material = phongMaterial;
    }

    entity->addComponent(material);

    // Transform with proper positioning
    Qt3DCore::QTransform *transform = new Qt3DCore::QTransform();
    float sizeMultiplier = getSizeMultiplier(size);

    // Good visible scale
    float finalScale = sizeMultiplier * 2.0f;
    transform->setScale3D(QVector3D(finalScale, finalScale, finalScale));

    // Aplicare constrângeri podea
    QVector3D minBounds, maxBounds;
    QVector3D dimensions = calculateBoundingBox(objectType, size, minBounds, maxBounds);
    float actualHeight = dimensions.y() * sizeMultiplier * 2.0f; // Account for scale

    // Start with input position
    QVector3D position(x, y, z);

    // ALWAYS apply floor constraint with actual height
    position = getFloorConstrainedPosition(position, actualHeight);

    // TRIPLE CHECK: Never ever below floor
    float absoluteMinY = m_floorLevel + 0.5f; // Minimum buffer above floor
    if (position.y() < absoluteMinY) {
        position.setY(absoluteMinY + actualHeight/2.0f);
        qDebug() << "ABSOLUTE SAFETY: Forced object" << id << "to Y=" << position.y();
    }

    transform->setTranslation(position);

    entity->addComponent(transform);
    qDebug() << "FINAL POSITION for" << id << ":" << position << "Floor level:" << m_floorLevel << "Object height:" << actualHeight;

    // Creare obiect scenă
    SceneObject sceneObj;
    sceneObj.id = id;
    sceneObj.type = objectType;
    sceneObj.color = color;
    sceneObj.size = size;
    sceneObj.position = position;
    sceneObj.originalPosition = position; // Store original position for animations
    sceneObj.entity = entity;
    sceneObj.transform = transform;
    sceneObj.animations = animations;
    sceneObj.boundingSphereRadius = calculateBoundingSphere(objectType, size);

    // Initialize animation state
    sceneObj.animationState.bouncePhase = 0.0f;
    sceneObj.animationState.floatPhase = 0.0f;
    sceneObj.animationState.pulsePhase = 0.0f;
    sceneObj.animationState.swingPhase = 0.0f;
    sceneObj.animationState.rotationAngle = 0.0f;

    // Calculate bounding box
    // QVector3D minBounds, maxBounds;
    // calculateBoundingBox(objectType, size, minBounds, maxBounds);
    sceneObj.boundingBoxMin = position + (minBounds * finalScale);
    sceneObj.boundingBoxMax = position + (maxBounds * finalScale);

    m_sceneObjects[id] = sceneObj;

    qDebug() << "Loaded object:" << id << "of type:" << objectType << "at position:" << position;
}

// Implementarea funcțiilor de animație
void MyOpenGLWidget::setupAnimations(const QJsonArray &objects, const QJsonArray &animationCouples)
{
    // Setup animații individuale pentru obiecte
    for (const QJsonValue &value : objects) {
        QJsonObject obj = value.toObject();
        QString objectId = obj["id"].toString();
        QJsonArray animationsArray = obj["attributes"].toObject()["animations"].toArray();

        if (!m_sceneObjects.contains(objectId)) {
            continue;
        }

        SceneObject &sceneObj = m_sceneObjects[objectId];

        for (const QJsonValue &animValue : animationsArray) {
            QString animationType = animValue.toString();
            setupObjectAnimation(sceneObj, animationType);
        }
    }

    // Setup animații cuplu (orbitale)
    for (const QJsonValue &value : animationCouples) {
        QJsonObject couple = value.toObject();
        QString primaryId = couple["primary_object"].toString();
        QString referenceId = couple["reference_object"].toString();
        QString animationType = couple["animation_type"].toString();
        QString description = couple["description"].toString();

        setupOrbitalAnimation(primaryId, referenceId, animationType, description);
    }
}

void MyOpenGLWidget::setupObjectAnimation(SceneObject &obj, const QString &animationType)
{
    if (!obj.entity || !obj.transform) {
        return;
    }

    if (animationType == "rotate" || animationType == "rotation") {
        // Animația va fi gestionată în updateAnimations()
        qDebug() << "Setup rotation animation for object:" << obj.id;
    }
    else if (animationType == "bounce" || animationType == "bouncing") {
        // Animația va fi gestionată în updateAnimations()
        qDebug() << "Setup bounce animation for object:" << obj.id;
    }
    else if (animationType == "float" || animationType == "floating") {
        // Animația va fi gestionată în updateAnimations()
        qDebug() << "Setup float animation for object:" << obj.id;
    }
    else if (animationType == "pulse" || animationType == "pulsing") {
        // Animația va fi gestionată în updateAnimations()
        qDebug() << "Setup pulse animation for object:" << obj.id;
    }
    else if (animationType == "swing" || animationType == "swinging") {
        // Animația va fi gestionată în updateAnimations()
        qDebug() << "Setup swing animation for object:" << obj.id;
    }
}

void MyOpenGLWidget::setupOrbitalAnimation(const QString &primaryId, const QString &referenceId,
                                         const QString &animationType, const QString &description)
{
    if (!m_sceneObjects.contains(primaryId) || !m_sceneObjects.contains(referenceId)) {
        qDebug() << "Cannot setup orbital animation - objects not found:" << primaryId << referenceId;
        return;
    }

    OrbitalAnimation orbital;
    orbital.primaryObjectId = primaryId;
    orbital.referenceObjectId = referenceId;
    orbital.animationType = animationType;
    orbital.description = description;

    // Configurare parametri bazați pe tip
    if (animationType == "orbit" || animationType == "orbiting") {
        orbital.radius = 5.0f;
        orbital.speed = 0.5f;
    }
    else if (animationType == "circle" || animationType == "circling") {
        orbital.radius = 3.0f;
        orbital.speed = 1.0f;
    }
    else if (animationType == "revolve" || animationType == "revolving") {
        orbital.radius = 7.0f;
        orbital.speed = 0.3f;
    }

    m_orbitalAnimations.append(orbital);
    qDebug() << "Setup orbital animation:" << primaryId << animationType << "around" << referenceId;
}

// void MyOpenGLWidget::updateAnimations()
// {
//     static float animationTime = 0.0f;
//     animationTime += 0.016f; // ~60 FPS

//     // Update animații individuale
//     for (auto it = m_sceneObjects.begin(); it != m_sceneObjects.end(); ++it) {
//         SceneObject &obj = it.value();

//         if (!obj.entity || !obj.transform) {
//             continue;
//         }

//         for (const QString &animationType : obj.animations) {
//             if (animationType == "rotate" || animationType == "rotation") {
//                 // Rotație continuă pe axa Y
//                 QQuaternion currentRotation = obj.transform->rotation();
//                 QQuaternion yRotation = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0),
//                                                                      animationTime * 30.0f);
//                 obj.transform->setRotation(yRotation);
//             }
//             else if (animationType == "bounce" || animationType == "bouncing") {
//                 // Sărire verticală
//                 float bounceOffset = abs(sin(animationTime * 2.0f)) * 2.0f;
//                 QVector3D newPos = obj.position;
//                 newPos.setY(obj.position.y() + bounceOffset);
//                 obj.transform->setTranslation(newPos);
//             }
//             else if (animationType == "float" || animationType == "floating") {
//                 // Plutire lentă
//                 float floatOffset = sin(animationTime * 0.5f) * 1.0f;
//                 QVector3D newPos = obj.position;
//                 newPos.setY(obj.position.y() + floatOffset);
//                 obj.transform->setTranslation(newPos);
//             }
//             else if (animationType == "pulse" || animationType == "pulsing") {
//                 // Scalare pulsantă
//                 float pulseScale = 1.0f + sin(animationTime * 3.0f) * 0.2f;
//                 float baseSizeMultiplier = getSizeMultiplier(obj.size);
//                 obj.transform->setScale(baseSizeMultiplier * pulseScale);
//             }
//             else if (animationType == "swing" || animationType == "swinging") {
//                 // Pendulare pe axa Z
//                 float swingAngle = sin(animationTime * 1.5f) * 15.0f;
//                 QQuaternion swingRotation = QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), swingAngle);
//                 obj.transform->setRotation(swingRotation);
//             }
//         }
//     }

//     // Update animații orbitale
//     for (OrbitalAnimation &orbital : m_orbitalAnimations) {
//         if (!m_sceneObjects.contains(orbital.primaryObjectId) ||
//             !m_sceneObjects.contains(orbital.referenceObjectId)) {
//             continue;
//         }

//         SceneObject &primaryObj = m_sceneObjects[orbital.primaryObjectId];
//         const SceneObject &referenceObj = m_sceneObjects[orbital.referenceObjectId];

//         if (!primaryObj.transform) {
//             continue;
//         }

//         // Calculare poziție orbitală
//         orbital.currentAngle += orbital.speed * 0.016f;
//         if (orbital.currentAngle > 2.0f * M_PI) {
//             orbital.currentAngle -= 2.0f * M_PI;
//         }

//         float x = referenceObj.position.x() + cos(orbital.currentAngle) * orbital.radius;
//         float z = referenceObj.position.z() + sin(orbital.currentAngle) * orbital.radius;
//         float y = referenceObj.position.y(); // Menține aceeași înălțime

//         QVector3D newPosition(x, y, z);
//         newPosition = getFloorConstrainedPosition(newPosition, primaryObj.boundingSphereRadius);

//         primaryObj.transform->setTranslation(newPosition);
//         primaryObj.position = newPosition;

//         // Update bounding box
//         QVector3D minBounds, maxBounds;
//         calculateBoundingBox(primaryObj.type, primaryObj.size, minBounds, maxBounds);
//         primaryObj.boundingBoxMin = newPosition + minBounds;
//         primaryObj.boundingBoxMax = newPosition + maxBounds;
//     }
// }

void MyOpenGLWidget::updateAnimations()
{
    const float deltaTime = 0.016f; // ~60 FPS
    m_animationTime += deltaTime;

    // Update individual object animations
    for (auto it = m_sceneObjects.begin(); it != m_sceneObjects.end(); ++it) {
        SceneObject &obj = it.value();

        if (!obj.entity || !obj.transform) {
            continue;
        }

        // Start with the original position and identity transforms
        QVector3D currentPosition = obj.originalPosition;
        QQuaternion currentRotation = QQuaternion();
        float currentScale = getSizeMultiplier(obj.size);

        // Apply all individual animations additively
        for (const QString &animationType : obj.animations) {
            if (animationType == "rotate" || animationType == "rotation" || animationType == "spin") {
                // Continuous rotation on Y axis
                obj.animationState.rotationAngle += 30.0f * deltaTime; // 30 degrees per second
                if (obj.animationState.rotationAngle > 360.0f) {
                    obj.animationState.rotationAngle -= 360.0f;
                }
                QQuaternion yRotation = QQuaternion::fromAxisAndAngle(QVector3D(0, 1, 0), obj.animationState.rotationAngle);
                currentRotation = currentRotation * yRotation;
            }
            else if (animationType == "bounce" || animationType == "bouncing" || animationType == "jump") {
                // Vertical bouncing
                obj.animationState.bouncePhase += 2.0f * deltaTime;
                float bounceOffset = qAbs(qSin(obj.animationState.bouncePhase)) * 2.0f;
                currentPosition.setY(currentPosition.y() + bounceOffset);
            }
            else if (animationType == "float" || animationType == "floating") {
                // Gentle floating motion
                obj.animationState.floatPhase += 0.5f * deltaTime;
                float floatOffset = qSin(obj.animationState.floatPhase) * 1.0f;
                currentPosition.setY(currentPosition.y() + floatOffset);
            }
            else if (animationType == "pulse" || animationType == "pulsing") {
                // Scale pulsing
                obj.animationState.pulsePhase += 3.0f * deltaTime;
                float pulseScale = 1.0f + qSin(obj.animationState.pulsePhase) * 0.2f;
                currentScale *= pulseScale;
            }
            else if (animationType == "swing" || animationType == "swinging" || animationType == "oscillate") {
                // Pendulum motion on Z axis
                obj.animationState.swingPhase += 1.5f * deltaTime;
                float swingAngle = qSin(obj.animationState.swingPhase) * 15.0f;
                QQuaternion swingRotation = QQuaternion::fromAxisAndAngle(QVector3D(0, 0, 1), swingAngle);
                currentRotation = currentRotation * swingRotation;
            }
            else if (animationType == "glow") {
                // For glow effect, you might want to modify material properties
                // This is a placeholder - actual glow would require shader modifications
                obj.animationState.pulsePhase += 2.0f * deltaTime;
                // Could modify material emission or intensity here
            }

            // AT THE END, ALWAYS check floor constraint:
            QVector3D finalPosition = obj.transform->translation();

            // NEVER allow objects below floor during animation
            QVector3D minBounds, maxBounds;
            QVector3D dimensions = calculateBoundingBox(obj.type, obj.size, minBounds, maxBounds);
            float objectHeight = dimensions.y() * obj.transform->scale3D().y();

            float minAllowedY = m_floorLevel + objectHeight/2.0f + 0.1f;
            if (finalPosition.y() < minAllowedY) {
                finalPosition.setY(minAllowedY);
                obj.transform->setTranslation(finalPosition);
                // qDebug() << "ANIMATION FLOOR CHECK: Corrected" << obj.id << "to Y=" << finalPosition.y();
            }

            obj.position = finalPosition;
        }

        // Apply floor constraint to the final position
        currentPosition = getFloorConstrainedPosition(currentPosition, obj.boundingSphereRadius);

        // Update transform with all combined animations
        obj.transform->setTranslation(currentPosition);
        obj.transform->setRotation(currentRotation);
        obj.transform->setScale(currentScale);

        // Update logical position for physics/collision detection
        obj.position = currentPosition;

        // Update bounding box
        QVector3D minBounds, maxBounds;
        calculateBoundingBox(obj.type, obj.size, minBounds, maxBounds);
        obj.boundingBoxMin = currentPosition + minBounds;
        obj.boundingBoxMax = currentPosition + maxBounds;
    }

    // Update orbital animations (these override position but preserve other animations)
    for (OrbitalAnimation &orbital : m_orbitalAnimations) {
        if (!m_sceneObjects.contains(orbital.primaryObjectId) ||
            !m_sceneObjects.contains(orbital.referenceObjectId))
            continue;

        SceneObject &primaryObj = m_sceneObjects[orbital.primaryObjectId];
        const SceneObject &referenceObj = m_sceneObjects[orbital.referenceObjectId];

        if (!primaryObj.transform) continue;

        orbital.currentAngle += orbital.speed * deltaTime;
        if (orbital.currentAngle > 2.0f * M_PI) {
            orbital.currentAngle -= 2.0f * M_PI;
        }

        float x = referenceObj.position.x() + qCos(orbital.currentAngle) * orbital.radius;
        float z = referenceObj.position.z() + qSin(orbital.currentAngle) * orbital.radius;
        float y = referenceObj.position.y(); // Maintain same height as reference

        QVector3D newOrbitalPosition(x, y, z);

        // Apply collision avoidance for orbital objects
        bool hasCollision = false;
        for (auto it = m_sceneObjects.begin(); it != m_sceneObjects.end(); ++it) {
            if (it.key() == orbital.primaryObjectId || it.key() == orbital.referenceObjectId)
                continue;

            SceneObject &other = it.value();
            SceneObject temp = primaryObj;
            temp.position = newOrbitalPosition;

            if (checkSphereCollision(temp, other)) {
                hasCollision = true;
                newOrbitalPosition.setY(newOrbitalPosition.y() + primaryObj.boundingSphereRadius * 2.0f);
                break;
            }
        }

        // Update the original position for orbital objects so other animations work from orbital position
        primaryObj.originalPosition = newOrbitalPosition;

        // Get current transform state (rotation and scale might have been modified by individual animations)
        QQuaternion currentRotation = primaryObj.transform->rotation();
        float currentScale = primaryObj.transform->scale3D().x(); // Assuming uniform scaling

        // Apply orbital position while preserving other animation effects
        primaryObj.transform->setTranslation(newOrbitalPosition);
        primaryObj.position = newOrbitalPosition;

        // Update bounding box for orbital position
        QVector3D minBounds, maxBounds;
        calculateBoundingBox(primaryObj.type, primaryObj.size, minBounds, maxBounds);
        primaryObj.boundingBoxMin = newOrbitalPosition + minBounds;
        primaryObj.boundingBoxMax = newOrbitalPosition + maxBounds;
    }
}

void MyOpenGLWidget::updatePhysics()
{
    const float deltaTime = 0.016f; // ~60 FPS
    const float damping = 0.98f;
    const float minVelocity = 0.01f;

    // Update fizică pentru obiectele dinamice
    for (auto it = m_sceneObjects.begin(); it != m_sceneObjects.end(); ++it) {
        SceneObject &obj = it.value();

        if (!obj.isDynamic || !obj.transform) {
            continue;
        }

        // Aplicare gravitație
        obj.velocity.setY(obj.velocity.y() + GRAVITY * deltaTime);

        // Update poziție
        QVector3D newPosition = obj.position + obj.velocity * deltaTime;

        // Verificare coliziune cu podea
        float minY = m_floorLevel + obj.boundingSphereRadius + 0.1f;
        if (newPosition.y() <= minY) {
            newPosition.setY(minY);
            obj.velocity.setY(-obj.velocity.y() * 0.6f); // Bounce cu pierdere de energie

            // Oprire dacă viteza este prea mică
            if (abs(obj.velocity.y()) < minVelocity) {
                obj.velocity.setY(0);
                obj.isDynamic = false; // Devine static din nou
            }
        }

        // Aplicare damping
        obj.velocity *= damping;

        // Oprire dacă viteza este prea mică
        if (obj.velocity.length() < minVelocity) {
            obj.velocity = QVector3D(0, 0, 0);
            obj.isDynamic = false;
        }

        // Update poziție și transform
        obj.position = newPosition;
        obj.transform->setTranslation(newPosition);

        // Update bounding box
        QVector3D minBounds, maxBounds;
        calculateBoundingBox(obj.type, obj.size, minBounds, maxBounds);
        obj.boundingBoxMin = newPosition + minBounds;
        obj.boundingBoxMax = newPosition + maxBounds;
    }

    // Verificare coliziuni între obiecte
    checkObjectCollisions();
}

void MyOpenGLWidget::checkObjectCollisions()
{
    QVector<QString> objectIds = m_sceneObjects.keys().toVector();

    for (int i = 0; i < objectIds.size(); ++i) {
        for (int j = i + 1; j < objectIds.size(); ++j) {
            SceneObject &obj1 = m_sceneObjects[objectIds[i]];
            SceneObject &obj2 = m_sceneObjects[objectIds[j]];

            // Verificare coliziune sphere
            if (checkSphereCollision(obj1, obj2)) {
                // Aplicare impuls doar dacă unul dintre obiecte este dinamic
                if (obj1.isDynamic && !obj2.isDynamic) {
                    applyImpulse(obj2, obj1);
                }
                else if (obj2.isDynamic && !obj1.isDynamic) {
                    applyImpulse(obj1, obj2);
                }
                else if (obj1.isDynamic && obj2.isDynamic) {
                    // Ambele dinamice - schimb de impuls
                    QVector3D direction = obj2.position - obj1.position;
                    direction.normalize();

                    QVector3D relativeVelocity = obj1.velocity - obj2.velocity;
                    float velocityAlongNormal = QVector3D::dotProduct(relativeVelocity, direction);

                    if (velocityAlongNormal > 0) continue; // Obiectele se îndepărtează

                    float impulse = 2 * velocityAlongNormal / 2; // Masa egală pentru simplitate
                    QVector3D impulseVector = direction * impulse;

                    obj1.velocity -= impulseVector;
                    obj2.velocity += impulseVector;
                }
            }
        }
    }
}

// Implementare Settings
void MyOpenGLWidget::loadSettings()
{
    m_language = m_settings->value("language", "en").toString();
    m_floorLevel = m_settings->value("floorLevel", -2.0f).toFloat();
    m_floorSize = m_settings->value("floorSize", 20.0f).toFloat();

    // Configurări cameră
    if (view && view->camera()) {
        QVector3D cameraPos = m_settings->value("cameraPosition", QVector3D(0, 15, 30)).value<QVector3D>();
        QVector3D cameraTarget = m_settings->value("cameraTarget", QVector3D(0, 0, 0)).value<QVector3D>();

        view->camera()->setPosition(cameraPos);
        view->camera()->setViewCenter(cameraTarget);
    }
}

void MyOpenGLWidget::saveSettings()
{
    m_settings->setValue("language", m_language);
    m_settings->setValue("floorLevel", m_floorLevel);
    m_settings->setValue("floorSize", m_floorSize);

    // Salvare configurări cameră
    if (view && view->camera()) {
        m_settings->setValue("cameraPosition", view->camera()->position());
        m_settings->setValue("cameraTarget", view->camera()->viewCenter());
    }

    m_settings->sync();
}

// Funcții utilitare suplimentare
void MyOpenGLWidget::resetCamera()
{
    if (view && view->camera()) {
        view->camera()->setPosition(QVector3D(0, 15, 30.0f));
        view->camera()->setViewCenter(QVector3D(0, 0, 0));
        view->camera()->setUpVector(QVector3D(0, 1, 0));
    }
}

void MyOpenGLWidget::setFloorLevel(float level)
{
    m_floorLevel = level;

    // Update poziția podelei
    if (floorEntity) {
        Qt3DCore::QTransform *floorTransform =
            floorEntity->componentsOfType<Qt3DCore::QTransform>().first();
        if (floorTransform) {
            floorTransform->setTranslation(QVector3D(0, m_floorLevel, 0));
        }
    }

    // Repoziționare obiecte dacă e nevoie
    for (auto it = m_sceneObjects.begin(); it != m_sceneObjects.end(); ++it) {
        SceneObject &obj = it.value();
        QVector3D newPos = getFloorConstrainedPosition(obj.position, obj.boundingSphereRadius);
        if (newPos != obj.position) {
            obj.position = newPos;
            if (obj.transform) {
                obj.transform->setTranslation(newPos);
            }
        }
    }
}

void MyOpenGLWidget::setFloorSize(float size)
{
    m_floorSize = size;

    // Update dimensiunea podelei
    if (floorEntity) {
        Qt3DExtras::QPlaneMesh *floorMesh =
            floorEntity->componentsOfType<Qt3DExtras::QPlaneMesh>().first();
        if (floorMesh) {
            floorMesh->setWidth(m_floorSize);
            floorMesh->setHeight(m_floorSize);
        }
    }
}

QStringList MyOpenGLWidget::getAvailableAnimations() const
{
    return QStringList() << "rotate" << "bounce" << "float" << "pulse" << "swing"
                        << "orbit" << "circle" << "revolve";
}

QStringList MyOpenGLWidget::getLoadedObjectIds() const
{
    return m_sceneObjects.keys();
}

SceneObject MyOpenGLWidget::getObjectById(const QString &id) const
{
    return m_sceneObjects.value(id, SceneObject());
}

void MyOpenGLWidget::removeObject(const QString &id)
{
    if (m_sceneObjects.contains(id)) {
        SceneObject &obj = m_sceneObjects[id];
        if (obj.entity) {
            delete obj.entity;
        }
        m_sceneObjects.remove(id);

        // Elimină animațiile orbitale asociate
        for (int i = m_orbitalAnimations.size() - 1; i >= 0; --i) {
            if (m_orbitalAnimations[i].primaryObjectId == id ||
                m_orbitalAnimations[i].referenceObjectId == id) {
                m_orbitalAnimations.removeAt(i);
            }
        }
    }
}

void MyOpenGLWidget::pauseAnimations()
{
    m_animationTimer->stop();
}

void MyOpenGLWidget::resumeAnimations()
{
    m_animationTimer->start(16);
}

void MyOpenGLWidget::pausePhysics()
{
    m_physicsTimer->stop();
}

void MyOpenGLWidget::resumePhysics()
{
    m_physicsTimer->start(16);
}
