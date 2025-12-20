# RosettaPad Debug Relay - Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -DENABLE_USB -DENABLE_BLUETOOTH
LDFLAGS = -pthread -lbluetooth

TARGET = debug-bt-relay

SRCS = common.c debug.c ds3.c usb_gadget.c bt_hid.c main.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean