QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    commhelper.cpp \
    globalsettings.cpp \
    main.cpp \
    mainwindow.cpp \
    netsetting.cpp \
    paramsetting.cpp

HEADERS += \
    qlitethread.h \
    commhelper.h \
    globalsettings.h \
    mainwindow.h \
    netsetting.h \
    paramsetting.h

FORMS += \
    mainwindow.ui \
    netsetting.ui \
    paramsetting.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES +=

DESTDIR = $$PWD/../build
contains(QT_ARCH, x86_64) {
    # x64
    DESTDIR = $$DESTDIR/x64
} else {
    # x86
    DESTDIR = $$DESTDIR/x86
}

DESTDIR = $$DESTDIR/qt$$QT_VERSION/
message(DESTDIR = $$DESTDIR)

TARGET = LowXRayFSS

# 避免创建空的debug和release目录
CONFIG -= debug_and_release

#指定编译产生的文件分门别类放到对应目录
MOC_DIR     = temp/moc
RCC_DIR     = temp/rcc
UI_DIR      = temp/ui
OBJECTS_DIR = temp/obj

#把所有警告都关掉眼不见为净
CONFIG += warn_off

#开启大资源支持
CONFIG += resources_big

#############################################################################################################
exists (./.git) {
    GIT_BRANCH   = $$system(git rev-parse --abbrev-ref HEAD)
    GIT_TIME     = $$system(git show --oneline --format=\"%ci%H\" -s HEAD)
    APP_VERSION = "Git: $${GIT_BRANCH}: $${GIT_TIME}"
} else {
    GIT_BRANCH      = None
    GIT_TIME        = None
    APP_VERSION     = None
}

DEFINES += GIT_BRANCH=\"\\\"$$GIT_BRANCH\\\"\"
DEFINES += GIT_TIME=\"\\\"$$GIT_TIME\\\"\"
DEFINES += APP_VERSION=\"\\\"$$APP_VERSION\\\"\"

windows {
    # MinGW
    *-g++* {
        QMAKE_CXXFLAGS += -Wall -Wextra -pedantic
    }
    # MSVC
    *-msvc* {
        QMAKE_CXXFLAGS += /utf-8
    }
}

include($$PWD/../3rdParty/log4qt/Include/log4qt.pri)
include($$PWD/../3rdParty/resource/resource.pri)
include($$PWD/../3rdParty/QCustomplot/QCustomplot.pri)
