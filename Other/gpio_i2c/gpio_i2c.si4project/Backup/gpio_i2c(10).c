
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/fcntl.h>

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/workqueue.h>

#include <asm/uaccess.h>
//#include <asm/system.h>
#include <asm/io.h>
#include <mach/io.h>

#include <linux/gpio/driver.h>
#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/acpi.h>
#include <linux/platform_device.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/slab.h>


#include "gpio_i2c.h" 


#define GPIO_6_BASE 0x120D3000
#define SCL                 (1 << 2)    /* GPIO3_2 */
#define SDA                 (1 << 3)    /* GPIO3_3 */

#define GPIO_I2C_SCL_REG    0x10
#define GPIO_I2C_SDA_REG    0x20
#define GPIO_I2C_SCLSDA_REG 0x30

#define GPIO_6_DIR 0x400

#define HW_REG(reg)         *((volatile unsigned int *)(reg))

#define DELAY(us)           time_delay_us(us)


void __iomem *gpioBaseAddr;

/* 
 * I2C by GPIO simulated  clear 0 routine.
 *
 * @param whichline: GPIO control line
 *
 */
static void i2c_clr(unsigned char whichline)
{
	unsigned char regvalue;
	
	if(whichline == SCL)
	{
	    regvalue = readl(gpioBaseAddr + GPIO_6_DIR);
        regvalue |= SCL;
        writel(regvalue, gpioBaseAddr + GPIO_6_DIR);

        writel(0, gpioBaseAddr + GPIO_I2C_SCL_REG);
        //printk("clr scl read[%x]\n",readl(gpioBaseAddr + GPIO_I2C_SCL_REG));
		return;
	}
	else if(whichline == SDA)
	{
	    regvalue = readl(gpioBaseAddr + GPIO_6_DIR);
        regvalue |= SDA;
        writel(regvalue, gpioBaseAddr + GPIO_6_DIR);

        writel(0, gpioBaseAddr + GPIO_I2C_SDA_REG);
        //printk("clr sda read[%x]\n",readl(gpioBaseAddr + GPIO_I2C_SDA_REG));
		return;
	}
	else if(whichline == (SDA|SCL))
	{
	    regvalue = readl(gpioBaseAddr + GPIO_6_DIR); 

        regvalue |= (SDA|SCL);
        
        writel(regvalue, gpioBaseAddr + GPIO_6_DIR);

        writel(0, gpioBaseAddr + GPIO_I2C_SCLSDA_REG);
        //printk("clr sda+scl read[%x]\n",readl(gpioBaseAddr + GPIO_I2C_SCLSDA_REG));
        return;
	}
	else
	{
		printk("Error input.\n");
		return;
	}
}

/* 
 * I2C by GPIO simulated  set 1 routine.
 *
 * @param whichline: GPIO control line
 *
 */
static void  i2c_set(unsigned char whichline)
{
	unsigned char regvalue;
    
	if(whichline == SCL)
	{
    	regvalue = readl(gpioBaseAddr + GPIO_6_DIR);
        regvalue |= SCL;
        writel(regvalue, gpioBaseAddr + GPIO_6_DIR);
        
        writel(SCL, gpioBaseAddr + GPIO_I2C_SCL_REG);
        //printk("set scl read[%x]\n",readl(gpioBaseAddr + GPIO_I2C_SCL_REG));
		return;
	}
	else if(whichline == SDA)
	{
	    regvalue = readl(gpioBaseAddr + GPIO_6_DIR);
        regvalue |= SDA;
        writel(regvalue, gpioBaseAddr + GPIO_6_DIR);

        writel(SDA, gpioBaseAddr + GPIO_I2C_SDA_REG);
        //printk("set sda read[%x]\n",readl(gpioBaseAddr + GPIO_I2C_SDA_REG));
		return;
	}
	else if(whichline == (SDA|SCL))
	{
	    regvalue = readl(gpioBaseAddr + GPIO_6_DIR);
        
        regvalue |= (SDA|SCL);
        
        writel(regvalue, gpioBaseAddr + GPIO_6_DIR);
              
        writel(SDA|SCL, gpioBaseAddr + GPIO_I2C_SCLSDA_REG);
        //printk("set sda+scl read[%x]\n",readl(gpioBaseAddr + GPIO_I2C_SCLSDA_REG));
        return;
	}
	else
	{
		printk("Error input.\n");
		return;
	}

}

