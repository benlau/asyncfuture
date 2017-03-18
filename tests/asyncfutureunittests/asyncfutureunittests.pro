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
    testfunctions.cpp

DEFINES += SRCDIR=\\\"$$PWD/\\\" QUICK_TEST_SOURCE_DIR=\\\"$$PWD/qmltests\\\"

include(vendor/vendor.pri)
include(../../asyncfuture.pri)

DISTFILES +=     qpm.json     qmltests/tst_QmlTests.qml \
    ../../README.md

HEADERS += \    
    example.h \
    asyncfuturetests.h \
    testfunctions.h

