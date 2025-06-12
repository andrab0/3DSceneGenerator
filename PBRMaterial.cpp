#include "PBRMaterial.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDebug>
#include <QImage>
#include <QUrl>
#include <QDir>
#include <QStandardPaths>

QString findExistingTextureFile(const QString &basePath)
{
    static QStringList extensions = { ".png", ".jpg", ".jpeg", ".exr" };

    for (const QString &ext : extensions) {
        QString candidate = basePath + ext;
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return QString(); // nimic găsit
}

PBRMaterial::PBRMaterial(Qt3DCore::QNode *parent, const QString &baseName, const QColor &albedoColor)
    : Qt3DRender::QMaterial(parent)
{
    qDebug() << "🔧 Creating FIXED PBR Material for:" << baseName << "albedo:" << albedoColor.name();

    auto *effect = new Qt3DRender::QEffect(this);
    auto *technique = new Qt3DRender::QTechnique(effect);
    technique->graphicsApiFilter()->setApi(Qt3DRender::QGraphicsApiFilter::OpenGL);
    technique->graphicsApiFilter()->setMajorVersion(3);
    technique->graphicsApiFilter()->setMinorVersion(3);
    technique->graphicsApiFilter()->setProfile(Qt3DRender::QGraphicsApiFilter::CoreProfile);

    auto *renderPass = new Qt3DRender::QRenderPass(technique);
    auto *shader = new Qt3DRender::QShaderProgram(renderPass);
    QString shaderBase = QCoreApplication::applicationDirPath() + "/../../../";

    qDebug() << "🔧 Loading shaders from:" << shaderBase;
    shader->setVertexShaderCode(Qt3DRender::QShaderProgram::loadSource(QUrl::fromLocalFile(shaderBase + "pbr.vert")));
    shader->setFragmentShaderCode(Qt3DRender::QShaderProgram::loadSource(QUrl::fromLocalFile(shaderBase + "pbr.frag")));

    renderPass->setShaderProgram(shader);

    technique->addRenderPass(renderPass);
    effect->addTechnique(technique);
    setEffect(effect);

    setupTextures(baseName, albedoColor);

    // Simplified lighting (match your shader)
    addParameter(new Qt3DRender::QParameter(QStringLiteral("lightPositions[0]"), QVector3D(0.0f, 10.0f, 10.0f)));
    addParameter(new Qt3DRender::QParameter(QStringLiteral("lightColors[0]"), QVector3D(1.0f, 1.0f, 1.0f)));
    addParameter(new Qt3DRender::QParameter(QStringLiteral("lightIntensity"), 100.0f));
    addParameter(new Qt3DRender::QParameter(QStringLiteral("camPos"), QVector3D(0.0f, 15.0f, 30.0f)));

    qDebug() << "✅ FIXED PBR Material created successfully";
}

PBRMaterial::~PBRMaterial() = default;

void PBRMaterial::setupTextures(const QString &baseName, const QColor &albedoColor)
{
    QString path = QCoreApplication::applicationDirPath() + "/../../../Models/Textures/";
    QString base = path + baseName;

    struct Tex { QString suffix, uniform; };
    QVector<Tex> texList = {
        { "_diff",      "albedoMap" },
        { "_nor_gl",    "normalMap" },
        { "_roughness", "roughnessMap" },
        { "_metallic",  "metallicMap" }
    };

    int loaded = 0;
    for (const auto &tex : texList) {
        QString full = findExistingTextureFile(base + tex.suffix);

        SimpleTexture2D *texture = nullptr;

        if (QFileInfo::exists(full)) {
            qDebug() << "✅ Loading real texture:" << full;
            texture = loadOrPlaceholder(full, tex.uniform, albedoColor);
            loaded++;
        } else {
            qDebug() << "📋 Creating default texture for:" << tex.uniform;
            texture = createDefaultTexture(tex.uniform, albedoColor);
        }

        if (texture) {
            addParameter(new Qt3DRender::QParameter(tex.uniform, texture));
            qDebug() << "✅ Added texture parameter:" << tex.uniform;
        } else {
            qDebug() << "❌ Failed to create texture for:" << tex.uniform;
        }
    }

    qDebug() << "🎨 Loaded" << loaded << "real textures for" << baseName;

    // Always add albedo color as fallback
    addParameter(new Qt3DRender::QParameter(QStringLiteral("albedoColor"), QVector3D(
            albedoColor.redF(), albedoColor.greenF(), albedoColor.blueF())));
}

SimpleTexture2D* PBRMaterial::loadOrPlaceholder(const QString &filePath, const QString &uniformName, const QColor &albedoColor)
{
    auto *tex = new SimpleTexture2D(this);
    Qt3DRender::QTextureImage *img = new Qt3DRender::QTextureImage(tex);

    if (QFileInfo::exists(filePath)) {
        qDebug() << "📂 Loading texture file:" << filePath;
        img->setSource(QUrl::fromLocalFile(filePath));
        tex->addTextureImage(img);
        return tex;
    } else {
        qDebug() << "❌ Texture file not found:" << filePath;
        delete tex;
        return createDefaultTexture(uniformName, albedoColor);
    }
}

SimpleTexture2D* PBRMaterial::createSolidColorTexture(const QColor &color)
{
    auto *tex = new SimpleTexture2D(this);

    // Create a simple 1x1 color texture in memory (no file save)
    QImage image(1, 1, QImage::Format_RGBA8888);
    image.fill(color);

    // Create a temporary file in system temp directory
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tempDir);
    QString tempFile = tempDir + QString("/pbr_color_%1_%2_%3.png")
                                    .arg(color.red())
                                    .arg(color.green())
                                    .arg(color.blue());

    if (image.save(tempFile)) {
        Qt3DRender::QTextureImage *img = new Qt3DRender::QTextureImage(tex);
        img->setSource(QUrl::fromLocalFile(tempFile));
        tex->addTextureImage(img);
        qDebug() << "✅ Created color texture:" << tempFile;
        return tex;
    } else {
        qDebug() << "❌ Failed to save color texture";
        delete tex;
        return nullptr;
    }
}

