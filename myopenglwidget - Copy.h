#ifndef MYOPENGLWIDGET_H
#define MYOPENGLWIDGET_H

#include <QObject>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMouseEvent>
#include <QTimer>
#include "myopenglwidget.h"
// #include "camera.h"

#include <Qt3DCore/QEntity>
#include <Qt3DRender/QCamera>
#include <Qt3DRender/QMesh>
#include <Qt3DExtras/Qt3DWindow>
#include <Qt3DExtras/QPhongMaterial>
#include <Qt3DExtras/QOrbitCameraController>

class MyOpenGLWidget : public QWidget
{
    Q_OBJECT
public:
    MyOpenGLWidget(QWidget *parent = nullptr);
    ~MyOpenGLWidget();

    void loadModel(const QString &filePath);
    void clearScene();
    void loadScene(const QString &filePath);
    // void loadModelInScene(const QString &objectType, const QString &color, const QString &texture, const QString &size, float x, float y, float z);
    void loadModelInScene(const QString &objectType, float x, float y, float z);

protected:
    // void initializeGL();
    // void resizeGL(int w, int h);
    // void paintGL();

    // void mousePressEvent(QMouseEvent *event) override;
    // void mouseReleaseEvent(QMouseEvent *event) override;
    // void mouseMoveEvent(QMouseEvent *event) override;
    // void wheelEvent(QWheelEvent *event) override;
    // void keyPressEvent(QKeyEvent *event) override;
    QVector3D generateNonCollidingPosition(const QVector<QVector3D>& usedPositions, QString size);

private slots:
    // void onTimeout();

private:
    Qt3DExtras::Qt3DWindow *view;
    Qt3DCore::QEntity *rootEntity;
    Qt3DRender::QCamera *camera;
    Qt3DCore::QEntity *modelEntity;
    // QTimer *timer;
    // Camera *camera;
    // QPoint lastMousePosition;
    // bool rightMouseButtonPressed;
    // float rotationAngle;

};

#endif // MYOPENGLWIDGET_H