/*
 *  delays for a specified number of micro seconds rountine.
 *
 *  @param usec: number of micro seconds to pause for
 *
 */
void time_delay_us(unsigned int usec)
{
	udelay(10);	
}

/* 
 * I2C by GPIO simulated  read data routine.
 *
 * @return value: a bit for read 
 *
 */
 
static unsigned char i2c_data_read(void)
{
	unsigned char regvalue;

    regvalue = readl(gpioBaseAddr + GPIO_6_DIR);
    regvalue &= (~SDA);
    writel(regvalue, gpioBaseAddr + GPIO_6_DIR);
    
    DELAY(1);

    regvalue = readl(gpioBaseAddr + GPIO_I2C_SDA_REG);
    if((regvalue & SDA) != 0)
		return 1;
	else
		return 0;
}



/*
 * sends a start bit via I2C rountine.
 *
 */
static void i2c_start_bit(void)
{
        DELAY(1);
        i2c_set(SDA | SCL);
        DELAY(1);
        i2c_clr(SDA);
        DELAY(1);
}

/*
 * sends a stop bit via I2C rountine.
 *
 */
static void i2c_stop_bit(void)
{
        /* clock the ack */
        DELAY(1);
        i2c_set(SCL);
        DELAY(1); 
        i2c_clr(SCL);  

        /* actual stop bit */
        DELAY(1);
        i2c_clr(SDA);
        DELAY(1);
        i2c_set(SCL);
        DELAY(1);
        i2c_set(SDA);
        DELAY(1);
}

/*
 * sends a character over I2C rountine.
 *
 * @param  c: character to send
 *
 */
static void i2c_send_byte(unsigned char c)
{
    int i;
    local_irq_disable();
    for (i=0; i<8; i++)
    {
        DELAY(1);
        i2c_clr(SCL);
        DELAY(1);

        if (c & (1<<(7-i)))
            i2c_set(SDA);
        else
            i2c_clr(SDA);

        DELAY(1);
        i2c_set(SCL);
        DELAY(1);
        i2c_clr(SCL);
    }
    DELAY(1);
   // i2c_set(SDA);
    local_irq_enable();
}

/*  receives a character from I2C rountine.
 *
 *  @return value: character received
 *
 */
static unsigned char i2c_receive_byte(void)
{
    int j=0;
    int i;
    unsigned char regvalue;

    local_irq_disable();
    for (i=0; i<8; i++)
    {
        DELAY(1);
        i2c_clr(SCL);
        DELAY(1);
        i2c_set(SCL);

        regvalue = readl(gpioBaseAddr + GPIO_6_DIR);
        regvalue &= (~SDA);
        writel(regvalue, gpioBaseAddr + GPIO_6_DIR);
        
        DELAY(1);
        
        if (i2c_data_read())
            j+=(1<<(7-i));

        DELAY(1);
        i2c_clr(SCL);
    }
    local_irq_enable();
    DELAY(1);
   // i2c_clr(SDA);
   // DELAY(1);

    return j;
}

/*  receives an acknowledge from I2C rountine.
 *
 *  @return value: 0--Ack received; 1--Nack received
 *          
 */
