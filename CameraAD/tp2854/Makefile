
TARGET = tp2854
obj-m := $(TARGET).o 

$(TARGET)-objs = tp2854b.o tp_proc.o

KERNEL_DIR := ../../linux-4.9.y/
#KERNEL_DIR := ../../kernel/linux-4.9.y/

PWD := $(shell pwd)
  
all:  
	make ARCH=arm CROSS_COMPILE=arm-himix200-linux- -C $(KERNEL_DIR) SUBDIRS=$(PWD) modules 
	arm-himix200-linux-gcc -o tp2854_test test.c

clean:  
	@make -C $(KERNEL_DIR) M=$(PWD) clean 
	@rm -rf tp2854_test