SimpleTexture2D* PBRMaterial::createDefaultTexture(const QString &type, const QColor &albedoColor)
{
    auto *tex = new SimpleTexture2D(this);

    // Create appropriate default based on texture type
    QImage image(4, 4, QImage::Format_RGBA8888);

    if (type == "albedoMap") {
        // Use albedo color
        image.fill(albedoColor);
        qDebug() << "📋 Creating albedo placeholder with color:" << albedoColor.name();
    }
    else if (type == "normalMap") {
        // Flat normal: RGB(128, 128, 255) = normal pointing up
        image.fill(QColor(128, 128, 255, 255));
        qDebug() << "📋 Creating normal placeholder (flat)";
    }
    else if (type == "roughnessMap") {
        // Medium roughness: RGB(128, 128, 128) = 0.5 roughness
        image.fill(QColor(128, 128, 128, 255));
        qDebug() << "📋 Creating roughness placeholder (0.5)";
    }
    else if (type == "metallicMap") {
        // Non-metallic: RGB(0, 0, 0) = 0.0 metallic
        image.fill(QColor(0, 0, 0, 255));
        qDebug() << "📋 Creating metallic placeholder (0.0)";
    }
    else {
        // Default: white
        image.fill(QColor(255, 255, 255, 255));
        qDebug() << "📋 Creating default white placeholder";
    }

    // Save to temp directory
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(tempDir);
    QString tempFile = tempDir + QString("/pbr_%1_default.png").arg(type);

    if (image.save(tempFile)) {
        Qt3DRender::QTextureImage *img = new Qt3DRender::QTextureImage(tex);
        img->setSource(QUrl::fromLocalFile(tempFile));
    tex->addTextureImage(img);
        qDebug() << "✅ Created default texture:" << tempFile;
    return tex;
    } else {
        qDebug() << "❌ Failed to save default texture for:" << type;
        delete tex;
        return nullptr;
    }
}
