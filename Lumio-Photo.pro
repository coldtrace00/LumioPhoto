# Альтернатива CMake для qmake / Qt Creator
QT += widgets sql printsupport network
CONFIG += c++17
TARGET = LumioPhoto
TEMPLATE = app
VERSION = 1.0.0
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

SOURCES += $$files(src/*.cpp)
HEADERS += $$files(src/*.h)

win32 {
    RC_ICONS = resources/app.ico
    LIBS += -luser32 -lshell32 -ladvapi32
}
