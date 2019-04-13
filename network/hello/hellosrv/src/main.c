#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <daemonutils.h>
#include <logutils/syslogutils.h>
#include <netutils/netutils.h>

/* #define BACKLOG 20 */
#define BACKLOG SOMAXCONN

#define BUFSIZE 4096

int running = 1;

int connected = 0;

struct config {
    char *ip;
    int port;
    int max_connect;
};

int server_session(int sess_fd, const char *ip, const u_short port) {
    char buf[BUFSIZE];
    strcpy(buf, "Hi there!\n");
    ssize_t len = strlen(buf);
    for (int i = 0; i < 5; i++) {
        if (send(sess_fd, buf, len, MSG_NOSIGNAL) == -1) {
            _LOG_ERROR_ERRNO(root_logger,
                             "send on client connection from %s:%d: %s", errno,
                             ip, port);
            break;
        }
        sleep(1);
    }
    /* _LOG_INFO(root_logger, "close client connection from %s:%d", ip, port);
     */
    close(sess_fd);
}

int loop_fork(int srv_fd, const struct config *conf) {
    int ec = 0;
    SA_IN client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char ipbuf[INET_ADDRSTRLEN];
    while (running) {
        int sess_fd = accept(srv_fd, (SA *)&client_addr, &client_addr_len);
        if (sess_fd == -1) {
            _LOG_ERROR_ERRNO(root_logger, "%s on socket %d: %s", errno,
                             "accept", srv_fd);
            continue;
        }

        /* Format client IP address */
        if (getnameinfo((SA *)&client_addr, client_addr_len, ipbuf,
                        INET_ADDRSTRLEN, 0, 0, NI_NUMERICHOST) == 0) {
            _LOG_INFO(root_logger, "connect from %s:%d", ipbuf,
                      client_addr.sin_port);
        } else {
            _LOG_ERROR(root_logger, "invalid address for socket %d", sess_fd);
        }
        if (connected >= conf->max_connect) {
            const char *buf = "Too many connections\n";
            send(sess_fd, buf, strlen(buf), MSG_NOSIGNAL);
            _LOG_ERROR(root_logger, "%s", "too many connections");
            continue;
        }

        pid_t pid = fork();
        if (pid == -1) {
            _LOG_ERROR_ERRNO(root_logger,
                             "fork on client connection from %s:%d: %s", errno,
                             ipbuf, client_addr.sin_port);
        } else if (pid == 0) {
            /* child process */
            close(srv_fd);
            server_session(sess_fd, ipbuf, client_addr.sin_port);
            exit(0);
        }
        connected++;
        close(sess_fd);
    }
EXIT:
    close(srv_fd);
    return ec;
}

int start_server(const struct config *conf) {
    int ec = 0;
    int srv_fd; /* server socket */
    SA_IN srv_addr;

    if ((srv_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        ec = EXIT_FAILURE;
        _LOG_ERROR_ERRNO(root_logger, "%s: %s", errno, "socket");
        goto EXIT;
    }
    set_reuseaddr(srv_fd);

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(conf->port);
    if (conf->ip == NULL)
        srv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* List on any IP */
    else if (inet_aton(conf->ip, &srv_addr.sin_addr) == 0) {
        ec = EXIT_FAILURE;
        _LOG_ERROR(root_logger, "invalid address: %s", conf->ip);
        goto EXIT;
    }

    if (bind(srv_fd, (SA *)&srv_addr, sizeof(srv_addr)) == -1) {
        ec = EXIT_FAILURE;
        _LOG_ERROR_ERRNO(root_logger, "%s: %", errno, "bind");
        goto EXIT;
    }

    /* set_nonblock(srv_fd); */

    if (listen(srv_fd, BACKLOG) == -1) {
        ec = EXIT_FAILURE;
        _LOG_ERROR_ERRNO(root_logger, "%s: %", errno, "listen");
        goto EXIT;
    }

    _LOG_NOTICE(root_logger, "%s", "startup");

    ec = loop_fork(srv_fd, conf);

EXIT:
    _LOG_NOTICE(root_logger, "%s", "shutdown");
    return ec;
}

void app_shutdown() {
    running = 0;
    _LOG_NOTICE(root_logger, "%s", "shutdown initiate");
    sleep(10);
    _LOG_NOTICE(root_logger, "%s", "shutdown");
    exit(0);
}

void handle_sigchld() {
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {
        if (connected > 0)
            connected--;
    }
}

void sig_handler(int sig) {
    int saved_errno = errno;
    switch (sig) {
    case SIGHUP:
        _LOG_INFO(root_logger, "%s", "received SIGHUP signal");
        break;
    case SIGUSR1:
        _LOG_INFO(root_logger, "%s", "received SIGUSR1 signal");
        break;
    case SIGINT:
    case SIGTERM:
        app_shutdown();
        break;
    case SIGCHLD:
        /* _LOG_INFO(root_logger, "%s", "received SIGCHLD signal"); */
        handle_sigchld();
        break;
    }
    errno = saved_errno;
}

int sig_handlers_init() {
    int ec = 0;
    /* Handle signals */
    struct sigaction sa;

    /* Setup the signal handler */
    sa.sa_handler = &sig_handler;

    /* Restart the system call, if at all possible */
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

    /* sa.sa_flags = 0; */

    /* Block every signal during the handler */
    sigfillset(&sa.sa_mask);

    /* Ignore SIGPIPE */
    if (sigaction(SIGPIPE, &(struct sigaction){SIG_IGN}, NULL) == -1) {
        perror("Error: cannot handle SIGPIPE");
        ec = 1;
    }

    /* SIGTERM is intended to gracefull kill your process */
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        perror("Cannot handle SIGTERM");
        ec = 1;
    }

    /* Intercept SIGINT */
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error: cannot handle SIGINT");
        ec = 1;
    }

    // Intercept SIGHUP
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("Error: cannot handle SIGHUP");
        ec = 1;
    }

    if (sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("Error: cannot handle SIGUSR1");
        ec = 1;
    }

    /* SIGCHLD for cleanup zombie child processes */
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
            perror("Cannot handle SIGCHLD");
            ec = 1;
    }

    /* Ignore SIGICHLD for auto-cleanup zombie child processes, if don't need custom handler */
    /*
    if (sigaction(SIGCHLD, &(struct sigaction){SIG_IGN}, NULL) == -1) {
        perror("Error: cannot handle SIGCHLD");
        ec = 1;
    }
    */

    return ec;
}

int main(int argc, char *const argv[]) {
    int ec = 0, pid;
    int background = 0;
    int closefd = FD_NOCLOSE;
    const char *name = "hellosrv";
    struct config conf;
    conf.ip = NULL;
    conf.port = 1234;
    conf.max_connect = 1000;

    if (sig_handlers_init()) {
        ec = 1;
        goto EXIT;
    }

    openlog(name, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);

    if (background)
        closefd = FD_CLOSE_STD;

    pid = daemon_init(background, 1, closefd);
    if (pid == 0) { /* child process */
        ec = start_server(&conf);
    } else if (pid < 0) {
        _LOG_ERROR_ERRNO(root_logger, "%s: %s", errno, "fork");
        ec = EXIT_FAILURE;
    }
EXIT:
    return ec;
}
