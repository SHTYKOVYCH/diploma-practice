#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/cred.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/wait.h>

#define CBUFF_SIZE 65536

#define RW_CHUNK_SIZE 4096
#define RW_LAST_TREE_BYTES 0xfff
#define RW_SHIFT_CONSTANT 12

#define MSG_PREFIX "lab2: "
#define ERROR_MESSAGE_MEMORY "Error: Couldn't allocate memory\n"

dev_t device;
struct cdev dev;

struct mutex* pipesMutex;

struct pipe {
    char* cbuff;
    unsigned int bitesAwaible;
    int head, tail;
    struct mutex* mutex;
    int numOfDescriptors;

    uid_t userId;

    wait_queue_head_t queue;

    struct list_head list;
};

struct pipe* pipes;

void* tryAllocateMemory(void** buff, int size) {
    *buff = kmalloc(size, GFP_USER);

    if (!(*buff)) {
        pr_info(MSG_PREFIX ERROR_MESSAGE_MEMORY);
        return NULL;
    }

    return *buff;
}

struct pipe* pipeInit(void) {
    struct pipe* newPipe;

    pr_info(MSG_PREFIX "creating new pipe\n");

    if (!tryAllocateMemory((void**)&newPipe, sizeof(struct pipe))) {
        pr_info(MSG_PREFIX "cound't create pipe(1)\n");
        return NULL;
    }

    if (!tryAllocateMemory((void**)&newPipe->cbuff, CBUFF_SIZE)) {
        pr_info(MSG_PREFIX "cound't create pipe(2)\n");
        kfree(newPipe);
        return NULL;
    }

    if (!tryAllocateMemory((void**)&newPipe->mutex, sizeof(struct mutex))) {
        pr_info(MSG_PREFIX "cound't create pipe(3)\n");
        kfree(newPipe->cbuff);
        kfree(newPipe);
        return NULL;
    }

    mutex_init(newPipe->mutex);

    newPipe->numOfDescriptors = 0;

    newPipe->head = 0;
    newPipe->tail = 0;

    newPipe->userId = current_uid().val;

    INIT_LIST_HEAD(&newPipe->list);

    init_waitqueue_head(&newPipe->queue);

    pr_info(MSG_PREFIX "created pipe for user %d\n", newPipe->userId);

    return newPipe;
}

void pipeDelete(struct pipe* pipeToDelete) {
    kfree(pipeToDelete->cbuff);
    kfree(pipeToDelete->mutex);
    kfree(pipeToDelete);
}

struct pipe* tryFindPipeWithUserId(struct pipe* pipes, uid_t userId) {
    struct pipe* needle;

    pr_info(MSG_PREFIX "searching pipe for user %d\n", userId);

    if (&pipes->list && pipes->userId == userId) {
        pr_info(MSG_PREFIX "found pipe for user %d\n", userId);
        return pipes;
    } 

    list_for_each_entry(needle, &pipes->list, list) {
        pr_info(MSG_PREFIX "needle is %d\n", needle->userId);
        if (needle->userId == userId) {
            pr_info(MSG_PREFIX "found pipe for user %d\n", userId);
            return needle;
        }
    }

    pr_info(MSG_PREFIX "didn't find a pipe for user %d\n", userId);
    return NULL;
}

void write_to_cbuff(struct pipe* writePipe, const char* buff, unsigned int numOfBytes) {
    unsigned i = 0;
    while (i < numOfBytes) {
        writePipe->cbuff[writePipe->tail] = buff[i];
        writePipe->tail += 1;
        i += 1;
    }

    writePipe->bitesAwaible += numOfBytes;
}

void read_from_cbuff(struct pipe* readPipe, char* buff, unsigned int numOfBytes) {
    unsigned i = 0;
    while (i < numOfBytes) {
        buff[i] = readPipe->cbuff[readPipe->head];
        readPipe->head += 1;
        i += 1;
    }

    readPipe->bitesAwaible -= numOfBytes;
}

