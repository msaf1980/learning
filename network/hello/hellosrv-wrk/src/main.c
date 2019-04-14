#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
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
#include <strutils.h>

struct config {
    char *ip;
    int port;
    /* long int max_connect; */ /* max connections */
    unsigned int delay;
    int workers;
};

/* #define BACKLOG 20 */
#define BACKLOG SOMAXCONN

#define BUFSIZE 4096

int running = 1;

int workers = 0;     /* number of workers */
int worker_died = 0; /* set to 1 in signal handler if worker died */

pid_t *wpids = NULL;

struct config conf;

int server_session(int sess_fd, const char *ip, const u_short port,
                   const struct config *conf) {
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
        sleep(conf->delay);
    }
    /* _LOG_INFO(root_logger, "close client connection from %s:%d", ip, port);
     */
    close(sess_fd);
}

pid_t loop_child(int srv_fd, const struct config *conf) {
    pid_t pid = fork();
    if (pid == 0) {
        int ec = 0;
        SA_IN client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        char ipbuf[INET_ADDRSTRLEN];
        workers = -1; /* set to -1 in worker */
        while (running) {
            int sess_fd = accept(srv_fd, (SA *) &client_addr, &client_addr_len);
            if (sess_fd == -1) {
                if (errno != EINTR)
                    _LOG_ERROR_ERRNO(root_logger, "%s on socket %d: %s", errno,
                                     "accept", srv_fd);
                continue;
            }
            if (running == 0)
                break;

            /* Format client IP address */
            if (getnameinfo((SA *) &client_addr, client_addr_len, ipbuf,
                            INET_ADDRSTRLEN, 0, 0, NI_NUMERICHOST) == 0) {
                _LOG_INFO(root_logger, "connect from %s:%d", ipbuf,
                          client_addr.sin_port);
            } else {
                _LOG_ERROR(root_logger, "invalid address for socket %d",
                           sess_fd);
            }
            /*
            if (connected >= conf->max_connect) {
                const char *buf = "Too many connections\n";
                send(sess_fd, buf, strlen(buf), MSG_NOSIGNAL);
                close(sess_fd);
                _LOG_ERROR(root_logger, "%s", "too many connections");
                continue;
            }
            */

            server_session(sess_fd, ipbuf, client_addr.sin_port, conf);
        }
        exit(0);
    } else if (pid < 0) {
        _LOG_ERROR_ERRNO(root_logger, "%s: %s", errno, "fork worker");
    }
    return pid;
}

