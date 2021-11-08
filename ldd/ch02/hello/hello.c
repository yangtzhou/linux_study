#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

static char *whom = "world";
static int howmany = 1;
module_param(whom, charp, S_IRUGO);
module_param(howmany, int, S_IRUGO);

static int __init hello_init(void)
{
    int i = 0;

    for (i = 0; i < howmany; i++) {
        printk(KERN_ALERT "Hello, %s\n", whom);
    }

    return 0;
}
module_init(hello_init);

static void __exit hello_exit(void)
{
    printk(KERN_ALERT "Goodbye, world\n");
}
module_exit(hello_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("yang");
MODULE_DESCRIPTION("hello world kernel module");

