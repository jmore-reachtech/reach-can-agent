#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "can_agent.h"

/* module-wide "global" variables */
static int keepGoing;
static const char *progName;

static void canDumpHelp();
static void canAgent(unsigned short tcpPort, int baudRate, const char *unixSocketPath);
static inline int max(int a, int b) { return (a > b) ? a : b; }
static ethIf_t * network_open(uint8_t instance, int baudRate);
static int network_close(ethIf_t *ep);
static int execute_cmd_ex(const char *cmd, char *result, int result_size);


int main(int argc, char *argv[])
{
    int daemonFlag = 0;
    unsigned short canPort = 0;
    int baudRate = 0;
    const char *logFilePath = 0;
    /*
     * syslog isn't installed on the target so it's disabled in this program
     * by requiring an argument to -o|--log.
     */
    int logToSyslog = 0;
    int verboseFlag = 0;

    /* allocate memory for progName since basename() modifies it */
    const size_t nameLen = strlen(argv[0]) + 1;
    char arg0[nameLen];
    memcpy(arg0, argv[0], nameLen);
    progName = basename(arg0);

    while (1) {
        static struct option longOptions[] = {
            { "daemon",      no_argument,       0, 'd' },
            { "log",         required_argument, 0, 'o' },
            { "can_port",    required_argument, 0, 'c' },
            { "baudrate",    required_argument, 0, 'b' },
            { "verbose",     no_argument,       0, 'v' },
            { "help",        no_argument,       0, 'h' },
            { 0,             0, 0,  0  }
        };
        int c = getopt_long(argc, argv, "d:o:c:b:vh?", longOptions, 0);

        if (c == -1) {
            break;  // no more options to process
        }

        switch (c) {
        case 'd':
            daemonFlag = 1;
            break;

        case 'o':
            if (optarg == 0) {
                logToSyslog = 1;
                logFilePath = 0;
            } else {
                logToSyslog = 0;
                logFilePath = optarg;
            }
            break;
        case 'c':
            canPort = (optarg == 0) ? CAN_DEFAULT_SERVER_AGENT_PORT : atoi(optarg);
            break;
        case 'b':
            baudRate = (optarg == 0) ? CAN_BAUD_RATE : atoi(optarg);
            break;

        case 'v':
            verboseFlag = 1;
            break;

        case '?':
        case 'h':
        default:
            canDumpHelp();
            exit(1);
        }
    }

    /* set up logging to syslog or file; will be STDERR not told otherwise */
    LogOpen(progName, logToSyslog, logFilePath, verboseFlag);

    if (daemonFlag) {
        daemon(0, 1);
    }

    canAgent(canPort, baudRate, CAN_AGENT_UNIX_SOCKET);

    return 0;
}

static void canDumpHelp()
{
    fprintf(stderr, "CAN Agent %s \n\n", CAN_VERSION);

    fprintf(stderr, "usage: %s [options]\n"
            "  where options are:\n"
            "    -d             | --daemon            run in background\n"
            "    -o<path>       | --logfile=<path>    log to file instead of stderr\n"
            "    -c[<port>]     | --can_port[=<port>] CAN bus port 0, 1 ,2 \n"
            "    -b<baudrate>   | --baudrate          baudrate of CAN bus \n"
            "    -v             | --verbose           print progress messages\n"
            "    -h             | -? | --help         print usage information\n",
            progName, CAN_DEFAULT_SERVER_AGENT_PORT);
}

static void canInterruptHandler(int sig)
{
    keepGoing = 0;
}

/**
 * This is the main loop function.  It opens and configures the
 * CAN Bus Server port and opens the TIO socket using a Unix
 * domain and enters a select loop waiting for connections.
 *
 * @param canPort the port number to open for
 *        accepting connections from the CAN Bus 0 for can0 1 for can1 ect;
 *
 * @param unixSocketPath the file system path to use for a Unix domain socket;
 */