static int i2c_receive_ack(void)
{
    int nack;
    unsigned char regvalue;
    
    DELAY(1);

    regvalue = readl(gpioBaseAddr + GPIO_6_DIR);
    regvalue &= (~SDA);
    writel(regvalue, gpioBaseAddr + GPIO_6_DIR);
    
    DELAY(1);
    i2c_clr(SCL);
    DELAY(1);
    i2c_set(SCL);
    DELAY(1);
    
    

    nack = i2c_data_read();

    DELAY(1);
    i2c_clr(SCL);
    DELAY(1);
  //  i2c_set(SDA);
  //  DELAY(1);

    if (nack == 0)
    {
        return 1; 
    }
    return 0;
}

#if 0
static void i2c_send_ack(void)
{
    DELAY(1);
    i2c_clr(SCL);
    DELAY(1);
    i2c_set(SDA);
    DELAY(1);
    i2c_set(SCL);
    DELAY(1);
    i2c_clr(SCL);
    DELAY(1);
    i2c_clr(SDA);
    DELAY(1);
}
#endif

EXPORT_SYMBOL(gpio_i2c_read);
unsigned char gpio_i2c_read(unsigned char devaddress, unsigned char address)
{
    int rxdata;
    
    i2c_start_bit();
    i2c_send_byte((unsigned char)(devaddress));
    i2c_receive_ack();
    
    i2c_send_byte(address);
    i2c_receive_ack(); 
   
    i2c_start_bit();
    i2c_send_byte((unsigned char)(devaddress) | 1);
    i2c_receive_ack();
    
    rxdata = i2c_receive_byte();
    //i2c_send_ack();
    i2c_stop_bit();
    return rxdata;
}


EXPORT_SYMBOL(gpio_i2c_write);
void gpio_i2c_write(unsigned char devaddress, unsigned char address, unsigned char data)
{      
    i2c_start_bit();

    i2c_send_byte((unsigned char)(devaddress));
    i2c_receive_ack();
    
    i2c_send_byte(address);
    i2c_receive_ack();
    
    i2c_send_byte(data); 
   // i2c_receive_ack();
    i2c_stop_bit();
}


EXPORT_SYMBOL(gpio_i2c2_read);
unsigned int gpio_i2c2_read(unsigned char devaddress, unsigned short address, int num_bytes)
{
    unsigned char rxdata;
	unsigned int  ret = 0x00;
    int i;
    unsigned short syncWord = 0xFCF3;
    unsigned short commandEntry = 0x37;//MEM READ
	
    i2c_start_bit();
    i2c_send_byte((unsigned char)(devaddress));
    i2c_receive_ack();

    for (i = 1; i >= 0; --i) {
        i2c_send_byte((unsigned char)((syncWord >> (i*8)) & 0xff)); 
        i2c_receive_ack();
    }

    i2c_send_byte((unsigned char)(commandEntry & 0xff));
    i2c_receive_ack();

    for (i = 1; i >= 0; --i) {
        i2c_send_byte((unsigned char)((address >> (i*8)) & 0xff));
        i2c_receive_ack();
    }

    i2c_start_bit();
    i2c_send_byte((unsigned char)(devaddress) | 1);
    i2c_receive_ack();

    for (i=0; i<num_bytes-1; i++) {
		rxdata = i2c_receive_byte();
		//i2c_send_ack();
		ret |= rxdata;
		ret <<= 8;
	}
    rxdata = i2c_receive_byte();
	//  i2c_send_ack();
    i2c_stop_bit();
	ret |= rxdata;
	
	printk("rd dev:%x, reg:%x, rxd:%x\n", devaddress, address, ret);
	
    return ret;
}


