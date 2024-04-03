CC = gcc
CFLAGS += -I/usr/src/linux-2.4.18-14/include -Wall
OBJS = chat.o

all: $(OBJS)
    
clean:
	rm -f *.o *~
