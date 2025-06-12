#pragma once
#ifndef PBRMATERIAL_H
#define PBRMATERIAL_H

#include <Qt3DRender/QAbstractTexture>
#include <Qt3DRender/QMaterial>
#include <Qt3DRender/QParameter>
#include <Qt3DRender/QTextureImage>
#include <Qt3DRender/QEffect>
#include <Qt3DRender/QTechnique>
#include <Qt3DRender/QRenderPass>
#include <Qt3DRender/QShaderProgram>
#include <Qt3DRender/QGraphicsApiFilter>
#include <QColor>
#include <QString>
#include <Qt3DRender/QTextureWrapMode>
#include <QVector3D>

class SimpleTexture2D : public Qt3DRender::QAbstractTexture
{
    Q_OBJECT
public:
    explicit SimpleTexture2D(Qt3DCore::QNode *parent = nullptr)
        : Qt3DRender::QAbstractTexture(Qt3DRender::QAbstractTexture::Target2D, parent)
    {
        setMinificationFilter(LinearMipMapLinear);
        setMagnificationFilter(Linear);
        setWrapMode(Qt3DRender::QTextureWrapMode(Qt3DRender::QTextureWrapMode::Repeat));
        setGenerateMipMaps(true);
    }
};

class PBRMaterial : public Qt3DRender::QMaterial {
    Q_OBJECT

public:
    explicit PBRMaterial(Qt3DCore::QNode *parent = nullptr,
                         const QString &baseName = QString(),
                         const QColor &albedoColor = QColor(128, 128, 128));
    ~PBRMaterial();

private:
    void setupTextures(const QString &baseName, const QColor &albedoColor);
    SimpleTexture2D* loadOrPlaceholder(const QString &fullPath, const QString &uniformName, const QColor &albedoColor);
    SimpleTexture2D* createSolidColorTexture(const QColor &color);
    SimpleTexture2D* createDefaultTexture(const QString &type, const QColor &albedoColor);
};

#endif // PBRMATERIAL_H