EXPORT_SYMBOL(gpio_i2c2_write);
void gpio_i2c2_write(unsigned char devaddress, unsigned short address,  unsigned int data, int num_bytes)
{
    //eg: Start devAddr(C0) syncWord(FCF3) commandEntry(3B) address(1E34) data(008B)
    
	int i ;
	unsigned short syncWord = 0xFCF3;
    unsigned short commandEntry = 0x3B;
    
	printk("wr: %x %x %x %x %x %x\n", devaddress, address, syncWord, commandEntry, data, num_bytes);

    i2c_start_bit();
    i2c_send_byte((unsigned char)(devaddress));
    i2c_receive_ack();   

    //Each command sequence starts with a sync word �?xFCF3�?
    for (i = 1; i >= 0; --i) {
		i2c_send_byte((unsigned char)((syncWord >> (i*8)) & 0xff)); 
		i2c_receive_ack();
	}

    i2c_send_byte((unsigned char)(commandEntry & 0xff));
    i2c_receive_ack();
  
    for (i = 1; i >= 0; --i) {
        i2c_send_byte((unsigned char)((address >> (i*8)) & 0xff));
        i2c_receive_ack();
    }
    
    
	for (i = 1; i >= 0; --i) {
		i2c_send_byte((unsigned char)((data >> (i*8)) & 0xff)); 
		i2c_receive_ack();
	}

    i2c_stop_bit();

}


long gpioi2c_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned char devAddress;

    unsigned short syncWord, commandEntry, address, data;
    
    SHI_Seq_S shiComSeq;

    memset(&shiComSeq, 0, sizeof(SHI_Seq_S));
    
    unsigned int __user *argp = (unsigned int __user *)arg; 

    if (copy_from_user(&shiComSeq, argp, sizeof(SHI_Seq_S)))
    {
        printk("--------copy_from_user err-----------\n");
        return -1;
    }
    
	switch(cmd)
	{
		case GPIO_I2C_READ_SHORT: 		
            devAddress = shiComSeq.devAddress;

            address = shiComSeq.address;
            
			unsigned short reg_val = gpio_i2c2_read(devAddress, address, 2);

            //printk("rd: %x %x %x \n", devAddress, address, reg_val);
            
            shiComSeq.data = reg_val;
                
			if (copy_to_user(argp, &shiComSeq, sizeof(SHI_Seq_S)))
                return -1;
            
			break;	
		case GPIO_I2C_WRITE_SHORT:  
            devAddress = shiComSeq.devAddress;
            
            address = shiComSeq.address;
        
            syncWord = shiComSeq.syncWord;
            
            commandEntry = shiComSeq.commandEntry;
            
            data = shiComSeq.data;

			gpio_i2c2_write(devAddress, address, data, 2);

            break;		
            
		default:
			return -1;
	}
    return 0;
}

int gpioi2c_open(struct inode * inode, struct file * file)
{
    return 0;
}
int gpioi2c_close(struct inode * inode, struct file * file)
{
    return 0;
}


static struct file_operations gpioi2c_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = gpioi2c_ioctl,
    .open           = gpioi2c_open,
    .release        = gpioi2c_close
};


static struct miscdevice gpioi2c_dev = {
   .minor		= MISC_DYNAMIC_MINOR,
   .name		= "gpio_i2c",
   .fops        = &gpioi2c_fops,
};

//管脚复用配置
#define iocfg_reg36 IO_ADDRESS(0x114F0048)
#define iocfg_reg37 IO_ADDRESS(0x114F004C)

static int __init gpio_i2c_init(void)
{
    int ret;		

    resource_size_t gpioBaseOffset = GPIO_6_BASE;
    resource_size_t gpioBaseSize = 0x100;    

    gpioBaseAddr = ioremap(gpioBaseOffset, gpioBaseSize);//获取GPIO3基地址 
    
	i2c_set(SCL | SDA);	    

    ret = misc_register(&gpioi2c_dev);
    if(0 != ret)
    {
    	return -1;
    }

    printk(KERN_INFO "[kernel] gpio_i2c_init suc\n");
    

    return 0;    
}

static void __exit gpio_i2c_exit(void)
{
    misc_deregister(&gpioi2c_dev);
    
    iounmap(gpioBaseAddr);
}


module_init(gpio_i2c_init);
module_exit(gpio_i2c_exit);

#ifdef MODULE
//#include <linux/compile.h>
#endif
//MODULE_INFO(build, UTS_VERSION);
MODULE_LICENSE("GPL");




