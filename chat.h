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

int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);

loff_t my_llseek(struct file *, loff_t, int);

struct message_t {
    pid_t pid;
    time_t timestamp;
    char message[MAX_MESSAGE_LENGTH];
};
#endif