static void canAgent(unsigned short canPort, int baudRate, const char *unixSocketPath)
{
    int n;
    ethIf_t *ep = NULL;
    fd_set currFdSet;
    FD_ZERO(&currFdSet);

    /********************************* Open CAN BUS network ********************************/
    ep = network_open(canPort, baudRate);
    if (ep == NULL)
    {
        LogMsg(LOG_ERR, "Error: %s: network_open() failed: %s [%d]\n", __FUNCTION__, strerror(errno), errno);
        exit(1);
    }

    /********************************** Set up TIO Socket ***********************************/
    int connectedTIOFd = -1;  /* not currently connected */

    {
        /* install a signal handler to remove the socket file */
        struct sigaction a;
        memset(&a, 0, sizeof(a));
        a.sa_handler = canInterruptHandler;
        if (sigaction(SIGINT, &a, 0) != 0) {
            LogMsg(LOG_ERR, "sigaction() failed, errno = %d\n", errno);
            exit(1);
        }
    }

    /* open the tio socket */
    int addressTIOFamily = 0;
    const int listenTIOFd = canTioSocketInit(&addressTIOFamily,
                                             unixSocketPath);
    if (listenTIOFd < 0) {
        /* open failed, can't continue */
        LogMsg(LOG_ERR, "could not open tio socket\n");
        return;
    }
    else
    {
        LogMsg(LOG_INFO, "TIO Unix Socket Open\n");
    }

    FD_SET(listenTIOFd, &currFdSet);

    /********************************** Set up CAN Bus Socket ***********************************/
    int connectedServerFd = -1;  /* not currently connected */

    /* open the server socket */
    connectedServerFd = canServerSocketInit(canPort);
    if (connectedServerFd < 0) {
        /* open failed, can't continue */
        LogMsg(LOG_ERR, "could not open CAN Bus socket\n");
        return;
    }

    FD_SET(connectedServerFd, &currFdSet);

    n = max(connectedServerFd, listenTIOFd) + 1;

    /* execution remains in this loop until a fatal error or SIGINT */
    keepGoing = 1;

    while (keepGoing) {
        /*
         * This is the select loop which waits for characters to be received on
         * the CAN bus and on either the listen socket (meaning
         * an incoming connection is queued) or on a connected socket
         * descriptor.
         */

        /* check for packet received on the server socket or tio socket */
        //wait for a message

        fd_set readFdSet = currFdSet;
        const int sel = select(n, &readFdSet, 0, 0, 0);

        if (sel == -1) {
            if (errno == EINTR) {
                break;  /* drop out of while */
            } else {
                LogMsg(LOG_ERR, "select() returned -1, errno = %d\n", errno);
                exit(1);
            }
        } else if (sel <= 0) {
            continue;
        }


        if (FD_ISSET(connectedServerFd, &readFdSet)) {
            // read CAN frames
            char msgBuff[128];
            const int readCount = canServerSocketRead(connectedServerFd, msgBuff);
            if (readCount > 0) {
                if (connectedTIOFd >= 0)
                {
                    canTioSocketWrite(connectedTIOFd, msgBuff);
                    n = max(connectedTIOFd, connectedServerFd)  + 1;
                }
            }
        }

        /* check for a new tio connection to accept */
        if (FD_ISSET(listenTIOFd, &readFdSet)) {
            /* new connection is here, accept it */
            connectedTIOFd = canTioSocketAccept(listenTIOFd, addressTIOFamily);
            if (connectedTIOFd >= 0) {
                FD_CLR(listenTIOFd, &currFdSet);
                FD_SET(connectedTIOFd, &currFdSet);
                n = max(connectedTIOFd, connectedServerFd)  + 1;
            }
        }

        /* check for packet received on the tio socket */
        if ((connectedTIOFd >= 0) && FD_ISSET(connectedTIOFd, &readFdSet)) {
            /* connected tio_agent has something to relay to can bus */
            char msgBuff[128];
            const int readCount = canTioSocketRead(connectedTIOFd, msgBuff,
                                                   sizeof(msgBuff));
            if (readCount < 0) {
                FD_CLR(connectedTIOFd, &currFdSet);
                FD_SET(listenTIOFd, &currFdSet);
                n = max(listenTIOFd, connectedServerFd) + 1;
                connectedTIOFd = -1;
            } else if (readCount > 0) {
                if (connectedServerFd >= 0)
                    canServerSocketWrite(connectedServerFd, msgBuff);
            }
        }

    } /* end while */

    LogMsg(LOG_INFO, "cleaning up\n");

    if (connectedTIOFd >= 0) {
        close(connectedTIOFd);
    }
    if (listenTIOFd >= 0) {
        close(listenTIOFd);
    }

    if (connectedServerFd >= 0) {
        close(connectedServerFd);
    }

    /* best effort removal of socket */
    const int rv = unlink(unixSocketPath);
    if (rv == 0) {
        LogMsg(LOG_INFO, "socket file %s unlinked\n", unixSocketPath);
    } else {
        LogMsg(LOG_INFO, "socket file %s unlink failed\n", unixSocketPath);
    }

    if (network_close(ep) < 0)
    {
        LogMsg(LOG_INFO, "network close failed.\n");
    }

}

/****************************************************************************
 * network_open
 */
