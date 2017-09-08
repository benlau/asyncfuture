#-------------------------------------------------
#
# Project created by QtCreator 2016-02-25T18:56:34
#
#-------------------------------------------------

QT       += testlib qml concurrent

TARGET = asyncfutureunittests
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

SOURCES +=     main.cpp \
    example.cpp \
    asyncfuturetests.cpp \
    bugtests.cpp

DEFINES += SRCDIR=\\\"$$PWD/\\\" QUICK_TEST_SOURCE_DIR=\\\"$$PWD/qmltests\\\"

include(vendor/vendor.pri)
include(../../asyncfuture.pri)

DISTFILES +=     qpm.json  \
    ../../README.md \
    ../../qpm.json

HEADERS += \
    example.h \
    asyncfuturetests.h \
    testfunctions.h \
    bugtests.h