int device_open(struct inode* iocl, struct file* f) {
    // pointer for choosed pipe
    struct pipe* pipeToRemember;

    // locking list mutex
    mutex_lock(pipesMutex);

    // if there is no pipes
    if (!pipes) {
        // trying to init new one
        pr_info(MSG_PREFIX "allocating memory for list\n");

        pipes = pipeInit();

        if (!pipes) {
            // if faild then releasing mutex and leaving with error
            mutex_unlock(pipesMutex);
            return -1;
        }

        // if inited then the pipe for user is the head of pipes, so remembering it
        pipeToRemember = pipes;
    } else {
        // if there are some pipes
        struct pipe* t_pipe = tryFindPipeWithUserId(pipes, current_uid().val);

        // trying to find pipe with the same user id
        if (!t_pipe) {
            // if there is no such pipe then creating new one
            pr_info(MSG_PREFIX "allocating memory for new pipe\n");
            if (!(t_pipe = pipeInit())) {
                mutex_unlock(pipesMutex);
                return -1;
            }

            // adding new pipe to the list
            list_add(&t_pipe->list, &pipes->list);
        }

        // Pipe for user is either a founded one or the new one
        // in both cases remembering it and setting num of file's descriptors to 1
        pipeToRemember = t_pipe;
    }

    mutex_lock(pipeToRemember->mutex);
    pipeToRemember->numOfDescriptors += 1;
    mutex_unlock(pipeToRemember->mutex);

    // as we don't do anything with list, we can release it
    mutex_unlock(pipesMutex);

    // putting pipe pointer so we don't need to search it every time
    f->private_data = pipeToRemember;

    pr_info(MSG_PREFIX "pipe opened for %d\n", pipeToRemember->userId);

    return 0;
}

int device_close(struct inode* iocl, struct file* f) {
    // getting pointer to the user's pipe
    struct pipe* currentPipe = f->private_data;

    // locking list as we gonna modify it
    mutex_lock(pipesMutex);

    // also blocking pipe as we gonna work with it
    mutex_lock(currentPipe->mutex);

    // decreasing num of descriptors
    currentPipe->numOfDescriptors -= 1;

    // if there is no other users of this pipe - delete it
    if (currentPipe->numOfDescriptors == 0) {
        pr_info(MSG_PREFIX "deleting pipe\n");
        // if we deleting head of the list then choosing 
        if (pipes == currentPipe) {
            // if the list contains only one element - deinit it
            if (pipes->list.next == &pipes->list) {
                pr_info(MSG_PREFIX "now there is no list\n");
                pipes = NULL;
            } else {
                // else just move head one element further and removing element from it
                pipes = list_entry(pipes->list.next, struct pipe, list);
                pr_info(MSG_PREFIX "now the head is pipe for user %d\n", pipes->userId);
                list_del(&currentPipe->list);
            }
        } else {
            list_del(&currentPipe->list);
        }

        pr_info(MSG_PREFIX "deleted pipe for %d\n", currentPipe->userId);
        // then just deleting pipe
        pipeDelete(currentPipe);
        mutex_unlock(pipesMutex);

        return 0;
    }

    // unlocking list and pipe
    mutex_unlock(currentPipe->mutex);
    mutex_unlock(pipesMutex);

    pr_info(MSG_PREFIX "closed pipe for %d\n", currentPipe->userId);
    return 0;
}

static ssize_t device_write(struct file* f, const char __user* buff, size_t numOfBytes, loff_t* offset) {
    // getting user's pipe
    struct pipe* writePipe = f->private_data;

    // locking it
    mutex_lock(writePipe->mutex);

    // we gonna write data by chunks
    char t_buff[RW_CHUNK_SIZE] = {0};
    // smarty way to divide by 2^n value
    unsigned int numOfIteations = numOfBytes >> RW_SHIFT_CONSTANT;

    // writing to pipe by chunks
    unsigned int i = 0;
    while(i < numOfIteations) {
        copy_from_user(t_buff, buff + RW_CHUNK_SIZE*i, RW_CHUNK_SIZE);
        write_to_cbuff(writePipe, t_buff, RW_CHUNK_SIZE);
        i += 1;
    }

    // as we divide by chunk size there might be some remains
    if (numOfBytes & RW_LAST_TREE_BYTES) {
        copy_from_user(t_buff, buff + RW_CHUNK_SIZE*numOfIteations, numOfBytes & RW_LAST_TREE_BYTES);
        write_to_cbuff(writePipe, t_buff, numOfBytes & RW_LAST_TREE_BYTES);
    }

    pr_info(MSG_PREFIX"recieved message\n");

    // unlocking the pipe
    mutex_unlock(writePipe->mutex);
    wake_up_interruptible(&writePipe->queue);

    return numOfBytes;
}

