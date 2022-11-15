#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/cdev.h>

int majorNum;

dev_t device;
struct cdev dev;

int numOfDescriptors;

struct file_operations fops = {
    .owner = THIS_MODULE
};

int device_open(struct inode* iocl, struct file* f) {
    numOfDescriptors += 1;

}

static int __init lab2_init(void) {
    majorNum = alloc_chrdev_region(&device, 0, 1, "dmitrii_pipe");

    if (majorNum < 0) {
        printk(KERN_CRIT "[FAIL] Could't register device %d\n", majorNum);
        return majorNum;
    }


    printk(KERN_INFO "[LOG] dmitrii_pipe got major num: %d\n", MAJOR(device));

    cdev_init(&dev, &fops);

    int err = cdev_add(&dev, device, 1);

    if (err) {
        printk(KERN_CRIT"[FAIL] Coulnd't init cdev\n");
        return err;
    }

    printk(KERN_INFO "[LOG] Module loaded\n");

    numOfDescriptors = 0;
    return 0;
}

static void __exit lab2_exit(void) {
    cdev_del(&dev);
    unregister_chrdev_region(device, 1);
    printk(KERN_INFO "[LOG] dmitrii_pipe unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dmitrii Nashtykov");
