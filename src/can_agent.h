#ifndef CAN_AGENT_H
#define CAN_AGENT_H

#include <syslog.h>
#include <sys/stat.h>
#include <stdint.h>

/* functions defined in can_server_socket.c */
int canBusSocketInit(int canPort);
int canBusSocketRead(int newFd, char *msgBuff);
void canBusSocketWrite(int socketFd, const char *buff);


/* functions defined in can_tcp_socket.c */
/* functions defined in sio_socket.c */
int canQMLConnect(unsigned short port);
int canQMLSocketRead(int newFd, char *msgBuff, size_t bufferSize);
void canQMLSocketWrite(int socketFd, const char *buff);

/* functions defined in can_local.c */
char *canHandleLocal(char *qmlString);

/* functions exported from logmsg.c */
void LogOpen(const char *ident, int logToSyslog, const char *logFilePath,
    int verboseFlag);
void LogMsg(int level, const char *fmt, ...);

#define CAN_DEFAULT_SERVER_AGENT_PORT 0
#define TCP_DEFAULT_PORT 4000

#define CAN_BUFFER_SIZE 128
#define CAN_BAUD_RATE 1000000
#define NETWORK_CAN     2

/* structs for CAN */
typedef struct {
    char     if_name[32];
    #define _NET_USBOTG_LOADED  (0x01 << 0)
    #define _NET_CAN_LOADED     (0x01 << 1)
    #define _NET_INTERFACE_UP   (0x01 << 2)
    uint8_t flags;
} ethIf_t;


#endif  /* CAN_AGENT_H */
