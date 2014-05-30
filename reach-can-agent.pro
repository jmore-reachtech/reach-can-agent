TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += src/can_agent.c \
        src/can_local.c \
        src/can_tio_socket.c \
        src/can_server_socket.c \
        src/logmsg.c

HEADERS += src/can_agent.h

