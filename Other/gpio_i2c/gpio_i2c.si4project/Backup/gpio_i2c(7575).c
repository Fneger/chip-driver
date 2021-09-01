
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/fcntl.h>

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/workqueue.h>

#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>

#include "gpio_i2c.h" 

#define GPIO_0_BASE 0x121A0000
#define SCL                 (1 << 1)    /* GPIO5_1 */
#define SDA                 (1 << 2)    /* GPIO5_2 */
#define GPIO_I2C_SCL_REG    IO_ADDRESS(GPIO_0_BASE + 0x008)
#define GPIO_I2C_SDA_REG    IO_ADDRESS(GPIO_0_BASE + 0x010)
#define GPIO_I2C_SCLSDA_REG IO_ADDRESS(GPIO_0_BASE + 0x018)

#define GPIO_0_DIR IO_ADDRESS(GPIO_0_BASE + 0x400)

#define HW_REG(reg)         *((volatile unsigned int *)(reg))
#define DELAY(us)           time_delay_us(us)

#define SENSOR_I2C_W_ADDR 0xfe
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
		regvalue = HW_REG(GPIO_0_DIR);
		regvalue |= SCL;
		HW_REG(GPIO_0_DIR) = regvalue;
		
		HW_REG(GPIO_I2C_SCL_REG) = 0;
		return;
	}
	else if(whichline == SDA)
	{
		regvalue = HW_REG(GPIO_0_DIR);
		regvalue |= SDA;
		HW_REG(GPIO_0_DIR) = regvalue;
		
		HW_REG(GPIO_I2C_SDA_REG) = 0;
		return;
	}
	else if(whichline == (SDA|SCL))
	{
		regvalue = HW_REG(GPIO_0_DIR);
		regvalue |= (SDA|SCL);
		HW_REG(GPIO_0_DIR) = regvalue;
		
		HW_REG(GPIO_I2C_SCLSDA_REG) = 0;
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
		regvalue = HW_REG(GPIO_0_DIR);
		regvalue |= SCL;
		HW_REG(GPIO_0_DIR) = regvalue;
		
		HW_REG(GPIO_I2C_SCL_REG) = SCL;
		return;
	}
	else if(whichline == SDA)
	{
		regvalue = HW_REG(GPIO_0_DIR);
		regvalue |= SDA;
		HW_REG(GPIO_0_DIR) = regvalue;
		
		HW_REG(GPIO_I2C_SDA_REG) = SDA;
		return;
	}
	else if(whichline == (SDA|SCL))
	{
		regvalue = HW_REG(GPIO_0_DIR);
		regvalue |= (SDA|SCL);
		HW_REG(GPIO_0_DIR) = regvalue;
		
		HW_REG(GPIO_I2C_SCLSDA_REG) = (SDA|SCL);
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
	//int i,j;
	
	udelay(usec);
	
	/*
	for(i=0;i<usec * 50;i++)
	{
		for(j=0;j<4700;j++)
		{;}
	}*/
}

/* 
 * I2C by GPIO simulated  read data routine.
 *
 * @return value: a bit for read 
 * return 1:high level
 * return 0:low level
 */
 
