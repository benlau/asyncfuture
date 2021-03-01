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
    FinishedAndCancelThread.cpp \
    example.cpp \
    bugtests.cpp \
    samplecode.cpp \
    cookbook.cpp \
    testclass.cpp \
    trackingdata.cpp \
    spec.cpp

DEFINES += SRCDIR=\\\"$$PWD/\\\" QUICK_TEST_SOURCE_DIR=\\\"$$PWD/qmltests\\\"

include(vendor/vendor.pri)
include(../../asyncfuture.pri)

DISTFILES +=     qpm.json  \
    ../../README.md \
    ../../.travis.yml \
    ../../qpm.json

HEADERS += \
    FinishedAndCancelThread.h \
    example.h \
    testclass.h \
    testfunctions.h \
    bugtests.h \
    samplecode.h \
    cookbook.h \
    trackingdata.h \
    spec.h \
    tools.h

#!win32 {
#    QMAKE_CXXFLAGS += -Werror
#}