static ethIf_t * network_open(uint8_t instance, int baudRate)
{
    ethIf_t *ep = NULL;
    char if_name[32];
    char cmd[128];
    int rv = 0;
    int n;

    *if_name = '\0';
    sprintf(if_name, "can%d", instance);

    n = sizeof(*ep);
    if ((ep = malloc(n)) == NULL)
    {
        LogMsg(LOG_ERR, "Error: %s: malloc(%d) failed: %s [%d]\n", __FUNCTION__, n, strerror(errno), errno);
        exit(1);
    }
    memset(ep, 0, n);

    strcpy(ep->if_name, if_name);


    sprintf(cmd, "modprobe flexcan");
    rv = execute_cmd_ex(cmd, NULL, 0);
    if (rv < 0)
    {
        LogMsg(LOG_ERR, "Error: %s: execute_cmd('%s') failed: %s [%d]\n", __FUNCTION__, cmd, strerror(errno), errno);
        exit(1);
    }
    LogMsg(LOG_INFO, "cmd run: modprobe flexcan\n");

    // Load can config
    sprintf(cmd, "canconfig %s bitrate %d", if_name, baudRate);
    rv = execute_cmd_ex(cmd, NULL, 0);
    if (rv < 0)
    {
        LogMsg(LOG_ERR, "Error: %s: execute_cmd('%s') failed: %s [%d]\n", __FUNCTION__, cmd, strerror(errno), errno);
        exit(1);
    }
    LogMsg(LOG_INFO, "cmd run: %s\n", cmd);

    ep->flags |= _NET_CAN_LOADED;

    sprintf(cmd, "ifconfig %s up", if_name);
    rv = execute_cmd_ex(cmd, NULL, 0);
    if (rv < 0)
    {
        fprintf(stderr, "Error: %s: execute_cmd('%s') failed: %s [%d]\n", __FUNCTION__, cmd, strerror(errno), errno);
        exit(1);
    }
    LogMsg(LOG_INFO, "cmd run: ifconfig %s up\n", if_name);

    ep->flags |= _NET_INTERFACE_UP;

    return ep;
}

/****************************************************************************
 * network_close
 */
static int network_close(ethIf_t *ep)
{
    int status = 0;
    char cmd[128];
    int rv = 0;

    if (ep != NULL)
    {
        if (ep->flags & _NET_INTERFACE_UP)
        {
            ep->flags &= ~_NET_INTERFACE_UP;

            sprintf(cmd, "ifconfig %s down", ep->if_name);
            rv = execute_cmd_ex(cmd, NULL, 0);
            if (rv < 0)
            {
                LogMsg(LOG_ERR, "Error: %s: execute_cmd('%s') failed: %s [%d]\n", __FUNCTION__, cmd, strerror(errno), errno);
                status = rv;
            }
        }
        LogMsg(LOG_INFO, "cmd run: ifconfig %s down.\n", ep->if_name);

        if (ep->flags & _NET_CAN_LOADED)
        {
            ep->flags &= ~_NET_CAN_LOADED;
        }

        free(ep);
        ep = NULL;
    }

    return status;
}


/****************************************************************************
 * execute_cmd_ex
 */
static int execute_cmd_ex(const char *cmd, char *result, int result_size)
{
    int status = 0;
    FILE *fp = NULL;
    char *icmd = NULL;
    int n;


    if ((result != NULL) && (result_size > 0))
    {
        *result = '\0';
    }

    n = strlen(cmd) + 16;
    if ((icmd = malloc(n)) == NULL)
    {
        fprintf(stderr, "Error: %s: malloc(%d) failed: %s [%d]\n", __FUNCTION__, n, strerror(errno), errno);
        status = -1;
        goto e_execute_cmd_ex;
    }
    strcpy(icmd, cmd);
    strcat(icmd, " 2>&1");

    if ((fp = popen(icmd, "r")) != NULL)
    {
        char buf[256];
        int x;
        int p_status;

        buf[0] = '\0';

        while (fgets(buf, sizeof(buf), fp) != NULL)
        {
            // Strip trailing whitespace
            for (x = strlen(buf); (x > 0) && isspace(buf[x - 1]); x--); buf[x] = '\0';
            if (buf[0] == '\0') continue;

            if (result != NULL)
            {
                if ((strlen(result) + strlen(buf) + 4) < result_size)
                {
                    strcat(result, buf);
                    strcat(result, "\n");
                }
                else
                {
                    strcat(result, "...\n");
                    result = NULL;
                }
            }

        }

        p_status = pclose(fp);
        if (p_status == -1)
        {
            fprintf(stderr, "Error: %s: pclose() failed: %s [%d]\n", __FUNCTION__, strerror(errno), errno);
            status = -1;
            goto e_execute_cmd_ex;
        }
        else if (WIFEXITED(p_status))
        {
            int e_status;

            e_status = (int8_t) WEXITSTATUS(p_status);

            if (e_status != 0)
            {
                status = -1;
            }
        }
        else if (WIFSIGNALED(p_status))
        {
            int sig;

            sig = WTERMSIG(p_status);
            fprintf(stderr, "Error: %s: Command '%s' was killed with signal %d\n", __FUNCTION__, cmd, sig);
            status = -1;
        }
        else
        {
            fprintf(stderr, "Error: %s: Command '%s' exited for unknown reason\n", __FUNCTION__, cmd);
            status = -1;
        }

        fp = NULL;
    }
    else
    {
        fprintf(stderr, "Error: %s: popen('%s') failed: %s [%d]\n", __FUNCTION__, icmd, strerror(errno), errno);
        status = -1;
        goto e_execute_cmd_ex;
    }

e_execute_cmd_ex:
    if (icmd != NULL)
    {
        free(icmd);
        icmd = NULL;
    }

    return status;
}