static unsigned char i2c_data_read(void)
{
	unsigned char regvalue;
	
	regvalue = HW_REG(GPIO_0_DIR);
	regvalue &= (~SDA);
	HW_REG(GPIO_0_DIR) = regvalue;
	DELAY(1);
		
	regvalue = HW_REG(GPIO_I2C_SDA_REG);
	if((regvalue&SDA) != 0)//high voltage
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
        
        regvalue = HW_REG(GPIO_0_DIR);
        regvalue &= (~SDA);
        HW_REG(GPIO_0_DIR) = regvalue;
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
 *  @return value: 1--Ack received; 0--Nack received
 *          
 */
static int i2c_receive_ack(void)
{
    int nack;
    unsigned char regvalue;
    
    DELAY(1);

	
    local_irq_disable();
    regvalue = HW_REG(GPIO_0_DIR);
    regvalue &= (~SDA);
    HW_REG(GPIO_0_DIR) = regvalue;
    
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
	
  	local_irq_enable();
    if (nack == 0)
        return 1; 

    return 0;
}


static void i2c_send_ack(void)
{
    DELAY(1);
	
    local_irq_disable();
	
    i2c_clr(SCL);
    i2c_clr(SDA);
	DELAY(1);
	i2c_set(SCL);
	DELAY(5);
	i2c_clr(SCL);
	i2c_set(SDA);
  	DELAY(1);
   /*
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
    DELAY(1);*/
	
  	local_irq_enable();
}

static void i2c_send_nack(void)
{
    DELAY(1);
	
    local_irq_disable();
	
    i2c_clr(SCL);
    DELAY(1);
    i2c_set(SDA);
	DELAY(1);
	i2c_set(SCL);
	DELAY(1);
	i2c_clr(SCL);

	
  	local_irq_enable();
}


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
   // i2c_receive_ack();//add by hyping for tw2815
    i2c_stop_bit();
}


EXPORT_SYMBOL(gpio_i2c2_read);
unsigned int gpio_i2c2_read(unsigned char devaddress, unsigned short address, int num_bytes)
{
    unsigned char rxdata;
	unsigned int  ret = 0x00;
    int i;
	
#if 1
	for (i=0; i<num_bytes; i++) {
		i2c_start_bit();
		i2c_send_byte((unsigned char)(devaddress));
		i2c_receive_ack();
		i2c_send_byte((unsigned char)((address >> 8) & 0xff));
		i2c_receive_ack();
		i2c_send_byte((unsigned char)(address & 0xff));
		i2c_receive_ack();   
		
		i2c_start_bit();
		i2c_send_byte((unsigned char)(devaddress) | 1);
		i2c_receive_ack();
		rxdata = i2c_receive_byte();
		//		i2c_send_nack();
		i2c_stop_bit();
		
		ret |= (rxdata << (i * 8));
		
		address ++;
	}
#else
    i2c_start_bit();
    i2c_send_byte((unsigned char)(devaddress));
    i2c_receive_ack();
    i2c_send_byte((unsigned char)((address >> 8) & 0xff));
    i2c_receive_ack();
    i2c_send_byte((unsigned char)(address & 0xff));
    i2c_receive_ack();   
	
    i2c_start_bit();
    i2c_send_byte((unsigned char)(devaddress) | 1);
    i2c_receive_ack();
	for (i=0; i<num_bytes-1; i++) {
		rxdata = i2c_receive_byte();
		i2c_send_ack();
		ret |= rxdata;
		ret <<= 8;
	}
    rxdata = i2c_receive_byte();
	//  i2c_send_ack();
    i2c_stop_bit(ACK);
	ret |= rxdata;
#endif
	
	//	printk("dev=%x, reg =%x, rxd=%x\n", devaddress, address, ret);
	
    return ret;
}


EXPORT_SYMBOL(gpio_i2c2_write);
void gpio_i2c2_write(unsigned char devaddress, unsigned short address, unsigned int data, int num_bytes)
{
	int i;
	
	//	printk("wr: %x %x %x %x\n", devaddress, address, data, num_bytes);
#if 1
	for (i=0; i<num_bytes; i++) {
		i2c_start_bit();
		i2c_send_byte((unsigned char)(devaddress));
		i2c_receive_ack();
		i2c_send_byte((unsigned char)((address >> 8) & 0xff));
		i2c_receive_ack();
		i2c_send_byte((unsigned char)(address & 0xff));
		i2c_receive_ack();   
		
		i2c_send_byte((unsigned char)((data >> (i*8)) & 0xff)); 
		i2c_receive_ack();
		i2c_stop_bit();
		address++;
		//	DELAY(100);
	}
#else
    i2c_start_bit();
    i2c_send_byte((unsigned char)(devaddress));
    i2c_receive_ack();
    i2c_send_byte((unsigned char)((address >> 8) & 0xff));
    i2c_receive_ack();
    i2c_send_byte((unsigned char)(address & 0xff));
    i2c_receive_ack();
	for (i=0; i<num_bytes; i++) {
		i2c_send_byte((unsigned char)((data >> (i*8)) & 0xff)); 
		i2c_receive_ack();
	}
	//	i2c_send_byte((unsigned char)((data >> (i*8)) & 0xff)); 
    i2c_stop_bit(ACK);
#endif
}
//在发布版本时注释

