#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include "../globalfifo_poll/globalfifo.h"

#define BUF_LEN     16

int main(void)
{
    int i = 0;
    int j = 0;
    struct pollfd fds[GLOBALFIFO_DEV_NUM];
    char fname[32];
    char buf[BUF_LEN];
    int ret = 0;
    int count = 0;

    for (i = 0; i < GLOBALFIFO_DEV_NUM; i++) {
        sprintf(fname, "/dev/globalfifo%d", i);
        fds[i].fd = open(fname, O_RDONLY | O_NONBLOCK);
        if (fds[i].fd < 0) {
            printf("open %s failed\n", fname);
            break;
        }

        fds[i].events = POLLIN | POLLRDNORM;
    }

    if (i < GLOBALFIFO_DEV_NUM) {
        i--;
        while (i >= 0)
            close(fds[i].fd);
        return -1;
    }

    while (1) {
        ret = poll(fds, GLOBALFIFO_DEV_NUM, 10000);
        if (ret > 0) {
            printf("poll successfully\n");

            for (i = 0; i < GLOBALFIFO_DEV_NUM; i++) {
                if (fds[i].revents & (POLLIN | POLLRDNORM)) {
                    printf("globalfifo-%d: ", i);
                    while((count = read(fds[i].fd, buf, BUF_LEN)) > 0) {
                        for (j = 0; j < count; j++)
                            printf("%c", buf[j]);
                    }
                    printf("\n");
                } else if (fds[i].revents != 0) {
                    printf("globalfifo-%d: poll return exception [0x%x]\n",
                        i, (unsigned int)fds[i].revents);
                }
            }

            printf("\n");
        } else if (ret == 0) {
            printf("poll timeout\n\n");
        } else {
            printf("poll error [%d]\n", ret);
        }
    }

    for (i = 0; i < GLOBALFIFO_DEV_NUM; i++) 
        close(fds[i].fd);

    return 0;
}
