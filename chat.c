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

    printk("new module\n\n\n\n\n");
    
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
    // if(filp->private_data){ // duped
    //     return 0;
    // } 
    // handle open
    room_data* data = (room_data*)get_room_data(inode->i_rdev);
    if(!data){
        return ENOMEM;
    }    
    filp->private_data = data;
    filp->f_pos = 0;
    MOD_INC_USE_COUNT;
    return 0;
}


int my_release(struct inode *inode, struct file *filp)
{
    printk("enter release\n");
    // handle file closing
    room_data* data = (room_data*)filp->private_data;
    if(!data){
        return -EINVAL;
    }
    printk("amount open: %d\n", data->open_cnt);
    data->open_cnt--;
    MOD_DEC_USE_COUNT;
    if(data->open_cnt){ //still open by others
        return 0;
    } 
    printk("before message free\n");
    // free all data 
    msg_list* p = data->mlist;
    msg_list* next;
    while(p){
        printk("deleting message: %s\n", p->msg.message);
        next = p->next;
        kfree(p);
        p = next;
    }
    printk("before list del\n");
    list_del(&data->list);
    kfree(data);
    return 0;
}

ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    printk("enter read\n");
    printk("size of message: %d\n", (int)sizeof(message_t));
    //
    // Do read operation.
    // Return number of bytes read.
    if(!buf){
        return -EFAULT;
    }

    int cur_msg = (int)*f_pos / (int)sizeof(message_t);
    room_data* room = (room_data*)filp->private_data;
    msg_list* pos = room->mlist;
    msg_list* list_end = room->ml_tail;
    printk("cur msg: %d\nfpos: %d\npos = list end: %d\n", cur_msg, (int)*f_pos, (pos == list_end));
    int read_amount = (int)count / (int)sizeof(message_t);

    int idx = 0;
    while(pos != list_end && idx < cur_msg){
        pos = pos->next;
        idx++;
    }
    printk("read before starting to copy\nread amount: %d\nidx: %d\n", read_amount, idx);
    int count_msg = 0;
    //try to see what up with list end and pos
    printk("is pos = list end: %d\n", pos == (list_end));
    while(pos != list_end && count_msg < read_amount){
        printk("reading message number %d\n", count_msg);
        if(copy_to_user(buf, &pos->msg, sizeof(message_t))){
            return -EBADF;
        }
        printk("the message: %s\n", ((message_t*)buf)->message);
        printk("pid: %d\n", ((message_t*)buf)->pid);
        printk("timestamp: %ld\n", ((message_t*)buf)->timestamp);
        ((message_t*)buf)++;
        count_msg++;
        pos = pos->next;
    }
    *f_pos += count_msg * sizeof(message_t);
    printk("leaving read, f_pos: %d\n", (int)*f_pos);
    return count_msg * sizeof(message_t);
}

ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *off){
    if(count > MAX_MESSAGE_LENGTH){
        return -ENOSPC;
    }
    if(!filp || !buf){
        return -EFAULT;
    }
    room_data* room = (room_data*)filp->private_data;
    if(!room){
        return -EFAULT;
    }
    msg_list* tail = room->ml_tail;

    tail->next = (msg_list*)kmalloc(sizeof(*tail->next), GFP_KERNEL);
    if(!tail->next){
        return ENOMEM;
    }
    memset(tail->msg.message, 0, MAX_MESSAGE_LENGTH);
    if(copy_from_user(&tail->msg.message, buf, count)){
        return -EBADF;
    }
    printk("just written message: %s\n", tail->msg.message);
    tail->msg.pid = getpid();
    tail->msg.timestamp = gettime();

    room->ml_tail = tail->next;
    room->ml_tail->next = NULL;
    memset(room->ml_tail->msg.message, 0, MAX_MESSAGE_LENGTH);
    //printk("leaving write\n");
    return count;
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
    room_data* room = (room_data*)filp->private_data;
    msg_list* pos = room->mlist;
    msg_list* list_end = room->ml_tail;
    int cur_msg = (int)filp->f_pos / (int)sizeof(message_t);
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
    int msg_num = (int)offset / (int)sizeof(message_t);
    msg_list* pos = ((room_data*)filp->private_data)->mlist;
    msg_list* list_end = ((room_data*)filp->private_data)->ml_tail;
    int idx = 0;
    while(pos != list_end && idx < msg_num){
        idx++;
        pos = pos->next;
    }
    filp->f_pos = idx * sizeof(message_t);

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
    list_for_each(pos, &rooms) {
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
    if(!data->mlist){
        kfree(data);
        return NULL; 
    }

    data->ml_tail = data->mlist;
    data->ml_tail->next = NULL;

    list_add(&data->list, &rooms);

    return data;
}
