#-------------------------------------------------
#
# Project created by QtCreator 2019-06-02T10:33:42
#
#-------------------------------------------------

QT       += core gui

CONFIG   += c++14

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = rssdisk_client
TEMPLATE = app

DEFINES += QT_DEPRECATED_WARNINGS

RC_ICONS = ../resources/hdd.png

LIBS += -lpthread -lz -lcurl -lcrypto -lssl -lcryptopp

SOURCES += main.cpp\
        mainwindow.cpp \
    ../../../libs/basefunc_std.cpp \
    ../../client/rssdisk_pool.cpp \
    ../../../libs/network/tcp_client.cpp \
    ../../../libs/json/jsoncpp.cpp \
    ../../../libs/commpression_zlib.cpp \
    ../../../libs/curl_http.cpp \
    ../../../libs/DH/dh.cpp \
    ../../../libs/network/network_std.cpp \
    ../../../libs/base64.cpp \
    ../../../libs/md5.cpp \
    widjets/server_item2.cpp \
    ../../../libs/gost_28147_89/gost_89.cpp \
    classes/qlabelclicked.cpp \
    classes/pieview.cpp

HEADERS  += mainwindow.h \
    ../../../libs/basefunc_std.h \
    ../../../libs/network/tcp_client.hpp \
    ../../../libs/commpression_zlib.h \
    ../../../libs/curl_http.h \
    ../../../libs/DH/dh.h \
    ../../../libs/network/network_std.h \
    ../../../libs/base64.h \
    ../../../libs/md5.h \
    widjets/server_item2.h \
    ../../../libs/gost_28147_89/gost_89.h \
    classes/qlabelclicked.h \
    ../../checker/checker.h \
    classes/pieview.h

FORMS    += mainwindow.ui \
    widjets/server_item2.ui

RESOURCES += \
    res.qrc
