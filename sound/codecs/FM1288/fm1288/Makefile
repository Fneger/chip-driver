export ARCH=arm
export CROSS_COMPILE=arm-himix200-linux-

obj-m := fm1288.o
FM1288-y += fm1288.o

EXTRA_CFLAGS += -I$(PWD)/

LINUX_ROOT = $(PWD)/../../linux-4.9.y
 
default:	
	@make -C $(LINUX_ROOT) M=$(PWD) modules
	chmod 777 *.ko
	
clean:
	@rm *.o *.ko -rf
	@make -C $(LINUX_ROOT) M=$(PWD) clean