#if 1
void RegMaskWrite(unsigned char chip_addr, unsigned short addr, int lo_bit, int hi_bit, int data)
{
	int i, num_bytes;
	unsigned int value = 0;
	
	num_bytes = 4;
	
	value = gpio_i2c2_read(chip_addr, addr, num_bytes);

	for (i=lo_bit; i<=hi_bit; i++) {
		value &= ~(1<<i);
	}
	
	value |= (data << lo_bit);

	gpio_i2c2_write(chip_addr, addr, value, num_bytes);
}

#endif

unsigned int gpio_i2c2_read_1bytesubaddr(unsigned char devaddress, unsigned char address, int num_bytes)
{
    unsigned int value = 0;
    value = gpio_i2c2_read(devaddress, address, num_bytes);

    return value;
    
}

void gpio_i2c2_write_1bytesubaddr(unsigned char devaddress, unsigned char address, unsigned int data, int num_bytes)
{
    gpio_i2c2_write(devaddress, address, data, num_bytes);

}

//add for sensor
void i2c_start(u8 deviceId)
{
	int ack;
	i2c_start_bit();
    i2c_send_byte((unsigned char)(deviceId));
    ack = i2c_receive_ack();
}

char i2c_read_byte(unsigned char *p_byte, char ack)
{
	*p_byte = i2c_receive_byte();

	if (ack == 1)
		i2c_send_ack();
	else
		i2c_send_nack();
		
	return 0;
}
char i2c_write_byte(unsigned char c, char send_ackclk, char wait_ack)
{
	int ack;
	i2c_send_byte(c);

	if (send_ackclk == 1)
	{
    	ack = i2c_receive_ack();
	}
	return 0;
}

//此函数实现：地址是 16bit，数据是8bit
//bank是16bit地址高8位，reg是低8位
EXPORT_SYMBOL(i2c_read_reg);
unsigned char i2c_read_reg(unsigned char bank, unsigned char reg  )
{
    unsigned char read_data, check_sum, tmp;
    
    //step1 write
    i2c_start( SENSOR_I2C_W_ADDR );
    i2c_write_byte( 0x10,  1, 1 );//index MSB
    i2c_write_byte( 0x00,  1, 1 );//index LSB
    
    //package
    i2c_write_byte( 0x06,  1, 1 );//len
    i2c_write_byte( 0x01,  1, 1 );//cmd, 0x01:read
    i2c_write_byte( 0x01,  1, 1 );//data len
    i2c_write_byte( bank,  1, 1 );//reg MSB
    i2c_write_byte( reg,  1, 1 );//reg LSB
    check_sum = 0x06+0x01+0x01+bank+reg; //+dat;
    i2c_write_byte( check_sum,  1, 1 );//
    
    i2c_stop_bit();
    
    //step2 interrupt
    i2c_start( SENSOR_I2C_W_ADDR );
    i2c_write_byte( 0x10,  1, 1 );//index MSB
    i2c_write_byte( 0x80,  1, 1 );//index LSB
    i2c_write_byte( 0xFF,  1, 1 );
    i2c_stop_bit();
    
    mdelay(30);
    //step3 read result
    i2c_start( SENSOR_I2C_W_ADDR );
    i2c_write_byte( 0x10,  1, 1 );//index MSB
    i2c_write_byte( 0x00,  1, 1 );//index LSB
    
    i2c_start( SENSOR_I2C_W_ADDR+1 );
    i2c_read_byte( &tmp, 1 );//read len, no use
    i2c_read_byte( &tmp, 1 );//0xF1, read sucess flag, no use
    i2c_read_byte( &read_data, 1 );//read data
    i2c_read_byte( &tmp, 0 );//read sum, no use
    
    //printk("read_data =0x%x\n",read_data);
    
    
    i2c_stop_bit();
    
    return read_data;
}

