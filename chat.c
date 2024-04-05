/* chat.c: Example char device module.
 *
 */
/* Kernel Programming */
#define MODULE
#define LINUX
#define __KERNEL__

#include <linux/kernel.h>  	
#include <linux/module.h>
#include <linux/time.h>
#include <linux/fs.h>     
#include <linux/slab.h>  		
#include <asm/uaccess.h>
#include <linux/errno.h>  
#include <asm/segment.h>
#include <asm/current.h>
#include <linux/list.h>
#include <asm/types.h>

#include "chat.h"

#define MY_DEVICE "chat"

MODULE_AUTHOR("Anonymous");
MODULE_LICENSE("GPL");

/* globals */
int my_major = 0; /* will hold the major # of my device driver */
struct timeval tv; /* Used to get the current time */
static LIST_HEAD(rooms); // used to save rooms

struct file_operations my_fops = {
    .open = my_open,
    .release = my_release,
    .read = my_read,
    .write = my_write,
    .ioctl = my_ioctl,
    .llseek = my_llseek
};

int init_module(void)
{
    // This function is called when inserting the module using insmod
    my_major = register_chrdev(my_major, MY_DEVICE, &my_fops);

    if (my_major < 0)
    {
	    printk(KERN_WARNING "can't get dynamic major\n");
	    return my_major;
    }

    return 0;
}


void cleanup_module(void)
{
    // This function is called when removing the module using rmmod
    unregister_chrdev(my_major, MY_DEVICE);

    return;
}

int my_open(struct inode *inode, struct file *filp)
{
    // handle open
    room_data* room = (room_data*)get_room_data(inode->i_rdev);
    if(!room){
        return ENOMEM;
    }

    file_data* fdata = (file_data*)kmalloc(sizeof(*fdata), GFP_KERNEL);
    if(!fdata){
        room->open_cnt--;
        if(!room->open_cnt){
            list_del(&room->list);
            kfree(room);
            return ENOMEM;        
        }
    }

    fdata->cur = room->mlist;
    fdata->room = room;
    filp->private_data = fdata;
    filp->f_pos = 0;
    MOD_INC_USE_COUNT;

    return 0;
}


int my_release(struct inode *inode, struct file *filp) {
    // handle file closing
    room_data* data = ((file_data*)filp->private_data)->room;
    if(!data){
        return -EINVAL;
    }
    data->open_cnt--;
    MOD_DEC_USE_COUNT;
    if(data->open_cnt){ //still open by others
        return 0;
    }

    // free all data 
    msg_list* p = data->mlist;
    msg_list* next;
    while(p){
        next = p->next;
        kfree(p);
        p = next;
    }
    list_del(&data->list);
    kfree(data);
    kfree(filp->private_data);

    return 0;
}

ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) {
    if (!buf) {
        return -EFAULT;
    }

    room_data* room = ((file_data*)filp->private_data)->room;
    if (!room) {
        return -EFAULT;
    }
    msg_list* pos = ((file_data*)filp->private_data)->cur;
    msg_list* list_end = room->ml_tail;
    int read_amount = (int)count / (int)sizeof(message_t);
    int count_msg = 0;

    while (pos != list_end && count_msg < read_amount) {
        if (copy_to_user(buf, &pos->msg, sizeof(message_t))) {
            return -EBADF;
        }

        ((message_t*)buf)++;
        count_msg++;
        pos = pos->next;
    }
    *f_pos += count_msg * sizeof(message_t);
    ((file_data*)filp->private_data)->cur = pos;
    return count_msg * sizeof(message_t);
}

ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *off) {
    if(count > MAX_MESSAGE_LENGTH){
        return -ENOSPC;
    }
    if(!filp || !buf){
        return -EFAULT;
    }
    room_data* room = ((file_data*)filp->private_data)->room;
    if(!room){
        return -EFAULT;
    }
    msg_list* tail = room->ml_tail;

    //always keep empty message at the end;
    tail->next = (msg_list*)kmalloc(sizeof(*tail->next), GFP_KERNEL);
    if (!tail->next) {
        return ENOMEM;
    }
    memset(tail->msg.message, 0, MAX_MESSAGE_LENGTH);
    if (copy_from_user(&tail->msg.message, buf, count)) {
        return -EBADF;
    }
    tail->msg.pid = getpid();
    tail->msg.timestamp = gettime();

    room->ml_tail = tail->next;
    room->ml_tail->next = NULL;
    memset(room->ml_tail->msg.message, 0, MAX_MESSAGE_LENGTH);
    
    return count;
}


int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case COUNT_UNREAD:
            return my_count_unread(filp);
	    break;
        default:
	    return -ENOTTY;
    }

    return 0;
}

int my_count_unread(struct file *filp) {
    room_data* room = ((file_data*)filp->private_data)->room;
    msg_list* pos = ((file_data*)filp->private_data)->cur;
    msg_list* list_end = room->ml_tail;
    int count = 0;
    
    while(pos != list_end){
        pos = pos->next;
        count++;
    }

    return count;
}

loff_t my_llseek(struct file *filp, loff_t offset, int whence) {
    // Change f_pos field in filp according to offset and whence.
    if(whence != 0){
        return -EINVAL;
    }
    
    int msg_num = (int)offset / (int)sizeof(message_t);
    room_data* room = ((file_data*)filp->private_data)->room;
    msg_list* pos = room->mlist;
    msg_list* list_end = room->ml_tail;
    int idx = 0;
    
    while(pos != list_end && idx < msg_num){
        idx++;
        pos = pos->next;
    }
    
    filp->f_pos = idx * sizeof(message_t);
    ((file_data*)filp->private_data)->cur = pos;

    return filp->f_pos;
}

time_t gettime() {
    do_gettimeofday(&tv);
    return tv.tv_sec;
}

pid_t getpid() {
    return current->pid;
}

// gets room data for given dev, creates new room if needed!
struct room_data *get_room_data(dev_t dev) {
    room_data *data;
    struct list_head* pos;
    // Iterate through the device list to find the existing room_data structure
    list_for_each (pos, &rooms) {
        data = list_entry(pos, room_data, list);
        if (data->dev == dev) {
            // room_data structure already exists for this device
            data->open_cnt++;
            return data;
        }
    }

    // room_data structure not found, create a new one
    data = (room_data*)kmalloc(sizeof(*data), GFP_KERNEL);
    if (!data) {
        return NULL;
    }
    data->dev = dev;
    data->open_cnt = 1; // we just opened
    // always create empty msg at the end of list
    data->mlist = (msg_list*)kmalloc(sizeof(*data->mlist), GFP_KERNEL); 
    if (!data->mlist) {
        kfree(data);
        return NULL; 
    }

    data->ml_tail = data->mlist;
    data->ml_tail->next = NULL;

    list_add(&data->list, &rooms);

    return data;
}
