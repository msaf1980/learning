#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <daemonutils.h>
#include <logutils/syslogutils.h>
#include <netutils/netutils.h>
#include <strutils.h>

/* #define BACKLOG 20 */
#define BACKLOG SOMAXCONN

#define RESPSIZE 80
#define REQSIZE  22


int running = 1;

struct config {
    char *ip;
    int port;
};

enum SESS_STATE {SESS_INIT = 0, SESS_CLIENT = 1, SESS_STAR = 2, SESS_ERR = -1 };

void *root_logger;

int session_start(int sess_fd, const char *ip, const u_short port,
                   char *buf) {
    size_t n = recv(sess_fd, buf, 22, MSG_NOSIGNAL);
    if (n > 0)
        buf[n] = '\0';
    if (n != REQSIZE) {
        if (n < 0) {
            _LOG_ERROR_ERRNO(root_logger,
                             "recv on client connection from %s:%d: %s", errno,
                             ip, port);
        } else {
            _LOG_ERROR(root_logger, "length error for %s:%d: %s", ip, port, buf);
        }
        return SESS_ERR;
    }
}

int server_session(int sess_fd, const char *ip, const u_short port,
                   const struct config *conf) {
    char buf[RESPSIZE+1];
    int status = SESS_INIT;

    while (running) {
        if (status == SESS_INIT) {
            status = session_start(sess_fd, ip, port, buf);
            if (status == SESS_CLIENT) {

            } else if (status == SESS_STAR) {

            } else
                break;
        }
    }

    /*
    strcpy(buf, "Hi there!\n");
    ssize_t len = strlen(buf);
    for (int i = 0; i < 5; i++) {
        if (send(sess_fd, buf, len, MSG_NOSIGNAL) == -1) {
            _LOG_ERROR_ERRNO(root_logger,
                             "send on client connection from %s:%d: %s", errno,
                             ip, port);
            break;
        }
    }
    */
    /* _LOG_INFO(root_logger, "close client connection from %s:%d", ip, port);
     */
    close(sess_fd);
}

int listener_loop(int srv_fd, const struct config *conf) {
    int ec = 0;
    SA_IN client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char ipbuf[INET_ADDRSTRLEN];
    while (running) {
        int sess_fd = accept(srv_fd, (SA *)&client_addr, &client_addr_len);
        if (sess_fd == -1) {
            if (errno != EINTR)
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
        server_session(sess_fd, ipbuf, client_addr.sin_port, conf);
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
        ec = -1;
        _LOG_ERROR_ERRNO(root_logger, "%s: %s", errno, "socket");
        goto EXIT;
    }
    /*
    set_reuseaddr(srv_fd);
    */

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(conf->port);
    if (conf->ip == NULL)
        srv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* List on any IP */
    else if (inet_aton(conf->ip, &srv_addr.sin_addr) == 0) {
        ec = -1;
        _LOG_ERROR(root_logger, "invalid address: %s", conf->ip);
        goto EXIT;
    }

    if (bind(srv_fd, (SA *)&srv_addr, sizeof(srv_addr)) == -1) {
        ec = -1;
        _LOG_ERROR_ERRNO(root_logger, "%s: %s", errno, "bind");
        goto EXIT;
    }

    /* set_nonblock(srv_fd); */

    if (listen(srv_fd, BACKLOG) == -1) {
        ec = -1;
        _LOG_ERROR_ERRNO(root_logger, "%s: %s", errno, "listen");
        goto EXIT;
    }

    _LOG_NOTICE(root_logger, "%s", "startup");

    ec = listener_loop(srv_fd, conf);

EXIT:
    if (ec)
        _LOG_NOTICE(root_logger, "%s", "shutdown with error");
    return ec;
}

void app_shutdown() {
    running = 0;
    sleep(1);
    exit(0);
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

    return ec;
}

void usage(const char *name) {
    fprintf(stderr, "use: %s [options]\n%s", name,
            "\t-b | --background Fork and run in background\n"
            "\t-a | --address <LISTEN_ADDRESS> (default all)\n"
            "\t-p | --port <LISTEN_PORT> (default 1234)\n");
    exit(1);
}

int main(int argc, char *const argv[]) {
    int ec = 0, pid;
    int background = 0;
    int closefds = FD_NOCLOSE;
    const char *name = "astrosrv";
    struct config conf;
    conf.ip = NULL;
    conf.port = 1234;

    int opt = 0;
    int opt_idx = 0;

    const char *opts = "hba:p:";
    const struct option long_opts[] = {
        /* Use flags like so:
        {"verbose",	no_argument,	&verbose_flag, 'V'}*/
        /* Argument styles: no_argument, required_argument, optional_argument */
        {"help", no_argument, 0, 'h'},
        {"background", no_argument, 0, 'b'},
        {"address", required_argument, 0, 'a'},
        {"port", required_argument, 0, 'p'},
        {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, opts, long_opts, &opt_idx)) != -1) {
        switch (opt) {
        case 'h':
            usage(argv[0]);
            break;
        case 'b':
            background = 1;
            break;
        case 'a':
            conf.ip = optarg;
            break;
        case 'p':
            conf.port = atoi(optarg);
            if (conf.port <= 0) {
                fprintf(stderr, "invalid port: %s\n", optarg);
                return EXIT_FAILURE;
            }
            break;
        case 0: /* binded option, set by getopt */
            break;
        case '?':
            /* getopt_long will have already printed an error */
            return -1;
        default:
            /* Not sure how to get here... */
            return -1;
        }
    }
    if (optind < argc) {
        fprintf(stderr, "Non-option arguments: ");
        while (optind < argc)
            fprintf(stderr, "%s ", argv[optind++]);
        fprintf(stderr, "\n");
        return EXIT_FAILURE;
    }

    if (sig_handlers_init()) {
        ec = 1;
        goto EXIT;
    }

    openlog(name, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);

    if (background)
        closefds = FD_CLOSE_STD;

    pid = daemon_init(background, 1, closefds);
    if (pid == 0) { /* child process */
        ec = start_server(&conf);
    } else if (pid < 0) {
        _LOG_ERROR_ERRNO(root_logger, "%s: %s", errno, "fork");
        ec = EXIT_FAILURE;
    } else { /* parent, check child status */
        int wstatus;
        if (waitpid(pid, &wstatus, WNOHANG) != 0) {
            ec = EXIT_FAILURE;
            perror("check forked process");
        } else {
            if (WIFEXITED(wstatus) || WIFSIGNALED(wstatus)) {
                ec = EXIT_FAILURE;
            }
        }
    }
EXIT:
    if (ec != 0) {
        fprintf(stderr, "exit with error, check log\n");
    }
    return ec;
}
