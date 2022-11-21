#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/cred.h>
#include <linux/mutex.h>
#include <linux/slab.h>

dev_t device;
struct cdev dev;

int numOfDescriptors;
char* cbuff;
unsigned int bitesAwaible;
int head, tail;
struct mutex* mutex;

void write_to_cbuff(char* buff, unsigned int numOfBytes) {
    unsigned i = 0;
    while (i < numOfBytes) {
        cbuff[tail] = buff[i];
        tail += 1;
        i += 1;
    }

    bitesAwaible += numOfBytes;
}

void read_from_cbuff(char* buff, unsigned int numOfBytes) {
    unsigned i = 0;
    while (i < numOfBytes) {
        buff[i] = cbuff[head];
        head += 1;
        i += 1;
    }

    bitesAwaible -= numOfBytes;
}

int device_open(struct inode* iocl, struct file* f) {
    printk(KERN_INFO "[LOG] dmitrii_pipe opened\n");

    return 0;
}

int device_close(struct inode* iocl, struct file* f) {
    printk(KERN_INFO "[LOG] dmitrii_pipe closed\n");

    return 0;
}

static ssize_t device_write(struct file* f, const char __user* buff, size_t numOfBytes, loff_t* offset) {
    mutex_lock(mutex);

    char t_buff[4096] = {0};
    unsigned int numOfIteations = numOfBytes >> 12;

    unsigned int i = 0;
    while(i < numOfIteations) {
        copy_from_user(t_buff, buff + 4096 * i, 4096);
        write_to_cbuff(t_buff, 4096);
        i += 1;
    }

    if (numOfBytes & 0xfff) {
        copy_from_user(t_buff, buff + (4096 * numOfIteations), numOfBytes & 0xfff);
        write_to_cbuff(t_buff, numOfBytes & 0xfff);
    }

    printk(KERN_INFO "[LOG] Recieved message");

    mutex_unlock(mutex);
    return numOfBytes;
}

static ssize_t device_read(struct file* f, char __user* buff, size_t numOfBytes, loff_t* offset) {
    mutex_lock(mutex);


    char t_buff[4096] = {0};
    int bytesToWrite = numOfBytes;

    if (bitesAwaible < numOfBytes) {
        bytesToWrite = bitesAwaible;
    }

    unsigned int numOfIteations = bytesToWrite >> 12;

    unsigned int i = 0;
    while (i < numOfIteations) {
        read_from_cbuff(t_buff, 4096);
        copy_to_user(buff + 4096 * i, t_buff, 4096);
        i += 1;
    }

    if (numOfBytes & 0xfff) {
        read_from_cbuff(t_buff, numOfBytes & 0xfff);
        copy_to_user(buff + (4096 * numOfIteations), t_buff, numOfBytes & 0xfff);
    }

    mutex_unlock(mutex);
    printk(KERN_INFO "[LOG] Sent message of %d bytes\n", bytesToWrite);

    return numOfBytes;
}

struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_close,
    .read = device_read,
    .write = device_write
};

static int __init lab2_init(void) {
    int error = alloc_chrdev_region(&device, 0, 1, "dmitrii_pipe");

    if (error < 0) {
        printk(KERN_CRIT "[FAIL] Could't register device %d\n", error);
        return error;
    }


    printk(KERN_INFO "[LOG] dmitrii_pipe got major num: %d\n", MAJOR(device));

    cdev_init(&dev, &fops);

    int err = cdev_add(&dev, device, 1);

    if (err) {
        printk(KERN_CRIT"[FAIL] Coulnd't init cdev\n");
        return err;
    }

    cbuff = kmalloc(65536, GFP_USER);

    if (!cbuff) {
        printk(KERN_ERR "Couldn't allocate memory\n");
        unregister_chrdev_region(device, 1);
        return -1;
    }

    bitesAwaible = 0;

    mutex = kmalloc(sizeof(struct mutex), GFP_USER);

    if (!mutex) {
        printk(KERN_ERR "Couldn't allocate memory\n");
        unregister_chrdev_region(device, 1);
        kfree(cbuff);
        return -1;
    }

    mutex_init(mutex);
    head = 0;
    tail = 0;

    printk(KERN_INFO "[LOG] Module loaded\n");

    return 0;
}

static void __exit lab2_exit(void) {
    cdev_del(&dev);
    kfree(cbuff);
    kfree(mutex);
    unregister_chrdev_region(device, 1);
    printk(KERN_INFO "[LOG] dmitrii_pipe unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dmitrii Nashtykov");
