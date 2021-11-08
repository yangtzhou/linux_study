#include <linux/ioctl.h>

#define GLOBALFIFO_DEV_NUM  8

#define GLOBALFIFO_TYPE         'G'

#define GLOBALFIFO_IOC_CLEAR    _IO(GLOBALFIFO_TYPE, 1)
