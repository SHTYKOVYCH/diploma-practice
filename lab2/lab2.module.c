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
int writen;
int start_buff, end_buff;
static struct mutex *mutex;

struct pipe {
    struct pipe* next;

    uid_t user_id;

    char* cbuff;
    int start_index, end_index;

    int numOfOpens;
};

struct pipe* pipes = NULL;

struct pipe* pipeInit(uid_t user_id) {
    struct pipe* pipe = kmalloc(sizeof(struct pipe), GFP_USER);

    if (!pipe) {
        printk(KERN_ERR "Couldn't allocate memory\n");
        return NULL;
    }

    pipe->cbuff = kmalloc(65536, GFP_USER);

    if (!pipe->cbuff) {
        printk(KERN_ERR "Couldn't allocate memory");
        kfree(pipes);
        return NULL;
    }

    pipe->start_index = 0;
    pipe->end_index = 0;

    pipe->numOfOpens = 1;

    return pipe;
}

void pipeFree(uid_t user_id) {
    struct pipe* pipeToDelete = pipes;

    while (pipeToDelete->user_id != user_id) {
        pipeToDelete = pipeToDelete->next;
    }
}

int device_open(struct inode* iocl, struct file* f) {
    /*    struct pipe* pipeForUser;

          if (!pipes) {
          pipes = pipeInit(current_uid().val);

          if (!pipes) {
          return -1;
          }

          pipeForUser = pipes;
          } else {
          struct pipe* elem = pipes;

          while (elem->user_id != current_uid().val && elem->next) {elem = elem->next;}

          if (elem->user_id == current_uid().val) {
          pipeForUser = elem->next;
          } else if (!elem->next) {
          elem->next = pipeInit(current_uid().val);

          if (!elem->next) {
          return -1;
          }

          pipeForUser = elem->next;
          }
          }

          f->private_data = pipeForUser;
          */

    printk(KERN_INFO "[LOG] dmitrii_pipe opened\n");

    return 0;
}

int device_close(struct inode* iocl, struct file* f) {
    /*
       struct pipe* pipe = file->private_data;

       pipe->numOfOpens -= 1;

       if (

       kfree(elem)
       */
    printk(KERN_INFO "[LOG] dmitrii_pipe closed\n");
    return 0;
}

static ssize_t device_write(struct file* f, const char __user* buff, size_t numOfBytes, loff_t* offset) {
    copy_from_user(cbuff, buff, numOfBytes);

    cbuff[numOfBytes] = 0;
    writen = numOfBytes;

    printk(KERN_INFO "[LOG] Recieved message: %s\n", cbuff);

    return numOfBytes;
}

static ssize_t device_read(struct file* f, char __user* buff, size_t numOfBytes, loff_t* offset) {
    printk(KERN_INFO "[LOG] Sent message of %d bytes\n", numOfBytes);

    if (writen > numOfBytes) {
        copy_to_user(buff, cbuff, numOfBytes);
        return numOfBytes;
    }

    copy_to_user(buff, cbuff, writen);
    return writen;
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

    writen = 0;

    printk(KERN_INFO "[LOG] Module loaded\n");

    return 0;
}

static void __exit lab2_exit(void) {
    cdev_del(&dev);
    kfree(cbuff);
    unregister_chrdev_region(device, 1);
    printk(KERN_INFO "[LOG] dmitrii_pipe unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dmitrii Nashtykov");
