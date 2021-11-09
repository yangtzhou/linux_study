#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include "../globalfifo_signal/globalfifo.h"

#define BUF_LEN     16

int gbl_fifo_fd = -1;

static void signalio_handler(int signum)
{
    int i = 0;
    int count = 0;
    char buf[BUF_LEN];

    printf("received a signal from globalfifo, signalnum: %d\n", signum);

    while((count = read(gbl_fifo_fd, buf, BUF_LEN)) > 0) {
        for (i = 0; i < count; i++)
            printf("%c", buf[i]);
    }
    printf("\n");
}

int main()
{
    int oflags = 0;

    gbl_fifo_fd = open("/dev/globalfifo0", O_RDWR, S_IRUSR|S_IWUSR);
    if (gbl_fifo_fd < 0) {
        printf("open /dev/globalfifo0 failed");
        return -1;
    }

    signal(SIGIO, signalio_handler);
    fcntl(gbl_fifo_fd, F_SETOWN, getpid());
    oflags = fcntl(gbl_fifo_fd, F_GETFL);
    fcntl(gbl_fifo_fd, F_SETFL, oflags|FASYNC);

    while (1)
        sleep(100);

    return 0;
}

