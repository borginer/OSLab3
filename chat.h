#ifndef _CHAT_H_
#define _CHAT_H_

#include <linux/ioctl.h>
#include <linux/types.h>

#define MY_MAGIC 'r'
#define COUNT_UNREAD _IO(MY_MAGIC, 0)

#define MAX_MESSAGE_LENGTH 100

//
// Function prototypes
//
int my_open(struct inode *, struct file *);

int my_release(struct inode *, struct file *);

ssize_t my_read(struct file *, char *, size_t, loff_t *);

ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);

int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);

loff_t my_llseek(struct file *, loff_t, int);

int my_count_unread(struct file *flip); 




struct message_t {
    pid_t pid;
    time_t timestamp;
    char message[MAX_MESSAGE_LENGTH];
};

typedef struct message_t message_t;

typedef struct {
    msg_list* mlist;
    msg_list* ml_tail;
    int open_cnt;
    kdev_t dev;
    struct list_head list;
} room_data;

typedef struct {
    msg_list* next;
    message_t msg;
} msg_list;

typedef struct {
    room_data* room;
    msg_list* cur;
} file_data;

#endif
