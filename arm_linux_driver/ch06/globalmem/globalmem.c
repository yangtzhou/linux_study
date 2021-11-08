#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define GLOBALMEM_SIZE      0x1000
#define GLOBALMEM_MAGIC     'g'
#define MEM_CLEAR           _IO(GLOBALMEM_MAGIC, 0)
#define GLOBALMEM_MAJOR     230
#define GLOBALMEM_DEV_NUM   8

static int globalmem_major = GLOBALMEM_MAJOR;
module_param(globalmem_major, int, S_IRUGO);

struct globalmem_dev {
    struct cdev cdev;
    unsigned char mem[GLOBALMEM_SIZE];
};

static struct globalmem_dev *globalmem_devp = NULL;

static int globalmem_open(struct inode *inode, struct file *filp)
{
    filp->private_data = container_of(inode->i_cdev,
        struct globalmem_dev, cdev);
    return 0;
}

static int globalmem_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t globalmem_read(struct file *filp, char __user *buf,
    size_t size, loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned int count = size;
    int ret = 0;
    struct globalmem_dev *dev = filp->private_data;

    if (p >= GLOBALMEM_SIZE)
        return 0;

    if (count > GLOBALMEM_SIZE - p)
        count = GLOBALMEM_SIZE - p;

    if (copy_to_user(buf, dev->mem + p, count))
        ret = -EFAULT;
    else {
        *ppos += count;
        ret = count;
        printk(KERN_INFO "read globalmem %u bytes from %lu\n", count, p);
    }

    return ret;
}

static ssize_t globalmem_write(struct file *filp, const char __user *buf,
    size_t size, loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned int count = size;
    int ret = 0;
    struct globalmem_dev *dev = filp->private_data;

    if (p >= GLOBALMEM_SIZE)
        return 0;

    if (count > GLOBALMEM_SIZE - p)
        count = GLOBALMEM_SIZE - p;

    if (copy_from_user(dev->mem + p, buf, count))
        ret = -EFAULT;
    else {
        *ppos += count;
        ret = count;
        printk(KERN_INFO "write globalmem %u bytes to %lu\n", count, p);
    }

    return ret;
}

static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig)
{
    loff_t ret = 0;

    switch (orig) {
    case 0:
        if (offset < 0) {
            ret = -EINVAL;
            break;
        }

        if (offset > GLOBALMEM_SIZE) {
            ret = -EINVAL;
            break;
        }

        filp->f_pos = offset;
        ret = filp->f_pos;
        break;

    case 1:
        if (filp->f_pos + offset > GLOBALMEM_SIZE) {
            ret = -EINVAL;
            break;
        }

        if (filp->f_pos + offset < 0) {
            ret = -EINVAL;
            break;
        }

        filp->f_pos += offset;
        ret = filp->f_pos;
        break;

    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

static long globalmem_ioctl(struct file *filp,
    unsigned int cmd, unsigned long arg)
{
    struct globalmem_dev *dev = filp->private_data;

    switch (cmd) {
    case MEM_CLEAR:
        memset(dev->mem, 0, GLOBALMEM_SIZE);
        printk(KERN_INFO "globalmem is set to zero\n");
        break;

    default:
        return -ENOIOCTLCMD;
    }

    return 0;
}

static const struct file_operations globalmem_fops = {
    .owner = THIS_MODULE,
    .open = globalmem_open,
    .release = globalmem_release,
    .llseek = globalmem_llseek,
    .read = globalmem_read,
    .write = globalmem_write,
    .unlocked_ioctl = globalmem_ioctl,
};

static int globalmem_setup_cdev(struct globalmem_dev *dev, int index)
{
    int ret = 0;
    int devno = MKDEV(globalmem_major, index);

    cdev_init(&dev->cdev, &globalmem_fops);
    dev->cdev.owner = THIS_MODULE;
    ret = cdev_add(&dev->cdev, devno, 1);
    if (ret)
        printk(KERN_ERR "Error %d adding globalmem%d\n", ret, index);

    return ret;
}

static int __init globalmem_init(void)
{
    int ret = 0;
    int i = 0;
    dev_t devno = MKDEV(globalmem_major, 0);

    if (globalmem_major)
        ret = register_chrdev_region(devno, GLOBALMEM_DEV_NUM, "globalmem");
    else {
        ret = alloc_chrdev_region(&devno, 0, GLOBALMEM_DEV_NUM, "globalmem");
        globalmem_major = MAJOR(devno);
    }

    if (ret) {
        printk(KERN_ERR "Error %d registering globalmem dev_num\n", ret);
        return ret;
    }

    globalmem_devp = kzalloc(sizeof(struct globalmem_dev) * GLOBALMEM_DEV_NUM,
        GFP_KERNEL);
    if (!globalmem_devp) {
        printk(KERN_ERR "Error allocating globalmem data\n");
        ret = -ENOMEM;
        goto fail_malloc;
    }

    for (i = 0; i < GLOBALMEM_DEV_NUM; i++) {
        ret = globalmem_setup_cdev(&globalmem_devp[i], i);
        if (ret) {
            printk(KERN_ERR "Error %d initializing globalmem cdev %d\n",
                ret, i);
            goto fail_cdev;
        }
    }

    //printk(KERN_ERR "size: %lu %lu %lu %lu\n",
    //    sizeof(ssize_t), sizeof(size_t), sizeof(loff_t), sizeof(dev_t));

    return 0;

fail_cdev:
    while (i > 0) {
        i--;
        cdev_del(&globalmem_devp[i].cdev);
    }
    kfree(globalmem_devp);
fail_malloc:
    unregister_chrdev_region(devno, GLOBALMEM_DEV_NUM);
    return ret;
}
module_init(globalmem_init);

static void __exit globalmem_exit(void)
{
    int i = 0;

    for (i = 0; i < GLOBALMEM_DEV_NUM; i++)
        cdev_del(&globalmem_devp[i].cdev);

    kfree(globalmem_devp);
    globalmem_devp = NULL;
    unregister_chrdev_region(MKDEV(globalmem_major, 0), GLOBALMEM_DEV_NUM);
}
module_exit(globalmem_exit);

MODULE_AUTHOR("yang <yangtzhou@qq.com>");
MODULE_LICENSE("GPL v2");