//此函数实现：地址是 16bit，数据是8bit
//bank是16bit地址高8位，reg是低8位
EXPORT_SYMBOL(i2c_write_reg);

void i2c_write_reg( unsigned char bank, unsigned char reg, unsigned char dat )
{
    unsigned char check_sum, tmp1, tmp2, tmp3;
    
    //step1 write
    i2c_start( SENSOR_I2C_W_ADDR );
    i2c_write_byte( 0x10,  1, 1 );//index MSB
    i2c_write_byte( 0x00,  1, 1 );//index LSB
    
    //package
    i2c_write_byte( 0x07,  1, 1 );//len
    i2c_write_byte( 0x02,  1, 1 );//cmd, 0x02:write
    i2c_write_byte( 0x01,  1, 1 );//data len
    i2c_write_byte( bank,  1, 1 );//reg MSB
    i2c_write_byte( reg,  1, 1 );//reg LSB
    i2c_write_byte( dat,  1, 1 );//dat
    check_sum = 0x07+0x02+0x01+bank+reg+dat;
    i2c_write_byte( check_sum,  1, 1 );//
    
    i2c_stop_bit();
    
    //step2 interrupt
    i2c_start( SENSOR_I2C_W_ADDR );
    i2c_write_byte( 0x10,  1, 1 );//index MSB
    i2c_write_byte( 0x80,  1, 1 );//index LSB
    i2c_write_byte( 0xFF,  1, 1 );
    i2c_stop_bit();
    
     mdelay(3);
    //step3 read result
    i2c_start( SENSOR_I2C_W_ADDR );
    i2c_write_byte( 0x10,  1, 1 );//index MSB
    i2c_write_byte( 0x00,  1, 1 );//index LSB
    
    i2c_start( SENSOR_I2C_W_ADDR+1 );
    i2c_read_byte( &tmp1, 1 );//read len, no use
    
    i2c_read_byte( &tmp2, 1 );//read reposne code, no use
    
    i2c_read_byte( &tmp3, 0 );//read sum, no use
    //printk("write reg response code =0x%x\n",tmp2);
    i2c_stop_bit();
}

//end

