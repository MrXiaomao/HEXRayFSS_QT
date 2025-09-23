QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    aboutwidget.cpp \
    commhelper.cpp \
    dataprocessor.cpp \
    globalsettings.cpp \
    main.cpp \
    mainwindow.cpp \
    netsetting.cpp \
    paramsetting.cpp \
    switchbutton.cpp

HEADERS += \
    aboutwidget.h \
    dataprocessor.h \
    qlitethread.h \
    commhelper.h \
    globalsettings.h \
    mainwindow.h \
    netsetting.h \
    paramsetting.h \
    switchbutton.h

FORMS += \
    aboutwidget.ui \
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

#开启工程的debug和release两种版本构建
#CONFIG += debug_and_release
#避免创建空的debug和release目录
CONFIG -= debug_and_release
CONFIG(debug, debug|release) {
    TARGET = LowXRayFSSd
} else {
    TARGET = LowXRayFSS
}

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
    GIT_VERSION = "Git: $${GIT_BRANCH}: $${GIT_TIME}"
} else {
    GIT_BRANCH      = None
    GIT_TIME        = None
    GIT_VERSION     = None
}

DEFINES += GIT_BRANCH=\"\\\"$$GIT_BRANCH\\\"\"
DEFINES += GIT_TIME=\"\\\"$$GIT_TIME\\\"\"
DEFINES += GIT_VERSION=\"\\\"$$GIT_VERSION\\\"\"
DEFINES += APP_VERSION="\\\"V1.0.1\\\""

windows {
    # MinGW
    *-g++* {
        QMAKE_CXXFLAGS += -Wall -Wextra -pedantic        
        #QMAKE_CXXFLAGS += -finput-charset=UTF-8
        #QMAKE_CXXFLAGS += -fexec-charset=UTF-8
        #QMAKE_CXXFLAGS += -fwide-exec-charset=UTF-16
        #设置wchar_t类型数据的编码格式。不同主机值可能不同，编译器运行时根据主机情况会自动识别出最符合
        #主机的方案作为默认值，这个参数是不需要动的。UTF-16 UTF-16BE UTF-16LE UTF-32LE UTF-32BE
    }
    # MSVC
    *-msvc* {
        QMAKE_CXXFLAGS += /utf-8
        QMAKE_CXXFLAGS += /source-charset:utf-8
        QMAKE_CXXFLAGS += /execution-charset:utf-8
    }
}

include($$PWD/../3rdParty/log4qt/Include/log4qt.pri)
include($$PWD/../3rdParty/resource/resource.pri)
include($$PWD/../3rdParty/QCustomplot/QCustomplot.pri)
#include($$PWD/../3rdParty/QGoodWindow/QGoodWindow/src/theme/theme.pri)
include($$PWD/../3rdParty/QGoodWindow/QGoodWindow/QGoodWindow.pri)
include($$PWD/../3rdParty/QGoodWindow/QGoodCentralWidget/QGoodCentralWidget.pri)
# include($$PWD/../3rdParty/QtFramelessWindow/QtFramelessWindow.pri)

# 启用反解能谱
#DEFINES += ENABLE_MATLAB

contains(DEFINES, ENABLE_MATLAB) {
    message("Including matlab library")
    include($$PWD/../3rdParty/Matlab2020b/Matlab2020b.pri)
} else {
    message("Skipping matlab library")
}

# 添加config配置
# CONFIG += precompile_header
# 指定要使用的预编译头文件
# PRECOMPILED_HEADER += stable.h
