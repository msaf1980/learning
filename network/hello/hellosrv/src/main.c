#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <netutils/netutils.h>
#include <logutils/syslogutils.h>
#include <daemonutils.h>

/* #define BACKLOG 20 */
#define BACKLOG SOMAXCONN

#define BUFSIZE 4096

int running = 1;

struct config {
  char *ip;
  int port;
};

int server_session(int sess_fd, const char *ip, const u_short port)
{
    char buf[BUFSIZE];
    strcpy(buf, "Hi there!\n");
    ssize_t len = strlen(buf);
    for (int i = 0; i < 5; i++) {
      if (send(sess_fd, buf, len, MSG_NOSIGNAL) == -1) {
        _LOG_ERROR_ERRNO(root_logger, "send on client connection from %s:%d: %s", 
                         errno, ip, port);
        break;
      }
    }
    /* _LOG_INFO(root_logger, "close client connection from %s:%d", ip, port); */
    close(sess_fd);
}

int loop_fork(int srv_fd) {
  int ec = 0;
  SA_IN client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  char ipbuf[INET_ADDRSTRLEN];
  while (running) {
    int sess_fd = accept(srv_fd, (SA *) &client_addr, &client_addr_len);
		if (sess_fd == -1) {
			_LOG_ERROR_ERRNO(root_logger, "%s on socket %d: %s", errno, "accept", srv_fd);
			continue;
		}

		/* Format client IP address */
    if (getnameinfo( (SA *) &client_addr, client_addr_len, 
                     ipbuf, INET_ADDRSTRLEN, 0,0, NI_NUMERICHOST) == 0) {
      _LOG_INFO(root_logger, "connect from %s:%d", ipbuf, client_addr.sin_port);
    } else {
      _LOG_ERROR(root_logger, "invalid address for socket %d", sess_fd);
    }

		pid_t pid = fork();
    if (pid == -1) {
      _LOG_ERROR_ERRNO(root_logger, "fork on client connection from %s:%d: %s", 
                       errno, ipbuf, client_addr.sin_port);
    } else if (pid == 0) {
      /* child process */
      close(srv_fd);
      server_session(sess_fd, ipbuf, client_addr.sin_port);
      exit(0);
    }
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

	if ( (srv_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ) {
    ec = EXIT_FAILURE; _LOG_ERROR_ERRNO(root_logger, "%s: %s", errno, "socket"); goto EXIT;
  }
	set_reuseaddr(srv_fd);

	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(conf->port);
	if (conf->ip == NULL)
		srv_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* List on any IP */
	else if ( inet_aton(conf->ip, &srv_addr.sin_addr) == 0 ) {
    ec = EXIT_FAILURE; _LOG_ERROR(root_logger, "invalid address: %s", conf->ip); goto EXIT;
  }

	if ( bind(srv_fd, (SA *) &srv_addr, sizeof(srv_addr)) == -1 ) {
    ec = EXIT_FAILURE; _LOG_ERROR_ERRNO(root_logger, "%s: %", errno, "bind"); goto EXIT;
  }

  /* set_nonblock(srv_fd); */

	if ( listen(srv_fd, BACKLOG) == -1 ) {
		ec = EXIT_FAILURE; _LOG_ERROR_ERRNO(root_logger, "%s: %", errno, "listen"); goto EXIT;
  }

	_LOG_INFO(root_logger, "%s", "startup");

  ec = loop_fork(srv_fd);

EXIT:
  return ec;
}

int main(int argc, char * const argv[]) {
    int ec = 0, pid;
    int background = 0;
    int closefd = FD_NOCLOSE;
    const char *name = "hellosrv";
    struct config conf;
    conf.ip = NULL;
    conf.port = 1234;

    openlog(name, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);

    if (background)
      closefd = FD_CLOSE_STD;

    pid = daemon_init(background, 1, closefd);
    if (pid ==  0) { /* child process */
      ec = start_server(&conf);
    } else if (pid < 0) {
      _LOG_ERROR_ERRNO(root_logger, "%s: %s", errno, "fork");
      ec = EXIT_FAILURE;
    }
EXIT:
    return ec;
}
