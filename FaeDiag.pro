QT += core gui widgets serialport

CONFIG += c++17

TARGET = FaeDiag
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    SerialPortManager.cpp \
    AdbManager.cpp

HEADERS += \
    mainwindow.h \
    LogQueue.h \
    SerialPortManager.h \
    AdbManager.h

FORMS += \
    mainwindow.ui

# 设置 Windows EXE 图标
RC_FILE = appicon.rc

# 添加资源文件
RESOURCES += resources.qrc
