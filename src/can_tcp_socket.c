#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "can_agent.h"

#define MAXPENDING 1

static void canDieWithError(char *errorMessage)
{
    LogMsg(LOG_ERR, "[CAN] Exiting: %s\n", errorMessage);
    exit(1);
}

/**
 * Initializes a socket for read and writes to the
 * qml-viewer via TCP/IP.
 *
 * @param port the TCP/IP port to connect to.
 *
 */
int canQMLConnect(unsigned short port)
{
    int sock;
    struct sockaddr_in echoServAddr;

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        canDieWithError("[CAN] socket() failed");
    }

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(port);

    if (connect(sock,(struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
        canDieWithError("[CAN] qml-viewer connect() failed");
    else
        LogMsg(LOG_INFO, "[CAN] connected to qml-viewer on port %d\n", port);

    return sock;
}


/**
 * Reads a single message from the socket connected to the
 * tio-agent. If no message is ready to be received, the call
 * will block until one is available.
 *
 * @param socketFd the file descriptor of for the already open
 *                 socket connecting to the tio-agent
 * @param msgBuff address of a contiguous array into which the
 *                message will be written upon receipt from the
 *                tio-agent
 * @param bufferSize the number of bytes in msgBuff
 *
 * @return int 0 if no message to return (handled here), -1 if
 *         recv() returned an error code (close connection) or
 *         >0 to indicate msgBuff has that many characters
 *         filled in
 */
int canQMLSocketRead(int socketFd, char *msgBuff, size_t bufferSize)
{
    int cnt;

    if ((cnt = recv(socketFd, msgBuff, bufferSize, 0)) <= 0) {
        LogMsg(LOG_INFO, "[CAN] %s(): recv() failed, client closed\n", __FUNCTION__);
        close(socketFd);
        return -1;
    } else {
        char str[cnt-2];
        strncpy(str, msgBuff, cnt-2);
        LogMsg(LOG_INFO, "[CAN] received from qml-viewer => \"%s\"\n", str);
        msgBuff[cnt] = 0;
        return cnt;
    }
}

/**
 * Sends a single message to the socket connected to the
 * TCP/IP port.
 *
 * @param socketFd the file descriptor for the already open
 *                 socket.
 * @param msgBuff  address of a contiguous array that contains
 *                 the message.
 *
 */
void canQMLSocketWrite(int socketFd, const char *buff)
{
    int cnt = strlen(buff);
    LogMsg(LOG_INFO, "[CAN] sending to qml-viewer] => \"%s\"\n", buff);

    if (send(socketFd, buff, cnt, 0) != cnt) {
        LogMsg(LOG_ERR, "[CAN] socket_send_to_client(): send() failed, %d\n",
            socketFd);
        perror("what's messed up?");
    }
}