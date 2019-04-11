#include <stdlib.h>
#include <signal.h>
#include <arpa/inet.h>

#include <netutils/netutils.h>
#include <logutils/syslogutils.h>
#include <daemonutils.h>

/* #define BACKLOG 20 */
#define BACKLOG SOMAXCONN

int running = 1;

struct config {
  char *ip;
  int port;
};

int loop_fork(int srv_fd) {
  int ec = 0;
  while (running) {


  }
EXIT:
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
    const char *name = "hellosrv";
    struct config conf;
    conf.ip = NULL;
    conf.port = 1234;

    openlog(name, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL0);

    pid = daemon_init(background, 1, background);
    if (pid ==  0) { /* child process */
      ec = start_server(&conf);
    } else if (pid < 0) {
      _LOG_ERROR_ERRNO(root_logger, "%s: %s", errno, "fork");
      ec = EXIT_FAILURE;
    }
EXIT:
    return ec;
}
