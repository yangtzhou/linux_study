#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/poll.h>

#define GLOBALFIFO_SIZE      0x1000
#define FIFO_CLEAR           0x1
#define GLOBALFIFO_MAJOR     230

static int globalfifo_major = GLOBALFIFO_MAJOR;
module_param(globalfifo_major, int, S_IRUGO);

struct globalfifo_dev {
    struct cdev cdev;
    unsigned int current_len;
    unsigned char fifo[GLOBALFIFO_SIZE];
    struct mutex mutex;
    wait_queue_head_t r_wait;
    wait_queue_head_t w_wait;
};

struct globalfifo_dev *globalfifo_devp;

static int globalfifo_open(struct inode *inode, struct file *filp)
{
    filp->private_data = globalfifo_devp;
    return 0;
}

static int globalfifo_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static long globalfifo_ioctl(struct file *filp,
    unsigned int cmd, unsigned long arg)
{
    struct globalfifo_dev *dev = filp->private_data;

    switch (cmd) {
    case FIFO_CLEAR:
        mutex_lock(&dev->mutex);
        memset(dev->fifo, 0, GLOBALFIFO_SIZE);
        dev->current_len = 0;
        mutex_unlock(&dev->mutex);
        printk(KERN_INFO "globalfifo is set to zero\n");
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

static unsigned int globalfifo_poll(struct file *filp,
    struct poll_table_struct *wait)
{
    unsigned int mask = 0;
    struct globalfifo_dev *dev = filp->private_data;

    mutex_lock(&dev->mutex);

    poll_wait(filp, &dev->r_wait, wait);
    poll_wait(filp, &dev->w_wait, wait);

    if (dev->current_len != 0) {
        mask |= POLLIN | POLLRDNORM;
    }

    if (dev->current_len != GLOBALFIFO_SIZE) {
        mask |= POLLOUT | POLLWRNORM;
    }

    mutex_unlock(&dev->mutex);
    return mask;
}

static ssize_t globalfifo_read(struct file *filp,
    char __user *buf, size_t size, loff_t *ppos)
{
    int ret = 0;
    struct globalfifo_dev *dev = filp->private_data;

    mutex_lock(&dev->mutex);

    while (dev->current_len == 0) {
        if (filp->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto exit1;
        } else {
            mutex_unlock(&dev->mutex);
            ret = wait_event_interruptible(dev->r_wait, (dev->current_len > 0));
            if (ret == 0) {
                mutex_lock(&dev->mutex);
            } else {
                printk(KERN_ERR "globalfifo wait for reading failed\n");
                ret = -ERESTARTSYS;
                goto exit2;
            }
        }
    }

    if (size > dev->current_len) {
        size = dev->current_len;
    }

    if (copy_to_user(buf, dev->fifo, size)) {
        printk(KERN_ERR "globalfifo copy driver data to user buffer failed\n");
        ret = -EFAULT;
    } else {
        memcpy(dev->fifo, dev->fifo + size, dev->current_len - size);
        dev->current_len -= size;
        printk(KERN_INFO "globalfifo read %lu bytes, current_len: %u\n",
            size, dev->current_len);
        wake_up_interruptible(&dev->w_wait);
        ret = size;
    }

exit1:
    mutex_unlock(&dev->mutex);
exit2:
    return ret;
}

static ssize_t globalfifo_write(struct file *filp,
    const char __user *buf, size_t size, loff_t *ppos)
{
    int ret = 0;
    struct globalfifo_dev *dev = filp->private_data;

    mutex_lock(&dev->mutex);

    while (dev->current_len == GLOBALFIFO_SIZE) {
        if (filp->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto exit1;
        } else {
            mutex_unlock(&dev->mutex);
            ret = wait_event_interruptible(dev->w_wait,
                (dev->current_len < GLOBALFIFO_SIZE));
            if (ret == 0) {
                mutex_lock(&dev->mutex);
            } else {
                printk(KERN_ERR "globalfifo wait for writing failed\n");
                ret = -ERESTARTSYS;
                goto exit2;
            }
        }
    }

    if (size > GLOBALFIFO_SIZE - dev->current_len) {
        size = GLOBALFIFO_SIZE - dev->current_len;
    }

    if (copy_from_user(dev->fifo + dev->current_len, buf, size)) {
        printk(KERN_ERR "globalfifo copy user data to driver buffer failed\n");
        ret = -EFAULT;
    } else {
        dev->current_len += size;
        printk(KERN_INFO "globalfifo write %lu bytes, current_len: %u\n",
            size, dev->current_len);
        wake_up_interruptible(&dev->r_wait);
        ret = size;
    }

exit1:
    mutex_unlock(&dev->mutex);
exit2:
    return ret;
}

static const struct file_operations globalfifo_fops = {
    .owner = THIS_MODULE,
    .read = globalfifo_read,
    .write = globalfifo_write,
    .unlocked_ioctl = globalfifo_ioctl,
    .poll = globalfifo_poll,
    .open = globalfifo_open,
    .release = globalfifo_release,
};

static void globalfifo_setup_cdev(struct globalfifo_dev *dev, int index)
{
    int err;
    int devno = MKDEV(globalfifo_major, index);

    cdev_init(&dev->cdev, &globalfifo_fops);
    dev->cdev.owner = THIS_MODULE;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_NOTICE "Error %d adding globalfifo%d", err, index);
    }
}

static int __init globalfifo_init(void)
{
    int ret;
    dev_t devno = MKDEV(globalfifo_major, 0);

    if (globalfifo_major) {
        ret = register_chrdev_region(devno, 1, "globalfifo");
    } else {
        ret = alloc_chrdev_region(&devno, 0, 1, "globalfifo");
        globalfifo_major = MAJOR(devno);
    }
    if (ret < 0)
        return ret;

    globalfifo_devp = kzalloc(sizeof(struct globalfifo_dev), GFP_KERNEL);
    if (!globalfifo_devp) {
        ret = -ENOMEM;
        goto fail_malloc;
    }

    mutex_init(&globalfifo_devp->mutex);
    init_waitqueue_head(&globalfifo_devp->r_wait);
    init_waitqueue_head(&globalfifo_devp->w_wait);
    globalfifo_setup_cdev(globalfifo_devp, 0);
    return 0;

fail_malloc:
    unregister_chrdev_region(devno, 1);
    return ret;
}
module_init(globalfifo_init);

static void __exit globalfifo_exit(void)
{
    cdev_del(&globalfifo_devp->cdev);
    kfree(globalfifo_devp);
    unregister_chrdev_region(MKDEV(globalfifo_major, 0), 1);
}
module_exit(globalfifo_exit);

MODULE_AUTHOR("Yang <yangtzhou@qq.com>");
MODULE_LICENSE("GPL v2");