int start_server(const struct config *conf) {
    int ec = 0;
    int srv_fd; /* server socket */
    SA_IN srv_addr;
    int reuse = 1;
    int status;
    pid_t wpid;
    wpids = (pid_t *) calloc(sizeof(pid_t), conf->workers);
    if (wpids == NULL) {
        _LOG_ERROR(root_logger, "%s", "alloc workers");
        return -1;
    }

    if ((srv_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        ec = -1;
        _LOG_ERROR_ERRNO(root_logger, "%s: %s", errno, "socket");
        goto EXIT;
    }
    set_reuseaddr(srv_fd);

    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(conf->port);
    if (conf->ip == NULL)
        srv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* List on any IP */
    else if (inet_aton(conf->ip, &srv_addr.sin_addr) == 0) {
        ec = -1;
        _LOG_ERROR(root_logger, "invalid address: %s", conf->ip);
        goto EXIT;
    }

    if (bind(srv_fd, (SA *) &srv_addr, sizeof(srv_addr)) == -1) {
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

    /* run workers */
    if (workers < conf->workers) {
        for (int i = 0; i < conf->workers; i++) {
            wpids[i] = loop_child(srv_fd, conf);
            if (wpids[i] < 0) {
                ec = -1;
                running = 0;
                break;
            }
            workers++;
        }
    }

    /* respawn died workers */
    while (running) {
        if (worker_died) {
            if (running) {
                _LOG_WARN(root_logger, "%s", "worker died");
            }
            worker_died = 0;
            for (int i = 0; i < conf->workers; i++) {
                if (wpids[i] <= 0) {
                    if (running == 0) {
                        break;
                    }
                    wpids[i] = loop_child(srv_fd, conf);
                    if (wpids[i] < 0) {
                        ec = -1;
                        workers--;
                        _LOG_INFO(root_logger, "%s", "worker start faled");
                        if (workers <= 0)
                            break;
                    } else {
                        _LOG_INFO(root_logger, "%s", "worker started");
                        workers++;
                    }
                }
            }
        } else {
            sleep(1);
        }
    }
EXIT:
    while (wait(&status) > 0) {
    }
    if (srv_fd)
        close(srv_fd);
    free(wpids);
    if (ec) {
        _LOG_NOTICE(root_logger, "%s", "shutdown with error");
    } else {
        _LOG_NOTICE(root_logger, "%s", "shutdown");
    }
    return ec;
}

void app_shutdown() {
    running = 0;
    if (workers >= 0) {
        _LOG_NOTICE(root_logger, "%s", "shutdown initiate");
        for (int i = 0; i < conf.workers; i++) {
            if (wpids[i] > 0) {
                kill(wpids[i], SIGTERM);
            }
        }
    } else {
        /* exit worker */
        sleep(1);
        exit(0);
    }
}

void handle_sigchld() {
    pid_t pid;
    int wstatus;
    while ((pid = waitpid((pid_t)(-1), &wstatus, WNOHANG)) > 0) {
        if (pid > 0) {
            for (int i = 0; i < conf.workers; i++) {
                if (wpids[i] == pid) {
                    wpids[i] = 0;
                }
            }
            worker_died = 1;
            if (WIFEXITED(wstatus)) {
                _LOG_ERROR(root_logger, "worker exited with status %d",
                           WEXITSTATUS(wstatus));
            } else if (WIFSIGNALED(wstatus)) {
                _LOG_ERROR(root_logger, "worker killed with signal '%s'",
                           strsignal(WTERMSIG(wstatus)));
            }
        }
    }
    workers--;
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

    /* Ignore SIGICHLD for auto-cleanup zombie child processes, if don't need
     * custom handler */
    /*
    if (sigaction(SIGCHLD, &(struct sigaction){SIG_IGN}, NULL) == -1) {
        perror("Error: cannot handle SIGCHLD");
        ec = 1;
    }
    */

    return ec;
}

void usage(const char *name) {
    fprintf(stderr, "use: %s [options]\n%s", name,
            "\t-b | --background Fork and run in background\n"
            "\t-a | --address <LISTEN_ADDRESS> (default all)\n"
            "\t-p | --port <LISTEN_PORT> (default 1234)\n"
            "\t-d | --delay <DELAY> (default 0)\n"
            "\t-w | --workers <WORKERS> (default 2)\n");
    exit(1);
}

int main(int argc, char *const argv[]) {
    int ec = 0, pid;
    int background = 0;
    int closefds = FD_NOCLOSE;
    const char *name = "hellosrv";
    conf.ip = NULL;
    conf.port = 1234;
    conf.workers = 2;
    /* conf.max_connect = INT_MAX; */
    conf.delay = 0;

    int opt = 0;
    int opt_idx = 0;

    const char *opts = "hba:p:w:d:";
    const struct option long_opts[] = {
        /* Use flags like so:
        {"verbose",	no_argument,	&verbose_flag, 'V'}*/
        /* Argument styles: no_argument, required_argument, optional_argument */
        {"help", no_argument, 0, 'h'},
        {"background", no_argument, 0, 'b'},
        {"address", required_argument, 0, 'a'},
        {"port", optional_argument, 0, 'p'},
        {"delay", required_argument, 0, 'd'},
        {"workers", required_argument, 0, 'w'},
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
                return -1;
            }
            break;
        case 'd': {
            char *endptr;
            long int n = str2l(optarg, &endptr, 10);
            if (errno || n < 0 || n > 30) {
                fprintf(stderr, "invalid delay: %s\n", optarg);
                return -1;
            } else {
                conf.delay = (unsigned int) n;
            }
            break;
        }
        case 'w': {
            char *endptr;
            conf.workers = atoi(optarg);
            if (conf.workers <= 0 || conf.workers > 8) {
                fprintf(stderr, "invalid workers: %s\n", optarg);
                return -1;
            }
            break;
        }
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
        return -1;
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
        ec = -1;
    } else { /* parent, check child status */
        int wstatus;
        if (waitpid(pid, &wstatus, WNOHANG) != 0) {
            ec = -1;
            perror("check forked process");
        } else {
            if (WIFEXITED(wstatus) || WIFSIGNALED(wstatus)) {
                ec = -1;
            }
        }
    }
EXIT:
    if (ec) {
        fprintf(stderr, "exit with error, check log\n");
    }
    return ec;
}
