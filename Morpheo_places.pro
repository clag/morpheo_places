#-------------------------------------------------
#
# Project created by QtCreator 2015-01-01T23:19:08
#
#-------------------------------------------------

QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Morpheo_places
TEMPLATE = app


SOURCES += src/main.cpp\
src/MainWindow.cpp \
src/Database.cpp \
src/Voies.cpp \
src/Graphe.cpp \
src/Arcs.cpp

HEADERS += src/MainWindow.h \
src/Database.h \
src/Graphe.h \
src/Voies.h \
src/Logger.h \
src/Arcs.h

FORMS    += src/mainwindow.ui
