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
    LogMsg(LOG_ERR, "[CAN] exiting: %s\n", errorMessage);
    exit(1);
}

/**
 * Initializes a socket for read and writes to the CAN bus
 * port.
 *
 * @param canPort the CAN bus interface to bind to.
 *
 */

int canBusSocketInit(int canPort)
{
    int sock = -1;
    int rv = 0;
    struct sockaddr_can sAddr;
    struct ifreq ifr;

    sock = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sock < 0)
    {
        canDieWithError("[CAN] can socket() failed");
    }

    memset(&ifr, 0, sizeof(ifr));
    sprintf(ifr.ifr_name, "can%d", canPort);


    rv = ioctl(sock, SIOGIFINDEX, &ifr);
    if (rv < 0)
    {
        canDieWithError("[CAN] Error: ioctl(SIOGIFINDEX) failed CAN BUS may not be up.");
    }

    memset(&sAddr, 0, sizeof(sAddr));
    sAddr.can_family = AF_CAN;
    sAddr.can_ifindex = ifr.ifr_ifindex;

    rv = bind(sock, (struct sockaddr *)&sAddr, sizeof(sAddr));


    if (rv < 0)
        canDieWithError("[CAN] Error: CAN bind() failed.");

    LogMsg(LOG_INFO, "[CAN] handling CAN bus client\n");

    return sock;
}


/**
 * Reads a single message from the socket connected to the 
 * can bus port. If no message is ready to be received, the call
 * will block until one is available. 
 * 
 * @param socketFd the file descriptor for the already open
 *                 socket connecting to the CAN bus
 * @param msgBuff address of a contiguous array into which the 
 *                message will be written upon receipt
 *                from CAN bus
 *
 * @return int 0 if no message to return (handled here), -1 if 
 *         recv() returned an error code (close connection) or
 *         >0 to indicate msgBuff has that many characters
 *         filled in
 */
int canBusSocketRead(int socketFd, char *msgBuff)
{
    int cnt;
    struct can_frame frame;
    memset(&frame, 0, sizeof(frame));
    cnt = read(socketFd, &frame, sizeof(frame));

    if (cnt < 0)
    {
        LogMsg(LOG_INFO, "[CAN] %s(): recv() failed, client closed\n", __FUNCTION__);
        close(socketFd);
        return -1;
    }
    else
    {
        strncpy(msgBuff, (char *)frame.data, 8);
        cnt = frame.can_dlc;
        msgBuff[cnt] = '\0';
        LogMsg(LOG_INFO, "[CAN] received = %s", msgBuff);
        return cnt;
    }

}

/**
 * Writes a single message to the socket connected to the
 * CAN bus port.
 *
 * @param socketFd the file descriptor for the already open
 *                 socket connecting to the CAN bus
 * @param msgBuff  address of a contiguous array that contains
 *                 the message.
 *
 */
void canBusSocketWrite(int socketFd, const char *msgBuff)
{
    struct can_frame frame;
    int cnt = strlen(msgBuff);
    if (cnt > 8)
        cnt = 8;

    memset(&frame, 0, sizeof(frame));
    frame.can_id = 0;
    strncpy((char *)frame.data, msgBuff, cnt);
    frame.can_dlc = cnt;

    if (write(socketFd, &frame, sizeof(frame)) < 0) {
        LogMsg(LOG_ERR, "[CAN] CAN BUS: write() failed, %d\n",
            socketFd);
        perror("what's messed up?");
    }
    else
    {
        LogMsg(LOG_INFO, "[CAN] sent = %s", frame.data);
    }
}


