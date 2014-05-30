#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h> 
#include <sys/un.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/can.h>
#include <net/if.h>
#include <sys/ioctl.h>

#include "can_agent.h"

#define MAXPENDING 1

static void canDieWithError(char *errorMessage)
{
    LogMsg(LOG_ERR, "Exiting: %s\n", errorMessage);
    exit(1);
}

static int canCreateServerSocket(int instance)
{
    int sock = -1;
    int rv = 0;
    struct sockaddr_can sAddr;
    struct ifreq ifr;

    sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0)
    {
        canDieWithError("can socket() failed");
    }

    memset(&ifr, 0, sizeof(ifr));
    sprintf(ifr.ifr_name, "can%d", instance);


    rv = ioctl(sock, SIOGIFINDEX, &ifr);
    if (rv < 0)
    {
        canDieWithError("Error: ioctl(SIOGIFINDEX) failed CAN BUS may not be up.");
    }

    memset(&sAddr, 0, sizeof(sAddr));
    sAddr.can_family = AF_CAN;
    sAddr.can_ifindex = ifr.ifr_ifindex;

    rv = bind(sock, (struct sockaddr *)&sAddr, sizeof(sAddr));


    if (rv < 0)
    {
        canDieWithError("Error: CAN bind() failed.");
    }

    LogMsg(LOG_INFO, "Handling CAN Bus client\n");

    return sock;
}

int canServerSocketInit(int instance)
{
    int listenFd = -1;
    listenFd = canCreateServerSocket(instance);
    return listenFd;
}


/**
 * Reads a single message from the socket connected to the 
 * tcp/ip server port. If no message is ready to be received, the call
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
int canServerSocketRead(int socketFd, char *msgBuff)
{
    int cnt;
    int i;
    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));
    cnt = read(socketFd, &frame, sizeof(frame));

    if (cnt < 0)
    {
        LogMsg(LOG_INFO, "%s(): recv() failed, client closed\n", __FUNCTION__);
        close(socketFd);
        return -1;
    }
    else
    {
        strncpy(msgBuff, (char *)frame.data, 7);
        cnt = frame.can_dlc;
        msgBuff[cnt] = '\0';
        LogMsg(LOG_INFO, "%s: buff = %s", __FUNCTION__, msgBuff);
        return cnt;
    }

}


void canServerSocketWrite(int socketFd, const char *buff)
{
    int cnt = strlen(buff);
    if (cnt > 7)
        cnt = 7;
    int i = 0;
    struct can_frame frame;

    memset(&frame, 0, sizeof(frame));

    frame.can_id = 0;
    strncpy((char *)frame.data, buff, cnt);
    frame.can_dlc = cnt;

    if (write(socketFd, &frame, sizeof(frame)) < 0) {
        LogMsg(LOG_ERR, "CAN BUS: write() failed, %d\n",
            socketFd);
        perror("what's messed up?");
    }
    else
    {
        LogMsg(LOG_INFO, "%s: sent = %s", __FUNCTION__, frame.data);
    }
}


