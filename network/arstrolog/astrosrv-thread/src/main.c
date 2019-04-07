#include <stdlib.h>

#include <daemonutils.h>

int main(int argc, char * const argv[]) {
    int ec = 0, pid;
    int foregound = 0;

    pid = daemon_init(foreground, 0, 0);
    if (pid ==  0) { /* child process */

    } else if (pid < 0)
        ec = EXIT_FAILURE;
EXIT:
    return ec;
}
