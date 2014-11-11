TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
TARGET=can-agent
VERSION = 1.0.1
# add #define for the version
DEFINES += CAN_VERSION=\\\"$$VERSION\\\"
SOURCES += src/can_agent.c \
        src/can_local.c \
        src/can_tio_socket.c \
        src/can_server_socket.c \
        src/logmsg.c

HEADERS += src/can_agent.h