static ssize_t device_read(struct file* f, char __user* buff, size_t numOfBytes, loff_t* offset) {
    // getting user's pipe
    struct pipe* readPipe = f->private_data;

    while (1) {
        // locking it
        mutex_lock(readPipe->mutex);

        if (readPipe->bitesAwaible) {
            // we're gonna read data by chunks
            char t_buff[RW_CHUNK_SIZE] = {0};
            int bytesToWrite = numOfBytes;

            // we might not have enought data so choose lessier number
            if (readPipe->bitesAwaible < numOfBytes) {
                bytesToWrite = readPipe->bitesAwaible;
            }

            // the same as for writing
            unsigned int numOfIteations = bytesToWrite >> RW_SHIFT_CONSTANT;

            unsigned int i = 0;
            while (i < numOfIteations) {
                read_from_cbuff(readPipe, t_buff, RW_CHUNK_SIZE);
                copy_to_user(buff + RW_CHUNK_SIZE * i, t_buff, RW_CHUNK_SIZE);
                i += 1;
            }

            if (numOfBytes & RW_LAST_TREE_BYTES) {
                read_from_cbuff(readPipe, t_buff, bytesToWrite & RW_LAST_TREE_BYTES);
                copy_to_user(buff + (RW_CHUNK_SIZE * numOfIteations), t_buff, bytesToWrite & RW_LAST_TREE_BYTES);
            }
            //
            // unlocking pipe
            mutex_unlock(readPipe->mutex);

            pr_info(MSG_PREFIX "sent message of %d bytes\n", bytesToWrite);

            return numOfBytes;
        }

        // unlocking pipe
        mutex_unlock(readPipe->mutex);

        wait_event_interruptible(readPipe->queue, readPipe->bitesAwaible);
    }
}

struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = device_open,
    .release = device_close,
    .read = device_read,
    .write = device_write
};

static int __init lab2_init(void) {
    // allocated number
    int error = alloc_chrdev_region(&device, 0, 1, "dmitrii_pipe");

    if (error < 0) {
        pr_info(MSG_PREFIX "could't register device %d\n", error);
        return error;
    }

    // initing the device
    cdev_init(&dev, &fops);

    int err = cdev_add(&dev, device, 1);

    if (err) {
        pr_info(MSG_PREFIX "coulnd't init cdev\n");
        return err;
    }

    // allocating memory for list mutex
    if (!tryAllocateMemory((void**)&pipesMutex, sizeof(struct mutex))) {
        unregister_chrdev_region(device, 1);
        return -1;
    }

    // initing mutex
    mutex_init(pipesMutex);

    // initing list mutex
    pipes = NULL;

    pr_info(MSG_PREFIX "dmitrii_pipe loaded\n");

    return 0;
}

static void __exit lab2_exit(void) {
    mutex_lock(pipesMutex);
    kfree(pipesMutex);

    if (pipes) {

        if (pipes->list.next != &pipes->list) {
            struct pipe* needle;
            struct pipe* n;

            for (needle = list_entry(pipes->list.next, struct pipe, list); needle != &pipes->list;) {
                n = list_entry(needle->list.next, struct pipe, list);

                pipeDelete(needle);

                needle = n;
            }
        }

        pipeDelete(pipes);
    }




    cdev_del(&dev);
    unregister_chrdev_region(device, 1);
    pr_info(MSG_PREFIX "dmitrii_pipe unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dmitrii Nashtykov");
