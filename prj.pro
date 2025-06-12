QT       += core gui 3dcore 3drender 3dinput 3dextras 3dquick 3dlogic

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets openglwidgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    PBRMaterial.cpp \
    camera.cpp \
    main.cpp \
    mainwindow.cpp \
    myopenglwidget.cpp

HEADERS += \
    PBRMaterial.h \
    camera.h \
    mainwindow.h \
    myopenglwidget.h

FORMS += \
    mainwindow.ui

DISTFILES += \
    Models/* \
    pbr.frag \
    pbr.vert

DEPLOYMENTFOLDERS = Models

LIBS += -lopengl32

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
