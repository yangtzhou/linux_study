#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include "../globalfifo_poll/globalfifo.h"

#define BUF_LEN     16

int main(void)
{
    int i = 0;
    int j = 0;
    int epfd = -1;
    int fd[GLOBALFIFO_DEV_NUM] = {-1};
    struct epoll_event ev;
    struct epoll_event rev[GLOBALFIFO_DEV_NUM];
    char fname[32];
    char buf[BUF_LEN];
    int ret = 0;
    int count = 0;

    epfd = epoll_create(GLOBALFIFO_DEV_NUM);
    if (epfd < 0) {
        printf("epoll create failed\n");
        return -1;
    }

    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN | EPOLLPRI;

    for (i = 0; i < GLOBALFIFO_DEV_NUM; i++) {
        sprintf(fname, "/dev/globalfifo%d", i);
        fd[i] = open(fname, O_RDONLY | O_NONBLOCK);
        if (fd[i] < 0) {
            printf("open %s failed\n", fname);
            ret = -1;
            goto exit;
        }

        printf("open %s: fd[%d]\n", fname, fd[i]);

        ev.data.fd = fd[i];
        ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd[i], &ev);
        if (ret) {
            printf("epoll add %s failed\n", fname);
            goto exit;
        }
    }

    printf("\n");

    while (1) {
        ret = epoll_wait(epfd, rev, GLOBALFIFO_DEV_NUM, 10000);
        if (ret > 0) {
            printf("epoll successfully ret[%d]\n", ret);

            for (i = 0; i < ret; i++) {
                printf("globalfifo-%u fd[%d]: ", i, rev[i].data.fd);
                while((count = read(rev[i].data.fd, buf, BUF_LEN)) > 0) {
                    for (j = 0; j < count; j++)
                        printf("%c", buf[j]);
                }
                printf("\n");
            }

            printf("\n");
        } else if (ret == 0) {
            printf("epoll timeout\n\n");
        } else {
            printf("epoll error [%d]\n", ret);
        }
    }

exit:
    for (i = 0; i < GLOBALFIFO_DEV_NUM; i++) {
        if (fd[i] >= 0)
            close(fd[i]);
    }

    if (epfd >= 0)
        close(epfd);

    return ret;
}
