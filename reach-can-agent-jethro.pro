TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
TARGET=can-agent
VERSION = 1.0.2
target.path=/application/bin
INSTALLS += target

QMAKE_CFLAGS_DEBUG =  -O0 -pipe -g -feliminate-unused-debug-types
# add #define for the version
DEFINES += CAN_VERSION=\\\"$$VERSION\\\"
SOURCES += src/can_agent.c \
        src/can_local.c \
        src/logmsg.c \
    src/can_tcp_socket.c \
    src/can_bus_socket.c

HEADERS += src/can_agent.h

