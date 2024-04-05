#!/usr/bin/python

from __future__ import division
from struct import *
import fcntl
import struct
import os

#
# Globals
#
DEVICE_PATH = '/dev/chat'
MAX_MESSAGE_LENGTH = 100
SEEK_SET = 0
SEEK_CUR = 1
SEEK_END = 2

#
# Utilities for calculating the IOCTL command codes.
#
sizeof = {
    'byte': calcsize('c'),
    'signed byte': calcsize('b'),
    'unsigned byte': calcsize('B'),
    'short': calcsize('h'),
    'unsigned short': calcsize('H'),
    'int': calcsize('i'),
    'unsigned int': calcsize('I'),
    'long': calcsize('l'),
    'unsigned long': calcsize('L'),
    'long long': calcsize('q'),
    'unsigned long long': calcsize('Q')
}

_IOC_NRBITS = 8
_IOC_TYPEBITS = 8
_IOC_SIZEBITS = 14
_IOC_DIRBITS = 2

_IOC_NRMASK = ((1 << _IOC_NRBITS)-1)
_IOC_TYPEMASK = ((1 << _IOC_TYPEBITS)-1)
_IOC_SIZEMASK = ((1 << _IOC_SIZEBITS)-1)
_IOC_DIRMASK = ((1 << _IOC_DIRBITS)-1)

_IOC_NRSHIFT = 0
_IOC_TYPESHIFT = (_IOC_NRSHIFT+_IOC_NRBITS)
_IOC_SIZESHIFT = (_IOC_TYPESHIFT+_IOC_TYPEBITS)
_IOC_DIRSHIFT = (_IOC_SIZESHIFT+_IOC_SIZEBITS)

_IOC_NONE = 0
_IOC_WRITE = 1
_IOC_READ = 2

def _IOC(dir, _type, nr, size):
    if type(_type) == str:
        _type = ord(_type)
        
    cmd_number = (((dir)  << _IOC_DIRSHIFT) | \
        ((_type) << _IOC_TYPESHIFT) | \
        ((nr)   << _IOC_NRSHIFT) | \
        ((size) << _IOC_SIZESHIFT))

    return cmd_number

def _IO(_type, nr):
    return _IOC(_IOC_NONE, _type, nr, 0)

def _IOR(_type, nr, size):
    return _IOC(_IOC_READ, _type, nr, sizeof[size])

def _IOW(_type, nr, size):
    return _IOC(_IOC_WRITE, _type, nr, sizeof[size])


def main():
    """Test the device driver"""
    
    #
    # Calculate the ioctl cmd number
    #
    MY_MAGIC = 'r'
    COUNT_UNREAD = _IO(MY_MAGIC, 0)
    SEARCH = _IO(MY_MAGIC, 1)
       
    # Open the device file
    f = os.open(DEVICE_PATH, os.O_RDWR)
    
    # Test write operation
    write_msg = "test"
    write_msg2 = "testhelloworld"
    write_msg3 = "testnumbertres"
    ret = os.write(f, write_msg)
    ret = os.write(f, write_msg2)
    ret = os.write(f, write_msg3)
    ret = os.write(f, write_msg)
    assert (ret == len(write_msg))
    
    print("after write")
    # Read the message we wrote
    message_t_format = 'Hl%ds' % MAX_MESSAGE_LENGTH
    message_t_size = struct.calcsize(message_t_format)
    message_t = os.read(f, 2*message_t_size)
    message_t = os.read(f, 1*message_t_size)
    pid, timestamp, read_msg = struct.unpack(message_t_format, message_t)
    read_msg = read_msg.split('\0', 1)[0] # Remove NULL padding
    assert (read_msg == write_msg3)
    print("after read")
    # We should not have any more unread messages since we read them all
    assert (fcntl.ioctl(f, COUNT_UNREAD) == 1)
    print("after first count")
    for i in range(300000):
        ret = os.write(f, write_msg)

    os.lseek(f, 1*message_t_size, 0)
    assert (fcntl.ioctl(f, COUNT_UNREAD) ==  300003)
    ret = os.write(f, write_msg)
    assert (fcntl.ioctl(f, COUNT_UNREAD) == 300004)
    message_t = os.read(f, 2*message_t_size)
    assert (fcntl.ioctl(f, COUNT_UNREAD) == 300002)
    for i in range(300000):
        os.read(f, 1*message_t_size)
    assert (fcntl.ioctl(f, COUNT_UNREAD) == 2)
    

    # Seek backwards one step
    #assert (os.lseek(f, -1*message_t_size, SEEK_CUR) == 0)

    # Search for our pid, we should find it at offset 0
    #mypid = os.getpid()
    #assert (fcntl.ioctl(f, SEARCH, mypid) == 0)

    # Finaly close the device file
    os.close(f)

    
if __name__ == '__main__':
    main()
    
