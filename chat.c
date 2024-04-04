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
#include <asm/uaccess.h>
#include <linux/errno.h>  
#include <asm/segment.h>
#include <asm/current.h>
#include <linux/list.h>

#include "chat.h"

#define MY_DEVICE "chat"

MODULE_AUTHOR("Anonymous");
MODULE_LICENSE("GPL");

/* globals */
int my_major = 0; /* will hold the major # of my device driver */
struct timeval tv; /* Used to get the current time */
static LIST_HEAD(rooms); // used to save room heads

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
    
    //
    // do_init();
    //
    return 0;
}


void cleanup_module(void)
{
    // This function is called when removing the module using rmmod

    unregister_chrdev(my_major, MY_DEVICE);

    //
    // do clean_up();
    //
    return;
}

int my_open(struct inode *inode, struct file *filp)
{
    if(flip->private_data){ // duped
        return 0;
    } 
    // handle open
    room_data* data = get_room_data(inode->i_rdev);
    if(!data){
        return ENOMEM;
    }    
    filp->private_data = data;
    filp->f_pos = 0;
    return 0;
}


int my_release(struct inode *inode, struct file *filp)
{
    // handle file closing
    room_data* data = (room_data*)filp->private_data;
    data->open_cnt--;
    if(data->open_cnt){ //still open on by others
        return 0;
    } 

    // free all data 
    msg_list* p = data->mlist;
    msg_list* next;
    while(p){
        next = p->next;
        kfree(p->msg);
        kfree(p);
        p = next;
    }

    proc_list* p = data->plist;
    proc_list* next;
    while(p){
        next = p->next;
        kfree(p);
        p = next;
    }
    list_del(data->list);
    kfree(data);
    return 0;
}

ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    //
    // Do read operation.
    // Return number of bytes read.
    if(!buf){
        return -EFAULT;
    }

    int cur_msg = *f_pos / sizeof(message_t);
    room_data* room = (room_data*)flip->private_data;
    msg_list* pos = room->mlist;
    msg_list* list_end = room->ml_tail;

    message_t* user_buf = (message_t*)buf;
    int read_amount = count / sizeof(message_t);

    int idx = 0;
    while(pos != list_end && idx < cur_msg){
        pos = pos->next;
        idx++;
    }
    int count = 0;
    while(pos != list_end){
        if(copy_to_user(user_buf, &pos->msg, sizeof(message_t))){
            return -EBADF;
        }
        user_buf++;
        count++;
        pos = pos->next;
    }
    *f_pos += count * sizeof(message_t);
    return count;
}

ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *off){
    if(count > MAX_MESSAGE_LENGTH){
        return -ENOSPC;
    }
    if(!flip || !buf){
        return -EFAULT;
    }
    room_data* room = (room_data*)flip->private_data;
    if(!room){
        return -EFAULT;
    }
    msg_list* tail = data->ml_tail;
    if(copy_from_user(&tail->msg, buf, count)){
        return -EBADF;
    }
    tail->pid = getpid();
    tail->timestamp = gettime();
    tail->next = (msg_list*)kmalloc(sizeof(*tail->next));
    if(!tail->next){
        return ENOMEM;
    }

    tail = tail->next;
    return 0;
}


int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch(cmd)
    {
    case COUNT_UNREAD:
        return my_count_unread(filp);
	break;
    default:
	return -ENOTTY;
    }

    return 0;
}

int my_count_unread(struct file *filp){
    room_data* room = (room_data*)flip->private_data;
    msg_list* pos = room->mlist;
    msg_list* list_end = room->ml_tail;
    int cur_msg = filp->f_pos / sizeof(message_t);
    int idx = 0;
    while(pos != list_end && idx < cur_msg){
        pos = pos->next;
        idx++;
    }
    int count = 0;
    while(pos != list_end){
        pos = pos->next;
        count++;
    }
    return count;
}

loff_t my_llseek(struct file *filp, loff_t offset, int whence)
{
    // Change f_pos field in filp according to offset and whence.
    if(whence != 0){
        return -EINVAL;
    }
    int msg_num = offset / sizeof(message_t);
    msg_list* pos = ((room_data*)flip->private_data)->mlist;
    msg_list* list_end = ((room_data*)flip->private_data)->ml_tail;
    int idx = 0;
    while(pos != list_end && idx < msg_num){
        idx++;
        pos = pos->next;
    }
    flip->f_pos = idx * sizeof(message_t);

    return flip->f_pos;
}

time_t gettime() {
    do_gettimeofday(&tv);
    return tv.tv_sec;
}

pid_t getpid() {
    return current->pid;
}

// gets room data for given dev, creates new room if needed!
static struct room_data *get_room_data(dev_t dev) {
    room_data *data;
    struct list_head pos;
    // Iterate through the device list to find the existing room_data structure
    list_for_each(pos, &rooms) {
        data = list_entry(pos, room_data, list);
        if (data->dev == dev) {
            // room_data structure already exists for this device
            data->open_cnt++;
            return data;
        }
    }

    // room_data structure not found, create a new one
    data = (room_data*)kmalloc(sizeof(*data));
    if (!data) {
        return NULL;
    }
    data->dev = dev;
    data->open_cnt = 1; // we just opened
    // always create empty msg at the end of list
    data->mlist = (msg_list*)kmalloc(sizeof(*data->mlist)); 
    if(!data->mlist){
        kfree(data);
        return NULL; 
    }
    data->ml_tail = data->mlist;

    list_add(&data->list, &device_list);

    return data;
}


