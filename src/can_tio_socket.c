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
    LogMsg(LOG_ERR, "Exiting: %s\n", errorMessage);
    exit(1);
}

static int canCreateUnixServerSocket(const char *socketPath)
{
    int sock;
    struct sockaddr_un echoServAddr;

    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        canDieWithError("socket() failed");
    }

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sun_family = AF_UNIX; 
    strncpy(echoServAddr.sun_path, socketPath, sizeof(echoServAddr.sun_path));
    echoServAddr.sun_path[sizeof(echoServAddr.sun_path) - 1] = '\0';

    /* remove socket if exists */
    const int status = unlink(socketPath);
    if ((status != 0) && (errno != ENOENT)) {
        canDieWithError("socket file unlink failed\n");
    }

    if (bind(sock, (struct sockaddr *)&echoServAddr,
        sizeof(echoServAddr)) < 0) {
        canDieWithError("bind() failed");
    }

    if (listen(sock, MAXPENDING) < 0) {
        canDieWithError("listen() failed");
    }

    return sock;
}

static int canCreateTCPServerSocket(unsigned short port)
{
    int sock;
    struct sockaddr_in echoServAddr;

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        canDieWithError("socket() failed");
    }

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET; 
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&echoServAddr,
        sizeof(echoServAddr)) < 0) {
        canDieWithError("bind() failed");
    }

    if (listen(sock, MAXPENDING) < 0) {
        canDieWithError("listen() failed");
    }

    return sock;
}


int canTioSocketAccept(int serverFd, int addressFamily)
{
    /* define a variable to hold the client's address for either family */
    union {
        struct sockaddr_un unixClientAddr;
        struct sockaddr_in inetClientAddr;
    } clientAddr;
    socklen_t clientLength = sizeof(clientAddr);

    const int clientFd = accept(serverFd, (struct sockaddr *)&clientAddr,
        &clientLength);
    if (clientFd >= 0) {
        switch (addressFamily) {
        case AF_UNIX:
            LogMsg(LOG_INFO, "Handling Unix client\n");
            break;

        case AF_INET:
            LogMsg(LOG_INFO, "Handling TCP client %s\n",
                inet_ntoa(clientAddr.inetClientAddr.sin_addr));
            break;

        default:
            break;
        }
    }

    return clientFd;
}


int canTioSocketInit(int *addressFamily,
    const char *unixSocketPath)
{
    int listenFd = -1;
    /* create a Unix domain socket */
    listenFd = canCreateUnixServerSocket(unixSocketPath);
    *addressFamily = AF_UNIX;
    return listenFd;
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
int canTioSocketRead(int socketFd, char *msgBuff, size_t bufferSize)
{
    int cnt;

    if ((cnt = recv(socketFd, msgBuff, bufferSize, 0)) <= 0) {
        LogMsg(LOG_INFO, "%s(): recv() failed, client closed\n", __FUNCTION__);
        close(socketFd);
        return -1;
    } else {
        msgBuff[cnt] = 0;
        LogMsg(LOG_INFO, "%s: buff = %s", __FUNCTION__, msgBuff);
        return cnt;
    }
}


void canTioSocketWrite(int socketFd, const char *buff)
{
    int cnt = strlen(buff);

    if (send(socketFd, buff, cnt, 0) != cnt) {
        LogMsg(LOG_ERR, "socket_send_to_client(): send() failed, %d\n",
            socketFd);
        perror("what's messed up?");
    }
    else
    {
        LogMsg(LOG_INFO, "%s: buff = %s", __FUNCTION__, buff);
    }
}