long gpioi2c_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    unsigned int val;
	
	char device_addr;
	int reg_val,reg_addr;
	cx25838_regs_data val1;
	unsigned char chip_addr;
	int addr;
	int lo_bit;
	int hi_bit;
	int data;
	
	
	switch(cmd)
	{
		case GPIO_I2C_READ_BYTE:
			val = *(unsigned int *)arg;
			device_addr = (val&0xff000000)>>24;
			reg_addr = (val&0xff0000)>>16;
			
			reg_val = gpio_i2c_read(device_addr, reg_addr);
			*(unsigned int *)arg = (val&0xffff0000)|reg_val;	
            //printk("Func:%s, line:%d  dev_addr=0x%x, reg_addr=0x%x, value=0x%x\n", 
            //    __FUNCTION__, __LINE__, device_addr, reg_addr, reg_val);
			break;
		
		case GPIO_I2C_WRITE_BYTE:
			val = *(unsigned int *)arg;
			device_addr = (val&0xff000000)>>24;
			reg_addr = (val&0xff0000)>>16;
			
			reg_val = val&0xffff;
			gpio_i2c_write(device_addr, reg_addr, reg_val);
			break;	
			
		case GPIO_I2C_READ_DWORD:
			val = *(unsigned int *)arg;
			device_addr = (val&0xff000000)>>24;
			reg_addr = (val&0xffff00)>>8;
			
			reg_val = gpio_i2c2_read(device_addr, reg_addr, 4);
			*(unsigned int *)arg = reg_val;			
			//printk("Func:%s, line:%d  dev_addr=0x%x, reg_addr=0x%x, value=0x%x\n", 
            //    __FUNCTION__, __LINE__, device_addr, reg_addr, reg_val);
			break;
		
		case GPIO_I2C_WRITE_DWORD:
			val1 = *(cx25838_regs_data *)arg;

			chip_addr=val1.chip&0xff;
			addr=val1.addr;
			lo_bit=val1.lobit;
			hi_bit=val1.hibit;
			data=val1.data;
			//printk("Func:%s, line:%d  dev_addr=0x%x, reg_addr=0x%x, lo_bit=0x%x, hi_bit=0x%x, data=0x%x\n", 
			//__FUNCTION__, __LINE__, chip_addr, addr, lo_bit, hi_bit, data);
            
			RegMaskWrite(chip_addr, addr, lo_bit, hi_bit, data);
			break;
		case GPIO_I2C_READ_REG:
			
            //printk("start i2c_read_reg\n");
			val = *(unsigned int *)arg;
			device_addr = (val&0xff000000)>>24;
			reg_addr = (val&0xff0000)>>16;
			reg_val = i2c_read_reg(device_addr,reg_addr);
			*(unsigned int *)arg = (val&0xffff0000)|reg_val;	
            //printk("Func:%s,line:%d  bank=0x%x,reg_addr=0x%x,data=0x%x\n",__FUNCTION__, __LINE__,device_addr,reg_addr,reg_val);
			break;
		case GPIO_I2C_WRITE_REG:
            //printk("start i2c_write_reg\n");
			val = *(unsigned int *)arg;
			device_addr = (val&0xff000000)>>24;
			reg_addr = (val&0xff0000)>>16;
			
			reg_val = val&0xffff;
			
			i2c_write_reg( device_addr,reg_addr, reg_val);
            //printk("Func:%s,line:%d  bank=0x%x,reg_addr=0x%x,data=0x%x\n",__FUNCTION__, __LINE__,device_addr,reg_addr,reg_val);
			
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
    //.ioctl          = gpioi2c_ioctl,
    .unlocked_ioctl = gpioi2c_ioctl,
    .open           = gpioi2c_open,
    .release        = gpioi2c_close
};


static struct miscdevice gpioi2c_dev = {
   .minor		= MISC_DYNAMIC_MINOR,
   .name		= "gpioi2c",
   .fops        = &gpioi2c_fops,
};

static int __init gpio_i2c_init(void)
{
    int ret;
    unsigned int reg;

	//管脚复用配置
#define muxctrl_reg56 IO_ADDRESS(0x120f00e0)
#define muxctrl_reg57 IO_ADDRESS(0x120f00e4)
	
	printk(KERN_INFO "gpio_i2c_init\n");		
	//配置为GPIO模式
	reg = HW_REG(muxctrl_reg56);
	reg &= 0xfffffffe;
	HW_REG(muxctrl_reg56) = reg;

	reg = HW_REG(muxctrl_reg57);
	reg &= 0xfffffffe;
	HW_REG(muxctrl_reg57) = reg;
	
	i2c_set(SCL | SDA);	  
	
    ret = misc_register(&gpioi2c_dev);
    if(0 != ret)
    {
    	return -1;
    }

    return 0;    
}

static void __exit gpio_i2c_exit(void)
{
    misc_deregister(&gpioi2c_dev);
}


module_init(gpio_i2c_init);
module_exit(gpio_i2c_exit);

#ifdef MODULE
//#include <linux/compile.h>
#endif
//MODULE_INFO(build, UTS_VERSION);
MODULE_LICENSE("GPL");




