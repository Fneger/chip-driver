/*
 * tp2854b.c
 */
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include <linux/list.h>
#include <asm/delay.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/poll.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/i2c.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/nvmem-provider.h>
#include <linux/uaccess.h>
#include <linux/i2c-dev.h>
#include <linux/mutex.h>

#include "tp2854b_def.h"
#include "tp2854b.h"
#include "tp_proc.h"

#define DEBUG_LEVEL 	1
#define DEV_NAME 		"tp2854"

#define DPRINTK(level, fmt,args...)	do { if(level < DEBUG_LEVEL)\
									  printk(KERN_INFO "%s [%s ,%d]: " fmt "\n",DEV_NAME,__FUNCTION__,__LINE__,##args);\
									}while(0)
									

#define HI3516 		1	// 1:3519 0:3516

#define VIN3_4 		0	// 1:vin3 vin4  0:vin1 vin2

#define TP2854B_MAJOR         	151
#define SENSOR_POWER_GPIO    	3

//0x88>>1: 0x44
#define TP2854B_I2C_ADDR 		0x44

#define TP2850_VERSION_CODE KERNEL_VERSION(0, 0, 1)

#define	TP285X_MAX_CHN			4

static unsigned int s_tp285x_bright[TP285X_MAX_CHN] 	= {0};
static unsigned int s_tp285x_contrast[TP285X_MAX_CHN] 	= {0x40, 0x40, 0x40, 0x40};
static unsigned int s_tp285x_saturation[TP285X_MAX_CHN] = {0x40, 0x40, 0x40, 0x40};
static unsigned int s_tp285x_hue[TP285X_MAX_CHN] 		= {0};
static unsigned int s_tp285x_sharpness[TP285X_MAX_CHN] 	= {0};

struct tp285x_data *tp2854;

static struct i2c_client* tp2854b_client;
struct mutex tp2854_CamRes_mutex;

static int setup_dev_int(struct i2c_client *client);

static struct i2c_board_info tp2854b_info =
{
    I2C_BOARD_INFO("tp2854b", TP2854B_I2C_ADDR),
};

/* Read 8-bit value from register */
static unsigned char tp285x_read(struct i2c_client *client, unsigned char reg)
{
	int val;
	
	val = i2c_smbus_read_byte_data(client, reg);
	if (val < 0)
	{
		dev_err(&client->dev, "%d-bit %s failed at 0x%02x\n", 8, "read", reg);
		return 0x00;	/* Arbitrary value */
	}
	
	return val;
}

/* Write 8-bit value to register */
static int tp285x_write(struct i2c_client *client, unsigned char reg, unsigned char val)
{
	int err;
	
	err = i2c_smbus_write_byte_data(client, reg, val);
	if (err < 0)
	{
		dev_err(&client->dev, "%d-bit %s failed at 0x%02x\n", 8, "write", reg);
	}
	
	return err;
}

int proc_i2c_read(struct i2c_client *client, unsigned char reg)
{
	return tp285x_read(client, reg);
}

int proc_i2c_write(struct i2c_client *client, unsigned char reg, unsigned char value)
{
	return  tp285x_write(client, reg, value);
}

static void tp2850_set_reg_page(struct i2c_client *client, unsigned char ch)
{
    switch(ch)
    {
    case MIPI_PAGE:
        tp285x_write(client, 0x40, 0x08);
        break;
		
    default:
        tp285x_write(client, 0x40, 0x00);
        break;
    }
}

static void TP285x_reset_default(struct i2c_client *client)
{
    unsigned int tmp;
	
    tp285x_write(client, 0x40, 0x08);
    printk("40 [res 0x08]: des [ %x]\n", tp285x_read(client, 0x40) );

    tp285x_write(client, 0x23, 0x02);
    printk("0x23 [res 0x02]: des [ %x]\n", tp285x_read(client, 0x23) );

    tp285x_write(client, 0x40, 0x00);
    printk("0x40 [res 0x0]: des [ %x]\n", tp285x_read(client, 0x40) );

    tp285x_write(client, 0x07, 0xC0);
    printk("0x07 [res 0xc0]: des [ %x]\n", tp285x_read(client, 0x07) );

    tp285x_write(client, 0x0B, 0xC0);
    printk("0x0b [res 0xc0]: des [ %x]\n", tp285x_read(client, 0x0b) );

    tmp = tp285x_read(client, 0x26);
    tmp &= 0xfe;
    tp285x_write(client, 0x26, tmp);
    printk("0x26 [res %x]: des [ %x]\n",tmp, tp285x_read(client, 0x26) );

#if 0
    printk("0x25 [res %x]: des [ %x]\n",0x4, tp285x_read(client, 0x25) );
    tp285x_write(client, 0x25, 0xff);
    printk("0x25 [res %x]: des [ %x]\n",0x4, tp285x_read(client, 0x25) );

    printk("0x27 [res %x]: des [ %x]\n",0x3, tp285x_read(client, 0x27) );
    tp285x_write(client, 0x27, 0xff);
    printk("0x27 [res %x]: des [ %x]\n",0x4, tp285x_read(client, 0x25) );
#endif
    tmp = tp285x_read(client, 0xa7);
    tmp &= 0xfe;
    tp285x_write(client, 0xa7, tmp);
    printk("0xa7 [res %x]: des [ %x]\n",tmp, tp285x_read(client, 0xa7) );

    tmp = tp285x_read(client, 0x06);
    tmp &= 0xfb;
    tp285x_write(client, 0x06, tmp);
    printk("0x06 [res %x]: des [ %x]\n",tmp, tp285x_read(client, 0x06) );
}

struct tp285x_data
{
	struct i2c_client *client;
	struct cdev cdev;
	struct device *dev;
	struct class *class;
    char  name[20];
	int   open_count;
    int   mode;
    int   num;
}tp285x_data;

static const struct i2c_device_id tp2850_id[] = 
{
    { "tp2854", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, tp2850_id);

/* tp2852B_channel_setting */

/////////////////////////////////
//ch: video channel
//fmt: PAL/NTSC/HD25/HD30
//std: STD_TVI/STD_HDA
////////////////////////////////
void TP2854_decoder_init(struct i2c_client *client, unsigned char ch, unsigned char fmt, unsigned char std)
{
	unsigned char tmp;
	const unsigned char SYS_MODE[5] = {0x01, 0x02, 0x04, 0x08, 0x0f}; 
	
	tp285x_write(client, 0x40, ch);
	//tp285x_write(client, 0x45, 0x01);	
	//tp285x_write(client, 0x06, 0x32);

	printk("TP2854_decoder_init in ch=%d, fmt=%d, std=%d",ch, fmt, std);
	
	if (PAL == fmt)
	{
		printk(" PAL in .. ");
		tmp = tp285x_read(client,0xf5);
		tmp |= SYS_MODE[ch];
		tp285x_write(client, 0xf5, tmp);
		
		tp285x_write(client, 0x02, 0x47);
		tp285x_write(client, 0x0c, 0x13); 
		tp285x_write(client, 0x0d, 0x51);  

		tp285x_write(client, 0x15, 0x13);
		tp285x_write(client, 0x16, 0x76); 
		tp285x_write(client, 0x17, 0x80); 
		tp285x_write(client, 0x18, 0x17);
		tp285x_write(client, 0x19, 0x20);
		tp285x_write(client, 0x1a, 0x17);				
		tp285x_write(client, 0x1c, 0x09);
		tp285x_write(client, 0x1d, 0x48);
	
		tp285x_write(client, 0x20, 0x48);  
		tp285x_write(client, 0x21, 0x84); 
		tp285x_write(client, 0x22, 0x37);
		tp285x_write(client, 0x23, 0x3f);

		tp285x_write(client, 0x2b, 0x70);  
		tp285x_write(client, 0x2c, 0x2a); 
		tp285x_write(client, 0x2d, 0x64);
		tp285x_write(client, 0x2e, 0x56);

		tp285x_write(client, 0x30, 0x7a);  
		tp285x_write(client, 0x31, 0x4a); 
		tp285x_write(client, 0x32, 0x4d);
		tp285x_write(client, 0x33, 0xf0);	
		
		tp285x_write(client, 0x35, 0x25); 
		tp285x_write(client, 0x38, 0x00);				
		tp285x_write(client, 0x39, 0x04); 
		
		if(STD_HDA == std)					  
		{									  
			printk(" FHD25 STD_HDA	in .. ");
			tp285x_write(client, 0x02, 0x44);
			
			tp285x_write(client, 0x0d, 0x73);
			
			tp285x_write(client, 0x15, 0x01);
			tp285x_write(client, 0x16, 0xf0);
			
			tp285x_write(client, 0x20, 0x3c);
			tp285x_write(client, 0x21, 0x46);
			
			tp285x_write(client, 0x25, 0xfe);
			tp285x_write(client, 0x26, 0x0d);
			
			tp285x_write(client, 0x2c, 0x3a);
			tp285x_write(client, 0x2d, 0x54);
			tp285x_write(client, 0x2e, 0x40);
			
			tp285x_write(client, 0x30, 0xa5);
			tp285x_write(client, 0x31, 0x86);
			tp285x_write(client, 0x32, 0xfb);
			tp285x_write(client, 0x33, 0x60);
		}
	}
	else if (NTSC == fmt)
	{
		printk(" NTSC in .. ");
		tmp = tp285x_read(client,0xf5);
		tmp |= SYS_MODE[ch];
		tp285x_write(client, 0xf5, tmp);
				
		tp285x_write(client, 0x02, 0x47);
		tp285x_write(client, 0x0c, 0x13); 
		tp285x_write(client, 0x0d, 0x50);  

		tp285x_write(client, 0x15, 0x13);
		tp285x_write(client, 0x16, 0x60); 
		tp285x_write(client, 0x17, 0x80); 
		tp285x_write(client, 0x18, 0x12);
		tp285x_write(client, 0x19, 0xf0);
		tp285x_write(client, 0x1a, 0x07);				
		tp285x_write(client, 0x1c, 0x09);
		tp285x_write(client, 0x1d, 0x38);
	
		tp285x_write(client, 0x20, 0x40);  
		tp285x_write(client, 0x21, 0x84); 
		tp285x_write(client, 0x22, 0x36);
		tp285x_write(client, 0x23, 0x3c);

		tp285x_write(client, 0x2b, 0x70);  
		tp285x_write(client, 0x2c, 0x2a); 
		tp285x_write(client, 0x2d, 0x68);
		tp285x_write(client, 0x2e, 0x57);

		tp285x_write(client, 0x30, 0x62);  
		tp285x_write(client, 0x31, 0xbb); 
		tp285x_write(client, 0x32, 0x96);
		tp285x_write(client, 0x33, 0xc0);
		
		tp285x_write(client, 0x35, 0x25); 
		tp285x_write(client, 0x38, 0x00);			
		tp285x_write(client, 0x39, 0x04); 	
	}
	else if (HD25 == fmt)
	{
		printk(" HD25 in .. ");
		tmp = tp285x_read(client, 0xf5);
		tmp |= SYS_MODE[ch];
		tp285x_write(client, 0xf5, tmp);
        //printk("0xf5 [res %x]: des [ %x]\n",tmp, tp285x_read(client, 0xf5));
				
		tp285x_write(client, 0x02, 0x42);
        //printk("0x02 [res 0x42]: des [ %x]\n", tp285x_read(client, 0x02));

		tp285x_write(client, 0x07, 0xc0); 
        //printk("0x07 [res 0xc0]: des [ %x]\n", tp285x_read(client, 0x07));

		tp285x_write(client, 0x0b, 0xc0);
        //printk("0x0b [res 0xc0]: des [ %x]\n", tp285x_read(client, 0x0b));

		tp285x_write(client, 0x0c, 0x13); 
        //printk("0x0c [res 0x13]: des [ %x]\n", tp285x_read(client, 0x0c));

		tp285x_write(client, 0x0d, 0x50);
        //printk("0x0d [res 0x50]: des [ %x]\n", tp285x_read(client, 0x0d));

		tp285x_write(client, 0x15, 0x13);
        //printk("0x15 [res 0x13]: des [ %x]\n", tp285x_read(client, 0x15));

		tp285x_write(client, 0x16, 0x15);
        //printk("0x16 [res 0x15]: des [ %x]\n", tp285x_read(client, 0x16));

		tp285x_write(client, 0x17, 0x00); 
        //printk("0x17 [res 0x0]: des [ %x]\n", tp285x_read(client, 0x17));

		tp285x_write(client, 0x18, 0x19);
        //printk("0x18 [res 0x19]: des [ %x]\n", tp285x_read(client, 0x18));

		tp285x_write(client, 0x19, 0xd0);
        //printk("0x19 [res 0xd0]: des [ %x]\n", tp285x_read(client, 0x19));

		tp285x_write(client, 0x1a, 0x25);			
        //printk("0x1a [res 0x25]: des [ %x]\n", tp285x_read(client, 0x1a));

		tp285x_write(client, 0x1c, 0x07);  //1280*720, 25fps
        //printk("0x1c [res 0x07]: des [ %x]\n", tp285x_read(client, 0x1c));

		tp285x_write(client, 0x1d, 0xbc);  //1280*720, 25fps
        //printk("0x1d [res 0xbc]: des [ %x]\n", tp285x_read(client, 0x1d));

		tp285x_write(client, 0x20, 0x30);  
        //printk("0x20 [res 0x30]: des [ %x]\n", tp285x_read(client, 0x20));

		tp285x_write(client, 0x21, 0x84); 
        //printk("0x21 [res 0x84]: des [ %x]\n", tp285x_read(client, 0x21));

		tp285x_write(client, 0x22, 0x36);
        //printk("0x22 [res 0x36]: des [ %x]\n", tp285x_read(client, 0x22));

		tp285x_write(client, 0x23, 0x3c);
        //printk("0x23 [res 0x3c]: des [ %x]\n", tp285x_read(client, 0x23));

		tp285x_write(client, 0x2b, 0x60);  
        //printk("0x2b [res 0x60]: des [ %x]\n", tp285x_read(client, 0x2b));

		tp285x_write(client, 0x2c, 0x0a); 
        //printk("0x2c [res 0x0a]: des [ %x]\n", tp285x_read(client, 0x2c));

		tp285x_write(client, 0x2d, 0x30);
        //printk("0x2d [res 0x30]: des [ %x]\n", tp285x_read(client, 0x2d));
		
		tp285x_write(client, 0x2e, 0x70);
        //printk("0x2e [res 0x70]: des [ %x]\n", tp285x_read(client, 0x2e));

		tp285x_write(client, 0x30, 0x48);  
        //printk("0x30 [res 0x48]: des [ %x]\n", tp285x_read(client, 0x30));

		tp285x_write(client, 0x31, 0xbb); 
        //printk("0x31 [res 0xbb]: des [ %x]\n", tp285x_read(client, 0x31));

		tp285x_write(client, 0x32, 0x2e);
        //printk("0x32 [res 0x2e]: des [ %x]\n", tp285x_read(client, 0x32));

		tp285x_write(client, 0x33, 0x90);
        //printk("0x33 [res 0x90]: des [ %x]\n", tp285x_read(client, 0x33));
		
		tp285x_write(client, 0x35, 0x25); 
        //printk("0x35 [res 0x25]: des [ %x]\n", tp285x_read(client, 0x35));

		tp285x_write(client, 0x38, 0x00);	
        //printk("0x38 [res 0x0]: des [ %x]\n", tp285x_read(client, 0x38));

		tp285x_write(client, 0x39, 0x18); 
        //printk("0x39 [res 0x18]: des [ %x]\n", tp285x_read(client, 0x39));

		if (STD_HDA == std)
		{
			printk(" HD25  STD_HDA in .. ");
	    	tp285x_write(client, 0x02, 0x46);
	        //printk("0x2 [res 0x46]: des [ %x]\n", tp285x_read(client, 0x2) );
			
	    	tp285x_write(client, 0x0d, 0x71);
	        //printk("0x0d [res 0x71]: des [ %x]\n", tp285x_read(client, 0x0d) );

	    	tp285x_write(client, 0x20, 0x40);
	        //printk("0x20 [res 0x40]: des [ %x]\n", tp285x_read(client, 0x20) );

	    	tp285x_write(client, 0x21, 0x46);
	        //printk("0x21 [res 0x46]: des [ %x]\n", tp285x_read(client, 0x21) );

	    	tp285x_write(client, 0x25, 0xfe);
	        //printk("0x25 [res 0xfe]: des [ %x]\n", tp285x_read(client, 0x25) );

	    	tp285x_write(client, 0x26, 0x01);
	        //printk("0x26 [res 0x1]: des [ %x]\n", tp285x_read(client, 0x26) );

	    	tp285x_write(client, 0x2c, 0x3a);
	        //printk("0x2c [res 0x3a]: des [ %x]\n", tp285x_read(client, 0x2c) );

	    	tp285x_write(client, 0x2d, 0x5a);
	        //printk("0x2d [res 0x5a]: des [ %x]\n", tp285x_read(client, 0x2d) );

	    	tp285x_write(client, 0x2e, 0x40);
	        //printk("0x2e [res 0x40]: des [ %x]\n", tp285x_read(client, 0x2e) );

	    	tp285x_write(client, 0x30, 0x9e);
	        //printk("0x30 [res 0x9e]: des [ %x]\n", tp285x_read(client, 0x30) );

	    	tp285x_write(client, 0x31, 0x20);
	        //printk("0x31 [res 0x20]: des [ %x]\n", tp285x_read(client, 0x31) );

	    	tp285x_write(client, 0x32, 0x10);
	        //printk("0x32 [res 0x10]: des [ %x]\n", tp285x_read(client, 0x32) );

	    	tp285x_write(client, 0x33, 0x90);
	        //printk("0x33 [res 0x90]: des [ %x]\n", tp285x_read(client, 0x33) );
		}
	}
	else if (HD30 == fmt)
	{
		printk(" HD30   in .. ");
		tmp = tp285x_read(client,0xf5);
		tmp |= SYS_MODE[ch];
		tp285x_write(client, 0xf5, tmp);
				
		tp285x_write(client, 0x02, 0x42);
		tp285x_write(client, 0x07, 0xc0); 
		tp285x_write(client, 0x0b, 0xc0);  		
		tp285x_write(client, 0x0c, 0x13); 
		tp285x_write(client, 0x0d, 0x50);  

		tp285x_write(client, 0x15, 0x13);
		tp285x_write(client, 0x16, 0x15); 
		tp285x_write(client, 0x17, 0x00); 
		tp285x_write(client, 0x18, 0x19);
		tp285x_write(client, 0x19, 0xd0);
		tp285x_write(client, 0x1a, 0x25);			
		tp285x_write(client, 0x1c, 0x06);  //1280*720, 30fps
		tp285x_write(client, 0x1d, 0x72);  //1280*720, 30fps

		tp285x_write(client, 0x20, 0x30);  
		tp285x_write(client, 0x21, 0x84); 
		tp285x_write(client, 0x22, 0x36);
		tp285x_write(client, 0x23, 0x3c);

		tp285x_write(client, 0x2b, 0x60);  
		tp285x_write(client, 0x2c, 0x0a); 
		tp285x_write(client, 0x2d, 0x30);
		tp285x_write(client, 0x2e, 0x70);

		tp285x_write(client, 0x30, 0x48);  
		tp285x_write(client, 0x31, 0xbb); 
		tp285x_write(client, 0x32, 0x2e);
		tp285x_write(client, 0x33, 0x90);
		
		tp285x_write(client, 0x35, 0x25); 
		tp285x_write(client, 0x38, 0x00);	
		tp285x_write(client, 0x39, 0x18); 

		if (STD_HDA == std)
		{
			printk(" HD30  STD_HDA in .. ");
	    	tp285x_write(client, 0x02, 0x46);

	    	tp285x_write(client, 0x0d, 0x70);

	    	tp285x_write(client, 0x20, 0x40);
	    	tp285x_write(client, 0x21, 0x46);

	    	tp285x_write(client, 0x25, 0xfe);
	    	tp285x_write(client, 0x26, 0x01);

	    	tp285x_write(client, 0x2c, 0x3a);
	    	tp285x_write(client, 0x2d, 0x5a);
	    	tp285x_write(client, 0x2e, 0x40);

	    	tp285x_write(client, 0x30, 0x9d);
	    	tp285x_write(client, 0x31, 0xca);
	    	tp285x_write(client, 0x32, 0x01);
	    	tp285x_write(client, 0x33, 0xd0);
		}	
	}
	else if (FHD30 == fmt)
	{
		printk(" FHD30  in .. ");
		tmp = tp285x_read(client,0xf5);
		tmp &= ~SYS_MODE[ch];
		tp285x_write(client, 0xf5, tmp);
		
		tp285x_write(client, 0x02, 0x40);
		tp285x_write(client, 0x07, 0xc0); 
		tp285x_write(client, 0x0b, 0xc0);  		
		tp285x_write(client, 0x0c, 0x03); 
		tp285x_write(client, 0x0d, 0x50);  

		tp285x_write(client, 0x15, 0x03);
		tp285x_write(client, 0x16, 0xd2); 
		tp285x_write(client, 0x17, 0x80); 
		tp285x_write(client, 0x18, 0x29);
		tp285x_write(client, 0x19, 0x38);
		tp285x_write(client, 0x1a, 0x47);				
		tp285x_write(client, 0x1c, 0x08);  //1920*1080, 30fps
		tp285x_write(client, 0x1d, 0x98);  //
	
		tp285x_write(client, 0x20, 0x30);  
		tp285x_write(client, 0x21, 0x84); 
		tp285x_write(client, 0x22, 0x36);
		tp285x_write(client, 0x23, 0x3c);

		tp285x_write(client, 0x2b, 0x60);  
		tp285x_write(client, 0x2c, 0x0a); 
		tp285x_write(client, 0x2d, 0x30);
		tp285x_write(client, 0x2e, 0x70);

		tp285x_write(client, 0x30, 0x48);  
		tp285x_write(client, 0x31, 0xbb); 
		tp285x_write(client, 0x32, 0x2e);
		tp285x_write(client, 0x33, 0x90);
			
		tp285x_write(client, 0x35, 0x05);
		tp285x_write(client, 0x38, 0x00); 
		tp285x_write(client, 0x39, 0x1C); 	
	
		if (STD_HDA == std)
		{
			printk(" FHD30	STD_HDA in .. ");
			tp285x_write(client, 0x02, 0x44);

			tp285x_write(client, 0x0d, 0x72);

			tp285x_write(client, 0x15, 0x01);
			tp285x_write(client, 0x16, 0xf0);

			tp285x_write(client, 0x20, 0x38);
			tp285x_write(client, 0x21, 0x46);

			tp285x_write(client, 0x25, 0xfe);
			tp285x_write(client, 0x26, 0x0d);

			tp285x_write(client, 0x2c, 0x3a);
			tp285x_write(client, 0x2d, 0x54);
			tp285x_write(client, 0x2e, 0x40);

			tp285x_write(client, 0x30, 0xa5);
			tp285x_write(client, 0x31, 0x95);
			tp285x_write(client, 0x32, 0xe0);
			tp285x_write(client, 0x33, 0x60);
		}
	}
	else if (FHD25 == fmt)
	{
		printk(" FHD25 in .. ");
		tmp = tp285x_read(client,0xf5);
		tmp &= ~SYS_MODE[ch];
		tp285x_write(client, 0xf5, tmp);
		
		tp285x_write(client, 0x02, 0x40);
		tp285x_write(client, 0x07, 0xc0); 
		tp285x_write(client, 0x0b, 0xc0);  		
		tp285x_write(client, 0x0c, 0x03); 
		tp285x_write(client, 0x0d, 0x50);  
  		
		tp285x_write(client, 0x15, 0x03);
		tp285x_write(client, 0x16, 0xd2); 
		tp285x_write(client, 0x17, 0x80); 
		tp285x_write(client, 0x18, 0x29);
		tp285x_write(client, 0x19, 0x38);
		tp285x_write(client, 0x1a, 0x47);				
  		
		tp285x_write(client, 0x1c, 0x0a);  //1920*1080, 25fps
		tp285x_write(client, 0x1d, 0x50);  //
			
		tp285x_write(client, 0x20, 0x30);  
		tp285x_write(client, 0x21, 0x84); 
		tp285x_write(client, 0x22, 0x36);
		tp285x_write(client, 0x23, 0x3c);
  		
		tp285x_write(client, 0x2b, 0x60);  
		tp285x_write(client, 0x2c, 0x0a); 
		tp285x_write(client, 0x2d, 0x30);
		tp285x_write(client, 0x2e, 0x70);
  		
		tp285x_write(client, 0x30, 0x48);  
		tp285x_write(client, 0x31, 0xbb); 
		tp285x_write(client, 0x32, 0x2e);
		tp285x_write(client, 0x33, 0x90);
					
		tp285x_write(client, 0x35, 0x05);
		tp285x_write(client, 0x38, 0x00); 
		tp285x_write(client, 0x39, 0x1C); 	

		if (STD_HDA == std)                    
		{                                     
			printk(" FHD25 STD_HDA  in .. ");
   			tp285x_write(client, 0x02, 0x44);
   			
   			tp285x_write(client, 0x0d, 0x73);
   			
   			tp285x_write(client, 0x15, 0x01);
   			tp285x_write(client, 0x16, 0xf0);
   			
   			tp285x_write(client, 0x20, 0x3c);
   			tp285x_write(client, 0x21, 0x46);
   			
   			tp285x_write(client, 0x25, 0xfe);
   			tp285x_write(client, 0x26, 0x0d);
   			
   			tp285x_write(client, 0x2c, 0x3a);
   			tp285x_write(client, 0x2d, 0x54);
   			tp285x_write(client, 0x2e, 0x40);
   			
   			tp285x_write(client, 0x30, 0xa5);
   			tp285x_write(client, 0x31, 0x86);
   			tp285x_write(client, 0x32, 0xfb);
   			tp285x_write(client, 0x33, 0x60);
		}
	}	
	
}
void TP2854B_mipi_out(struct i2c_client *client,unsigned char output)
{
	//mipi setting
	tp285x_write(client, 0x40, MIPI_PAGE); //MIPI page 
    tp285x_write(client, 0x01, 0xf8);
    tp285x_write(client, 0x02, 0x01);
    tp285x_write(client, 0x08, 0x0f);
	
	tp285x_write(client, 0x10, 0x20);
	tp285x_write(client, 0x11, 0x47);
	tp285x_write(client, 0x12, 0x54);
	tp285x_write(client, 0x13, 0xef);

	if (MIPI_2CH4LANE_594M == output)
	{
		tp285x_write(client, 0x20, 0x24);
		tp285x_write(client, 0x34, 0x32);	// vin 3 vin 4

		tp285x_write(client, 0x14, 0x06);
		tp285x_write(client, 0x15, 0x00);
		tp285x_write(client, 0x25, 0x08);
		tp285x_write(client, 0x26, 0x06);
		tp285x_write(client, 0x27, 0x11);
		tp285x_write(client, 0x29, 0x0a);
		
		tp285x_write(client, 0x0a, 0x80);
		tp285x_write(client, 0x33, 0x0f);
		tp285x_write(client, 0x33, 0x00);
		tp285x_write(client, 0x14, 0x86);
		tp285x_write(client, 0x14, 0x06);
	}
	else if (MIPI_2CH4LANE_297M == output)
	{
		tp285x_write(client, 0x20, 0x24);
		tp285x_write(client, 0x34, 0x32);

		tp285x_write(client, 0x14, 0x47);
		tp285x_write(client, 0x15, 0x01);
		tp285x_write(client, 0x25, 0x04);
		tp285x_write(client, 0x26, 0x03);
		tp285x_write(client, 0x27, 0x09);
		tp285x_write(client, 0x29, 0x02);
		
		tp285x_write(client, 0x0a, 0x80);
		tp285x_write(client, 0x33, 0x0f);
		tp285x_write(client, 0x33, 0x00);
		tp285x_write(client, 0x14, 0xc7);
		tp285x_write(client, 0x14, 0x47);
	}
	else if (MIPI_2CH2LANE_594M == output)
	{
		tp285x_write(client, 0x20, 0x22);

		//tp285x_write(client, 0x34, 0x32);	// vin 3 vin 4
		tp285x_write(client, 0x34, 0x10);	// vin 1 vin 2

		tp285x_write(client, 0x14, 0x06);
		tp285x_write(client, 0x15, 0x01);
		tp285x_write(client, 0x25, 0x08);
		tp285x_write(client, 0x26, 0x06);
		tp285x_write(client, 0x27, 0x11);
		tp285x_write(client, 0x29, 0x0a);
		
		tp285x_write(client, 0x0a, 0x80);
		tp285x_write(client, 0x33, 0x0f);
		tp285x_write(client, 0x33, 0x00);
		tp285x_write(client, 0x14, 0x86);
		tp285x_write(client, 0x14, 0x06);
	}
	else if (MIPI_2CH2LANE_297M == output)
	{
		//tp285x_write(client, 0x20, 0x12);
		tp285x_write(client, 0x20, 0x22);
        //printk("0x20 [res 0x22]: des [ %x]\n", tp285x_read(client, 0x20));

		//tp285x_write(client, 0x34, 0x32);
		tp285x_write(client, 0x34, 0x10);	// vin 1 vin 2
        //printk("0x34 [res 0x10]: des [ %x]\n", tp285x_read(client, 0x34));

		tp285x_write(client, 0x14, 0x47);
        //printk("0x14 [res 0x47]: des [ %x]\n", tp285x_read(client, 0x14));

		tp285x_write(client, 0x15, 0x02);
        //printk("0x15 [res 0x02]: des [ %x]\n", tp285x_read(client, 0x15));

		tp285x_write(client, 0x25, 0x04);
        //printk("0x25 [res 0x04]: des [ %x]\n", tp285x_read(client, 0x25));

		tp285x_write(client, 0x26, 0x03);
        //printk("0x26 [res 0x03]: des [ %x]\n", tp285x_read(client, 0x26));

		tp285x_write(client, 0x27, 0x09);
        //printk("0x27 [res 0x09]: des [ %x]\n", tp285x_read(client, 0x27));

		tp285x_write(client, 0x29, 0x02);
        //printk("0x29 [res 0x02]: des [ %x]\n", tp285x_read(client, 0x29));
		
		tp285x_write(client, 0x0a, 0x80);
        //printk("0x0a [res 0x80]: des [ %x]\n", tp285x_read(client, 0x0a));

		tp285x_write(client, 0x33, 0x0f);
        //printk("0x33 [res 0x0f]: des [ %x]\n", tp285x_read(client, 0x33));

		tp285x_write(client, 0x33, 0x00);
        //printk("0x33 [res 0x00]: des [ %x]\n", tp285x_read(client, 0x33));
		
		tp285x_write(client, 0x14, 0xc7);
        //printk("0x14 [res 0xc7]: des [ %x]\n", tp285x_read(client, 0x14));

		tp285x_write(client, 0x14, 0x47);
        //printk("0x14 [res 0x47]: des [ %x]\n", tp285x_read(client, 0x14));
	
	}
	else if (MIPI_2CH4LANE_148M == output)
	{
		tp285x_write(client, 0x20, 0x24);
		tp285x_write(client, 0x34, 0x32);

		tp285x_write(client, 0x14, 0x53);
		tp285x_write(client, 0x15, 0x02);	
		tp285x_write(client, 0x25, 0x02);
		tp285x_write(client, 0x26, 0x01);
		tp285x_write(client, 0x27, 0x05);
		tp285x_write(client, 0x29, 0x02);
		
		tp285x_write(client, 0x0a, 0x80);
		tp285x_write(client, 0x33, 0x0f);
		tp285x_write(client, 0x33, 0x00);
		tp285x_write(client, 0x14, 0xd3);
		tp285x_write(client, 0x14, 0x53);
	}
	
	/* Enable MIPI CSI2 output */
	tp285x_write(client, 0x23, 0x00);
	
    //printk("0x23 [res 0x0]: des [ %x]\n", tp285x_read(client, 0x23) );
}

void TP2854_common_init_t(struct i2c_client *client, tp285x_resolution *pstTp285x_resolution)
{
	unsigned int val = 0;
	
	printk("TP2854_common_init\n");
	
	TP285x_reset_default(client);
	
	tp285x_write(client, 0x40, 0x00);
	
    printk("40 [res 00]: des [ %x]\n", tp285x_read(client, 0x40));

	val = tp285x_read(client, 0xfe);
	val <<= 8;
	val |= tp285x_read(client, 0xff);
	if (0x2854 != val)
	{
		printk("Invalid chip %2x\n", val);
		return;
	}
	
	/* Disable MIPI CSI2 output */
	tp285x_write(client, 0x40, MIPI_PAGE);
    printk("0x40 [res %x]: des [ %x]\n",MIPI_PAGE, tp285x_read(client, 0x40));

	tp285x_write(client, 0x23, 0x02);
    printk("0x23 [res 0x02]: des [ %x]\n", tp285x_read(client, 0x23));

	tp285x_write(client, 0x40, 0x02);	// vin 1 2
    printk("0x40 [res 0x02]: des [ %x]\n", tp285x_read(client, 0x40));

	tp285x_write(client, 0x3d, 0xff);
    printk("0x3d [res 0xff]: des [ %x]\n", tp285x_read(client, 0x3d));

	tp285x_write(client, 0x40, 0x03);	// vin 1 2
    printk("0x40 [res 0x03]: des [ %x]\n", tp285x_read(client, 0x40));

	tp285x_write(client, 0x3d, 0xff);
    printk("0x3d [res 0xff]: des [ %x]\n", tp285x_read(client, 0x3d));

	tp285x_write(client, 0xf4, 0x2c);
    printk("0xf4 [res 0x2c]: des [ %x]\n", tp285x_read(client, 0xf4));
							
	tp285x_write(client, 0x40, CH_1);
    printk("0x40 [res %x]: des [ %x]\n",CH_1, tp285x_read(client, 0x40));

	tp285x_write(client, 0x4e, 0x00);
    printk("0x4e [res 0x0]: des [ %x]\n", tp285x_read(client, 0x4e));

	tp285x_write(client, 0xf5, 0xfc);	// vin 1 2	
    printk("0xf5 [res 0xfc]: des [ %x]\n", tp285x_read(client, 0xf5));

	printk("TP2854_common_init out ....\n");
}


void TP2854_common_init(struct i2c_client *client, int fs, int resloution)
{
	unsigned int val = 0;
	
	printk("TP2854_common_init\n");
	
	TP285x_reset_default(client);
	
	tp285x_write(client, 0x40, 0x00);
    printk("40 [res 00]: des [ %x]\n", tp285x_read(client, 0x40));
	
	val = tp285x_read(client, 0xfe);
	val <<= 8;
	val |= tp285x_read(client, 0xff);

	if (0x2854 != val)
	{
		printk("Invalid chip %2x\n", val);
		return;
	}
	
	#if 0	//VIN3_4
	/* Disable MIPI CSI2 output */
	tp285x_write(client, 0x40, MIPI_PAGE);
	tp285x_write(client, 0x23, 0x02);
	tp285x_write(client, 0x40, 0x00);	// vin 3 4
	tp285x_write(client, 0x3d, 0xff);
	tp285x_write(client, 0x40, 0x01);	// vin 3 4
	tp285x_write(client, 0x3d, 0xff);
	tp285x_write(client, 0xf4, 0x23);
							
	tp285x_write(client, 0x40, CH_1);
	tp285x_write(client, 0x4e, 0x00);
	tp285x_write(client, 0xf5, 0xf3);	// vin 3 4	
	#endif
	
	/* Disable MIPI CSI2 output */
	tp285x_write(client, 0x40, MIPI_PAGE);
	tp285x_write(client, 0x23, 0x02);
	tp285x_write(client, 0x40, 0x02);	// vin 1 2
	tp285x_write(client, 0x3d, 0xff);
	tp285x_write(client, 0x40, 0x03);	// vin 1 2
	tp285x_write(client, 0x3d, 0xff);
	tp285x_write(client, 0xf4, 0x2c);
	tp285x_write(client, 0x40, CH_1);
	tp285x_write(client, 0x4e, 0x00);
	tp285x_write(client, 0xf5, 0xfc);	// vin 1 2	

	if ((HD25 == fs) && (MIPI_2CH2LANE_297M == resloution))
	{
		TP2854_decoder_init(client, CH_1, HD25, STD_HDA);
		TP2854_decoder_init(client, CH_2, HD25, STD_HDA);
		TP2854B_mipi_out(client, MIPI_2CH2LANE_297M);
	}
	else
	{
		TP2854_decoder_init(client, CH_1, FHD25,STD_HDA);
		TP2854_decoder_init(client, CH_2, FHD25,STD_HDA);
		TP2854B_mipi_out(client, MIPI_2CH2LANE_594M);
	}
	
	printk("TP2854_common_init out ....\n");	
}

void TP2854_dumpReg(struct i2c_client *client)
{
#if 0
	const unsigned char aucMipiRegIndex[] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
		0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
		0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
		0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
		0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
		0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
		0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
		0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
		0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
		0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
		0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
		0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
		0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
		};

	const unsigned char aucVideoRegIndex[] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
		0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
		0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
		0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
		0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
		0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
		0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
		0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
		0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
		0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
		0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
		0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
		0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
		0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
		0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
		0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff

		};
#endif

#if 0
	tp285x_resolution tp285x_value;
	int channel = 0;
	int i = 0;	
	
	printk("TP2854_dumpReg in ....\n");
	
	tp285x_write(client, 0x40, 0x00);
	msleep(10);

	for (i = 0; i < 20; i++)
	{

		for(channel = 0; channel < 4; channel++)
		{
			tp285x_write(client, 0x40, channel); 
			msleep(10);
			if(channel == 0)
			{
				tp285x_value.channel0 = tp285x_read(client, 0x03);
				printk("TP2850 driver channel == %d tp285x_value = 0x%x\n",channel, tp285x_value.channel0 & 7);
			}
			else if(channel ==1)
			{
				tp285x_value.channel1 = tp285x_read(client, 0x03);
				printk("TP2850 driver channel == %d tp285x_value = 0x%x\n",channel, tp285x_value.channel1 & 7);
			}
			else if(channel ==2)
			{
				tp285x_value.channel2 = tp285x_read(client, 0x03);
				printk("TP2850 driver channel == %d tp285x_value = 0x%x\n",channel, tp285x_value.channel2 & 7);
			}
			else if(channel ==3)
			{
				tp285x_value.channel3 = tp285x_read(client, 0x03);
				printk("TP2850 driver channel == %d tp285x_value = 0x%x\n",channel, tp285x_value.channel3 & 7);
			}
		}
		
		msleep(2000);
	}
	
	printk("TP2854_dumpReg out ....\n");
#endif
}

int tp2854_init_data(struct i2c_client *client)
{
	int retval = 0;
	int iID;
	
    printk("TP2850 probe.. i2c_addr is %x\n", client->addr);

	tp285x_write(client, 0x40, 0x00);
	iID = tp285x_read(client, 0xfe);
	iID <<= 8;
	iID |= tp285x_read(client, 0xff);

	printk("TPdevice regID:[%x]...\n",iID); 
	
    if (!(tp2854 = devm_kzalloc(&client->dev, sizeof(tp285x_data), GFP_KERNEL)))
	{
        printk("devm_kzalloc err...\n");
        return -1;
    }
    memset(tp2854, 0, sizeof(tp285x_data));

    i2c_set_clientdata(client, tp2854);

    retval = setup_dev_int(client);   
	if (retval)
	{
		return -1;
	}
	
#if 0	//not 3516dv300
	tp2850_hardware_init(client);
#else   
	// TP2854_common_init(client, HD25, MIPI_2CH2LANE_297M);
#endif
	
	proc_debug_tp_init(client);
	
	return retval;
}

static int tp2850_release(struct inode *inode, struct file *file)
{
	// struct i2c_client *client = file->private_data;
	
	printk(KERN_INFO"%s\n",__func__);
	
	return 0;
}

static int tp2850_open(struct inode *inode, struct file *filp)
{
    filp->private_data = tp2854;
	
	printk(KERN_INFO"%s\n",__func__);
	
	return 0;
}

static int tp285x_set_image(struct i2c_client *client, tp285x_image_adjust *ptr_image_adjust)
{
	int		idx = 0;
	
	if ((client == NULL) || (ptr_image_adjust == NULL))
	{
		printk("[%s, %d] param error.\n", __FUNCTION__, __LINE__);
		return -1;
	}

	if ((ptr_image_adjust->chn >= 0) && (ptr_image_adjust->chn < 4))
	{
		tp285x_write(client, 0x40, (unsigned char)ptr_image_adjust->chn);

		if (s_tp285x_bright[ptr_image_adjust->chn] != ptr_image_adjust->brightness)
		{
			s_tp285x_bright[ptr_image_adjust->chn] = ptr_image_adjust->brightness;
			tp285x_write(client, 0x10, s_tp285x_bright[ptr_image_adjust->chn]);
		}
		if (s_tp285x_contrast[ptr_image_adjust->chn] != ptr_image_adjust->contrast)
		{
			s_tp285x_contrast[ptr_image_adjust->chn] = ptr_image_adjust->contrast;
			tp285x_write(client, 0x11, s_tp285x_contrast[ptr_image_adjust->chn]); 
		}
		if (s_tp285x_saturation[ptr_image_adjust->chn] != ptr_image_adjust->saturation)
		{
			s_tp285x_saturation[ptr_image_adjust->chn] 	= ptr_image_adjust->saturation;
			tp285x_write(client, 0x12, s_tp285x_saturation[ptr_image_adjust->chn]); 
		}
		if (s_tp285x_hue[ptr_image_adjust->chn] != ptr_image_adjust->hue)
		{
			s_tp285x_hue[ptr_image_adjust->chn] = ptr_image_adjust->hue;
			tp285x_write(client, 0x13, s_tp285x_hue[ptr_image_adjust->chn]);
		}
		if (s_tp285x_sharpness[ptr_image_adjust->chn] != ptr_image_adjust->sharpness)
		{
			s_tp285x_sharpness[ptr_image_adjust->chn] = ptr_image_adjust->sharpness;
			tp285x_write(client, 0x14, s_tp285x_sharpness[ptr_image_adjust->chn]);
		}
	}
	else if (ptr_image_adjust->chn == 0xFF)
	{
		tp285x_write(client, 0x40, 0x00);

		for (idx = 0; idx < TP285X_MAX_CHN; idx++)
		{
			s_tp285x_bright[idx] 		= ptr_image_adjust->brightness;
			s_tp285x_contrast[idx] 		= ptr_image_adjust->contrast;
			s_tp285x_saturation[idx] 	= ptr_image_adjust->saturation;
			s_tp285x_hue[idx] 			= ptr_image_adjust->hue;
			s_tp285x_sharpness[idx] 	= ptr_image_adjust->sharpness;
		}

		tp285x_write(client, 0x10, s_tp285x_bright[0]); 
	    tp285x_write(client, 0x11, s_tp285x_contrast[0]); 
	    tp285x_write(client, 0x12, s_tp285x_saturation[0]); 
		tp285x_write(client, 0x13, s_tp285x_hue[0]);
	    tp285x_write(client, 0x14, s_tp285x_sharpness[0]);
	}
	
	return 0;
}

static int tp285x_get_image(struct i2c_client *client, tp285x_image_adjust *ptr_image_adjust)
{
	if ((client == NULL) || (ptr_image_adjust == NULL))
	{
		printk("[%s, %d] param error.\n", __FUNCTION__, __LINE__);
		return -1;
	}

	if ((ptr_image_adjust->chn >= 0) && (ptr_image_adjust->chn < 4))
	{
		tp285x_write(client, 0x40, (unsigned char)ptr_image_adjust->chn);

		ptr_image_adjust->brightness 	= tp285x_read(client, 0x10);
		ptr_image_adjust->contrast		= tp285x_read(client, 0x11);
		ptr_image_adjust->saturation	= tp285x_read(client, 0x12);
		ptr_image_adjust->hue			= tp285x_read(client, 0x13);
		ptr_image_adjust->sharpness		= tp285x_read(client, 0x14);
	}
	
	return 0;
}

static long tp2850_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    unsigned int __user 	*argp = (unsigned int __user *)arg;
    tp285x_video_mode		video_mode;
	
 	tp285x_image_adjust    	image_adjust;
 	
    tp285x_register			register_data;
	
    struct tp285x_data 		*data = filp->private_data;
    struct i2c_client 		*client = data->client;
	
	int ret = 0;

	switch (_IOC_NR(cmd))
    {
	    case _IOC_NR(TP2850_SET_VIDEO_MODE):
			if (copy_from_user(&video_mode, argp, sizeof(tp285x_video_mode)))
			{
				return FAILURE; 
	        }

			printk("Try to set video mode to %d\n", video_mode.mode);
			
			if (video_mode.mode == TP2854_HDA_720P25_2LANES)
			{
				TP2854_common_init(client, HD25, MIPI_2CH2LANE_297M);
			}
			else if (video_mode.mode == TP2854_HDA_1080P25_2LANES)
			{
				TP2854_common_init(client, FHD25, MIPI_2CH2LANE_594M);
			}
			else if (video_mode.mode == TP2854_HDA_720P25_4LANES)
			{
				TP285x_reset_default(client);
				
                //video setting
                tp285x_write(client, 0x40, 0x04); //video page, write all
                printk("addr 0x40 value[0x04] %x\n", tp285x_read(client, 0x40));

                tp285x_write(client, 0x4e, 0x00);
                printk("addr 0x4e value[0x0] %x\n", tp285x_read(client, 0x4e));

                tp285x_write(client, 0xf5, 0xf0);
                printk("addr 0xf5 value[0xf0] %x\n", tp285x_read(client, 0xf5));

                tp285x_write(client, 0x02, 0x46);
                printk("addr 0x02 value[0x46] %x\n", tp285x_read(client, 0x02));

                tp285x_write(client, 0x07, 0xc0); 
                printk("addr 0x07 value[0xc0] %x\n", tp285x_read(client, 0x07));

                tp285x_write(client, 0x0b, 0xc0);  		
                printk("addr 0x0b value[0xc0] %x\n", tp285x_read(client, 0x0b));

                tp285x_write(client, 0x0c, 0x13); 
                printk("addr 0x0c value[0x13] %x\n", tp285x_read(client, 0x0c));

                tp285x_write(client, 0x0d, 0x71);  
                printk("addr 0x0d value[0x71] %x\n", tp285x_read(client, 0x0d));

                tp285x_write(client, 0x15, 0x13);
                printk("addr 0x15 value[0x13] %x\n", tp285x_read(client, 0x15));

                tp285x_write(client, 0x16, 0x15); 
                printk("addr 0x16 value[0x15] %x\n", tp285x_read(client, 0x16));

                tp285x_write(client, 0x17, 0x00); 
                printk("addr 0x17 value[0x0] %x\n", tp285x_read(client, 0x17));

                tp285x_write(client, 0x18, 0x19);
                printk("addr 0x18 value[0x19] %x\n", tp285x_read(client, 0x18));

                tp285x_write(client, 0x19, 0xd0);
                printk("addr 0x19 value[0xd0] %x\n", tp285x_read(client, 0x19));

                tp285x_write(client, 0x1a, 0x25);			
                printk("addr 0x1a value[0x25] %x\n", tp285x_read(client, 0x1a));

                tp285x_write(client, 0x1c, 0x07);  //1280*720, 25fps
                printk("addr 0x1c value[0x07] %x\n", tp285x_read(client, 0x1c));

                tp285x_write(client, 0x1d, 0xbc);  //1280*720, 25fps
                printk("addr 0x1d value[0xbc] %x\n", tp285x_read(client, 0x1d));

                tp285x_write(client, 0x20, 0x40);  
                printk("addr 0x20 value[0x40] %x\n", tp285x_read(client, 0x20));

                tp285x_write(client, 0x21, 0x46); 
                printk("addr 0x21 value[0x46] %x\n", tp285x_read(client, 0x21));

                tp285x_write(client, 0x22, 0x36);
                printk("addr 0x22 value[0x36] %x\n", tp285x_read(client, 0x22));

                tp285x_write(client, 0x23, 0x3c);
                printk("addr 0x23 value[0x3c] %x\n", tp285x_read(client, 0x23));

                tp285x_write(client, 0x25, 0xfe);
                printk("addr 0x25 value[0xfe] %x\n", tp285x_read(client, 0x25));

                tp285x_write(client, 0x26, 0x01);
                printk("addr 0x26 value[0x01] %x\n", tp285x_read(client, 0x26));

                tp285x_write(client, 0x2b, 0x60);  
                printk("addr 0x2b value[0x60] %x\n", tp285x_read(client, 0x2b));

                tp285x_write(client, 0x2c, 0x0a); 
                printk("addr 0x2c value[0x0a] %x\n", tp285x_read(client, 0x2c));

                tp285x_write(client, 0x2d, 0x5a);
                printk("addr 0x2d value[0x5a] %x\n", tp285x_read(client, 0x2d));

                tp285x_write(client, 0x2e, 0x70);
                printk("addr 0x2e value[0x70] %x\n", tp285x_read(client, 0x2e));

                tp285x_write(client, 0x30, 0x9e);  
                printk("addr 0x30 value[0x93] %x\n", tp285x_read(client, 0x30));

                tp285x_write(client, 0x31, 0x20); 
                printk("addr 0x31 value[0x20] %x\n", tp285x_read(client, 0x31));

                tp285x_write(client, 0x32, 0x01);
                printk("addr 0x32 value[0x01] %x\n", tp285x_read(client, 0x32));

                tp285x_write(client, 0x33, 0x90);
                printk("addr 0x33 value[0x90] %x\n", tp285x_read(client, 0x33));

                tp285x_write(client, 0x35, 0x25); 
                printk("addr 0x35 value[0x25] %x\n", tp285x_read(client, 0x35));

                tp285x_write(client, 0x38, 0x00);
                printk("addr 0x38 value[0x0] %x\n", tp285x_read(client, 0x38));

                tp285x_write(client, 0x39, 0x18); 
                printk("addr 0x39 value[0x18] %x\n", tp285x_read(client, 0x39));

                printk("addr 0xf5 %x\n",   tp285x_read(client, 0xf5) | 0x0f);
                tp285x_write(client, 0xf5, tp285x_read(client, 0xf5) | 0x0f);
                printk("addr 0xf5 %x\n",   tp285x_read(client, 0xf5));
				
                //mipi setting
                tp285x_write(client, 0x40, 0x08); //MIPI page 
                printk("addr 0x40 value[0x08] %x\n", tp285x_read(client, 0x40));

                tp285x_write(client, 0x01, 0xf8);
                printk("addr 0x01 value[0xf8] %x\n", tp285x_read(client, 0x01));

                tp285x_write(client, 0x02, 0x01);
                printk("addr 0x02 value[0x01] %x\n", tp285x_read(client, 0x02));

                tp285x_write(client, 0x08, 0x0f);
                printk("addr 0x8 value[0x0f] %x\n", tp285x_read(client, 0x8));

                tp285x_write(client, 0x0a, 0x80);
                printk("addr 0x0a value[0x80] %x\n", tp285x_read(client, 0x0a));

                tp285x_write(client, 0x10, 0x20);
                printk("addr 0x10 value[0x20] %x\n", tp285x_read(client, 0x10));

                tp285x_write(client, 0x11, 0x47);
                printk("addr 0x11 value[0x47] %x\n", tp285x_read(client, 0x11));

                tp285x_write(client, 0x12, 0x54);
                printk("addr 0x12 value[0x54] %x\n", tp285x_read(client, 0x12));

                tp285x_write(client, 0x13, 0xef);
                printk("addr 0x13 value[0xef] %x\n", tp285x_read(client, 0x13));

                tp285x_write(client, 0x20, 0x44);
                printk("addr 0x20 value[0x44] %x\n", tp285x_read(client, 0x20));

                tp285x_write(client, 0x34, 0xe4);
                printk("addr 0x34 value[0xe4] %x\n", tp285x_read(client, 0x34));

                tp285x_write(client, 0x14, 0x47);
                printk("addr 0x14 value[0x47] %x\n", tp285x_read(client, 0x14));

                tp285x_write(client, 0x15, 0x01);	
                printk("addr 0x15 value[0x01] %x\n", tp285x_read(client, 0x15));

                tp285x_write(client, 0x25, 0x04);
                printk("addr 0x25 value[0x04] %x\n", tp285x_read(client, 0x25));

                tp285x_write(client, 0x26, 0x04);
                printk("addr 0x26 value[0x04] %x\n", tp285x_read(client, 0x26));

                tp285x_write(client, 0x27, 0x09);
                printk("addr 0x27 value[0x09] %x\n", tp285x_read(client, 0x27));

                tp285x_write(client, 0x33, 0x0f);
                printk("addr 0x33 value[0x0f] %x\n", tp285x_read(client, 0x33));

                tp285x_write(client, 0x33, 0x00);
                printk("addr 0x33 value[0x0] %x\n", tp285x_read(client, 0x33));

                tp285x_write(client, 0x14, 0x4f);
                printk("addr 0x14 value[0x4f] %x\n", tp285x_read(client, 0x14));

                tp285x_write(client, 0x14, 0x47);
                printk("addr 0x14 value[0x47] %x\n", tp285x_read(client, 0x14));

                /* Disable MIPI CSI2 output */
                tp285x_write(client, 0x23, 0x02);
                printk("addr 0x23 value[0x02] %x\n", tp285x_read(client, 0x23));

                /* Enable MIPI CSI2 output */
                tp285x_write(client, 0x23, 0x00);
                printk("addr 0x23 value[0x0] %x\n", tp285x_read(client, 0x23));
			}
			else if (video_mode.mode == TP2854_HDA_1080P25_4LANES)
			{
				printk("Try to set video mode to TP2854_HDA_1080P25_4LANES\n");
				
				TP285x_reset_default(client);
				
                //video setting
                tp285x_write(client,0x40, 0x04); //video page, write all
                printk("addr 0x40 value[0x04] %x\n", tp285x_read(client, 0x40));

                tp285x_write(client,0x4e, 0x00);
                printk("addr 0x4e value[0x0] %x\n", tp285x_read(client, 0x4e));

                tp285x_write(client,0xf5, 0xf0);
                printk("addr 0xf5 value[0xf0] %x\n", tp285x_read(client, 0xf5));

                tp285x_write(client,0x02, 0x44);
                printk("addr 0x02 value[0x44] %x\n", tp285x_read(client, 0x02));

                tp285x_write(client,0x07, 0xc0); 
                printk("addr 0x07 value[0xc0] %x\n", tp285x_read(client, 0x07));

                tp285x_write(client,0x0b, 0xc0);        
                printk("addr 0x0b value[0xc0] %x\n", tp285x_read(client, 0x0b));

                tp285x_write(client,0x0c, 0x03); 
                printk("addr 0x0c value[0x03] %x\n", tp285x_read(client, 0x0c));

                tp285x_write(client,0x0d, 0x73);  
                printk("addr 0x0d value[0x73] %x\n", tp285x_read(client, 0x0d));

                tp285x_write(client,0x15, 0x01);
                printk("addr 0x15 value[0x01] %x\n", tp285x_read(client, 0x15));

                tp285x_write(client,0x16, 0xf0); 
                printk("addr 0x16 value[0xf0] %x\n", tp285x_read(client, 0x16));

                tp285x_write(client,0x17, 0x80); // Active=1920
                printk("addr 0x17 value[0x80] %x\n", tp285x_read(client, 0x17));

                tp285x_write(client,0x18, 0x29);
                printk("addr 0x18 value[0x29] %x\n", tp285x_read(client, 0x18));

                tp285x_write(client,0x19, 0x38);
                printk("addr 0x19 value[0x38] %x\n", tp285x_read(client, 0x19));

                tp285x_write(client,0x1a, 0x47);                
                printk("addr 0x1a value[0x47] %x\n", tp285x_read(client, 0x1a));

                tp285x_write(client,0x1c, 0x0a);  //1920*1080, 25fps
                printk("addr 0x1c value[0x0a] %x\n", tp285x_read(client, 0x1c));

                tp285x_write(client,0x1d, 0x50);  //
                printk("addr 0x1d value[0x50] %x\n", tp285x_read(client, 0x1d));

                tp285x_write(client,0x20, 0x3c);  
                printk("addr 0x20 value[0x3c] %x\n", tp285x_read(client, 0x20));

                tp285x_write(client,0x21, 0x46); 
                printk("addr 0x21 value[0x46] %x\n", tp285x_read(client, 0x21));

                tp285x_write(client,0x22, 0x36);
                printk("addr 0x22 value[0x36] %x\n", tp285x_read(client, 0x22));

                tp285x_write(client,0x23, 0x3c);
                printk("addr 0x23 value[0x3c] %x\n", tp285x_read(client, 0x23));

                tp285x_write(client,0x25, 0xfe);
                printk("addr 0x25 value[0xfe] %x\n", tp285x_read(client, 0x25));

                tp285x_write(client,0x26, 0x0d);
                printk("addr 0x26 value[0x0d] %x\n", tp285x_read(client, 0x26));

                tp285x_write(client,0x2b, 0x60);  
                printk("addr 0x2b value[0x60] %x\n", tp285x_read(client, 0x2b));

                tp285x_write(client,0x2c, 0x1a); 
                printk("addr 0x2c value[0x1a] %x\n", tp285x_read(client, 0x2c));

                tp285x_write(client,0x2d, 0x54);
                printk("addr 0x2d value[0x54] %x\n", tp285x_read(client, 0x2d));

                tp285x_write(client,0x2e, 0x40);
                printk("addr 0x2e value[0x40] %x\n", tp285x_read(client, 0x2e));

                tp285x_write(client,0x30, 0xa5);  
                printk("addr 0x30 value[0xa5] %x\n", tp285x_read(client, 0x30));

                tp285x_write(client,0x31, 0x86); 
                printk("addr 0x31 value[0x86] %x\n", tp285x_read(client, 0x31));

                tp285x_write(client,0x32, 0xfb);
                printk("addr 0x32 value[0xfb] %x\n", tp285x_read(client, 0x32));

                tp285x_write(client,0x33, 0x60);
                printk("addr 0x33 value[0x60] %x\n", tp285x_read(client, 0x33));

                tp285x_write(client,0x35, 0x05);
                printk("addr 0x35 value[0x05] %x\n", tp285x_read(client, 0x35));

                tp285x_write(client,0x38, 0x00); 
                printk("addr 0x38 value[0x0] %x\n", tp285x_read(client, 0x38));

                tp285x_write(client,0x39, 0x1C);    
                printk("addr 0x39 value[0x1c] %x\n", tp285x_read(client, 0x39));

                printk("addr 0xf5 %x\n",  tp285x_read(client, 0xf5) & 0xf0);
                tp285x_write(client,0xf5, tp285x_read(client,0xf5) & 0xf0);
                printk("addr 0xf5  %x\n", tp285x_read(client, 0xf5));
                //mipi setting
                tp285x_write(client,0x40, 0x08); //MIPI page 
                printk("addr 0x40 value[0x08] %x\n", tp285x_read(client, 0x40));

                tp285x_write(client,0x01, 0xf8);
                printk("addr 0x01 value[0xf8] %x\n", tp285x_read(client, 0x01));

                tp285x_write(client,0x02, 0x01);
                printk("addr 0x02 value[0x01] %x\n", tp285x_read(client, 0x02));

                tp285x_write(client,0x08, 0x0f);
                printk("addr 0x08 value[0x0f] %x\n", tp285x_read(client, 0x08));

                tp285x_write(client,0x10, 0x20);
                printk("addr 0x10 value[0x20] %x\n", tp285x_read(client, 0x10));

                tp285x_write(client,0x11, 0x47);
                printk("addr 0x11 value[0x47] %x\n", tp285x_read(client, 0x11));

                tp285x_write(client,0x12, 0x54);
                printk("addr 0x12 value[0x54] %x\n", tp285x_read(client, 0x12));

                tp285x_write(client,0x13, 0xef);
                printk("addr 0x13 value[0xef] %x\n", tp285x_read(client, 0x13));

                tp285x_write(client,0x20, 0x44);
                printk("addr 0x20 value[0x44] %x\n", tp285x_read(client, 0x20));

                tp285x_write(client,0x34, 0xe4);
                printk("addr 0x34 value[0xe4] %x\n", tp285x_read(client, 0x34));

                tp285x_write(client,0x14, 0x06);
                printk("addr 0x14 value[0x06] %x\n", tp285x_read(client, 0x14));

                tp285x_write(client,0x15, 0x00);        
                printk("addr 0x15 value[0x0] %x\n", tp285x_read(client, 0x15));

                tp285x_write(client,0x25, 0x08);
                printk("addr 0x25 value[0x08] %x\n", tp285x_read(client, 0x25));

                tp285x_write(client,0x26, 0x06);
                printk("addr 0x26 value[0x06] %x\n", tp285x_read(client, 0x26));

                tp285x_write(client,0x27, 0x11);
                printk("addr 0x27 value[0x11] %x\n", tp285x_read(client, 0x27));

                tp285x_write(client,0x0a, 0x80);
                printk("addr 0x0a value[0x80] %x\n", tp285x_read(client, 0x0a));

                tp285x_write(client,0x33, 0x0f);
                printk("addr 0x33 value[0x0f] %x\n", tp285x_read(client, 0x33));

                tp285x_write(client,0x33, 0x00);
                printk("addr 0x33 value[0x0] %x\n", tp285x_read(client, 0x33));

                tp285x_write(client,0x14, 0x86);
                printk("addr 0x14 value[0x86] %x\n", tp285x_read(client, 0x14));

                tp285x_write(client,0x14, 0x06);
                printk("addr 0x14 value[0x06] %x\n", tp285x_read(client, 0x14));

                /* Disable MIPI CSI2 output */
                tp285x_write(client,0x23, 0x02);
                printk("addr 0x23 value[0x02] %x\n", tp285x_read(client, 0x23));

                /* Enable MIPI CSI2 output */
                tp285x_write(client,0x23, 0x00);
                printk("addr 0x23 value[0x0] %x\n", tp285x_read(client, 0x23));
			}
			break;
		
	    case _IOC_NR(TP2850_SET_IMAGE_ADJUST):
	        if (copy_from_user(&image_adjust, argp, sizeof(tp285x_image_adjust)))
	        {
	            ret = FAILURE; 
	        }
			
			tp285x_set_image(client, &image_adjust);
	        break;

		case _IOC_NR(TP2850_GET_IMAGE_ADJUST):
			if (copy_from_user(&image_adjust, argp, sizeof(tp285x_image_adjust)))
	        {
	            return FAILURE;
	        }
			
			tp285x_get_image(client, &image_adjust);
			
			ret = copy_to_user(argp, &image_adjust, sizeof(tp285x_image_adjust));
	        if (ret == 0)
	        {
	            printk("tp285x get image success!\n");
	            return SUCCESS;       
	        }
	        else
	        {
	            printk("tp285x get image failed!\n");
	            return  FAILURE; 
	        }
			break;
		
		case _IOC_NR(TP2850_READ_REG):
	        if (copy_from_user(&register_data, argp, sizeof(tp285x_register)))
	        {
	            return FAILURE; 
	        }
	        tp2850_set_reg_page(client,register_data.ch);

	        register_data.value = tp285x_read(client,register_data.reg_addr);

	        ret = copy_to_user(argp, &register_data, sizeof(tp285x_register));
	        if (ret == 0)
	        {
	            printk("read reg:%d success\n", register_data.value);
	            return SUCCESS;       
	        }
	        else
	        {
	            printk("read reg ..!\n");
	            return  FAILURE; 
	        }
			break;
	   
	    case _IOC_NR(TP2850_WRITE_REG):
	       if (copy_from_user(&register_data, argp, sizeof(tp285x_register)))
	       {
	          	return FAILURE; 
	       }
		   tp2850_set_reg_page(client,register_data.ch);
	       tp285x_write(client, register_data.reg_addr, register_data.value); 
	       break;
		
	    case _IOC_NR(TP2850_DUMP_REG):
		{
	        unsigned int j = 0;
#if HI3516
			if (copy_from_user(&register_data, argp, sizeof(tp285x_register)))
	        {
	          	return FAILURE; 
	        }
			if(!strcmp(data->name,"tp2910"))
	        {
	            printk("\n......2910.......\n");
	            for(j = 0; j < 0xff; j++)
	            {
	                register_data.value = tp285x_read(client,j);  
	                printk("%02x:%02x\n", j, register_data.value );
	            }
	        }
	        else
#endif
	        {
	            tp2850_set_reg_page(client, MIPI_PAGE);
	            printk("\n......MIPI.......\n");
	            
	            for(j = 0; j < 0x40; j++)
	            {
	                register_data.value = tp285x_read(client,j);  
	                printk("%02x:%02x\n", j, register_data.value );
	            }
#if HI3516
	            printk("\n......VIDEO...........\n");
	            tp2850_set_reg_page(client, VIDEO_PAGE);
	            for(j = 0x00; j < 0xff; j++)
	            {
	                register_data.value = tp285x_read(client, j);
	                printk("%02x:%02x\n", j, register_data.value );
	            }
#endif
				tp285x_write(client, 0x40, 0x00);
				
				printk("\n......VIDEO.vin1..........\n");
				for(j = 0x00; j < 0xff; j++)
				{
					register_data.value = tp285x_read(client, j);
					printk("%02x:%02x\n", j, register_data.value );
				}
				
				tp285x_write(client, 0x40, 0x01);
				
				printk("\n......VIDEO.vin2..........\n");
				for(j = 0x00; j < 0xff; j++)
				{
					register_data.value = tp285x_read(client, j);
					printk("%02x:%02x\n", j, register_data.value );
				}
				
				tp285x_write(client, 0x40, 0x02);
				
				printk("\n......VIDEO.vin2..........\n");
				for(j = 0x00; j < 0xff; j++)
				{
					register_data.value = tp285x_read(client, j);
					printk("%02x:%02x\n", j, register_data.value );
				}
				
				tp285x_write(client, 0x40, 0x03);
				
				printk("\n......VIDEO.vin3..........\n");
				for(j = 0x00; j < 0xff; j++)
				{
					register_data.value = tp285x_read(client, j);
					printk("%02x:%02x\n", j, register_data.value );
				}
	        }

	        if (copy_to_user(argp, &register_data, sizeof(tp285x_register)))
	        {
	            printk("copying...\n");
	           	ret = FAILURE; 
	        }
			
	        break;
	    }
		
		case  _IOC_NR(TP285x_GET_CAMERA_RESOLUTION): 
		{
			tp285x_resolution tp285x_value;
			int 	channel = 0;
            char 	video_flag = 0xff;
			
            unsigned char	nRegValue_1 = 0x0;
            unsigned char	nRegValue_3 = 0x0;

			mutex_lock(&tp2854_CamRes_mutex);
			
			tp285x_write(client, 0x40, 0x00);

			msleep(10);

			for (channel = 0; channel < 4; channel++)
			{
				tp285x_write(client, 0x40, channel);

				msleep(10);

                video_flag = 0x00;

                nRegValue_1 = tp285x_read(client, 0x1);
                //printk("chn=%d, [0x1]=0x%X, vloss=%d,%d\n", channel, nRegValue_1, ((nRegValue_1 >> 7)&0x1), (nRegValue_1 & 0x1));

                nRegValue_3 = tp285x_read(client, 0x3);
                //printk("chn=%d, [0x3]=0x%X, \n", channel, ((nRegValue_3&0x7)));

                if ((((nRegValue_1 >> 7) & 0x1) == 0x1) || ((nRegValue_1 & 0x1) == 0x1))
                {
                    tp285x_value.chn_res[channel] = 0xff;
                    //printk("[%s, %d] chn=%d vloss, v_flag=0x%X\n", __FUNCTION__, __LINE__, channel, video_flag);
                }
                else
                {
                    tp285x_value.chn_res[channel] = tp285x_read(client, 0x03);
                    //printk("[%s, %d] chn=%d present, res=0x%x(0-720p60,1-720p50,2-1080p30,3-1080p25,4-720p30,5-720p25,6-sd)\n", \
                        __FUNCTION__, __LINE__, channel, tp285x_value.chn_res[channel]);
                }
			}         

			if (copy_to_user(argp, &tp285x_value, sizeof(tp285x_resolution)))
			{
				printk("copy_to_user failed \n");
				mutex_unlock(&tp2854_CamRes_mutex);
				return FAILURE; 
			}
			mutex_unlock(&tp2854_CamRes_mutex);
			break;
		}
		
		case  _IOC_NR(TP285x_SET_CAMERA_RESOLUTION): 
		{
			#if 0
			char *ppcCvStd[] =
			{
				"TP2854B_720P60",
				"TP2854B_720P50",
				"TP2854B_1080P30",
				"TP2854B_1080P25",
				"TP2854B_720P30",
				"TP2854B_720P25",
				"TP2854B_SD",
				"TP2854B_OTFM", /* Other formats */
				"TP2854B_720P60",
				"TP2854B_720P50",
				"TP2854B_1080P30",
				"TP2854B_1080P25",
				"TP2854B_720P30",
				"TP2854B_720P25",
				"TP2854B_SD",
				"TP2854B_OTFM", /* Other formats */
				"NULL", 
			};
			
			tp285x_resolution tp285x_value_s = {0xff};
			int channel_s = 0;

			mutex_lock(&tp2854_CamRes_mutex);
			
			if (copy_from_user(&tp285x_value_s, argp, sizeof(tp285x_value_s)))
			{
				printk("copy_from_user failed \n");
				mutex_unlock(&tp2854_CamRes_mutex);
				return FAILURE; 
			}
			
			printk(" TP285x_SET_CAMERA_RESOLUTION \n ch0 : 0x%02x %20s;\n ch1 : 0x%02x %20s;\n ch2 : 0x%02x %20s;\n ch3 : 0x%02x %20s; \n",
				tp285x_value_s.channel0, ppcCvStd[tp285x_value_s.channel0 & 0x07 ],
				tp285x_value_s.channel1, ppcCvStd[tp285x_value_s.channel1 & 0x07 ],
				tp285x_value_s.channel2, ppcCvStd[tp285x_value_s.channel2 & 0x07 ],
				tp285x_value_s.channel3, ppcCvStd[tp285x_value_s.channel3 & 0x07 ]);
			
			TP2854_common_init_t(client, &tp285x_value_s);

			msleep(10);
			
			for (channel_s = 0; channel_s < 4; channel_s++)
			{
				tp285x_write(client, 0x40, channel_s);
				
				msleep(10);
				
				if (channel_s == 0)
				{
					tp2854_set_video_mode(client, tp285x_value_s.channel0 , channel_s, STD_HDA);
				}
				else if (channel_s ==1)
				{
					tp2854_set_video_mode(client, tp285x_value_s.channel1 , channel_s, STD_HDA);
				}
				else if (channel_s ==2)
				{
					tp2854_set_video_mode(client, tp285x_value_s.channel2 , channel_s, STD_HDA);
				}
				else if (channel_s ==3)
				{
					tp2854_set_video_mode(client, tp285x_value_s.channel3 , channel_s, STD_HDA);
				}
			}
			mutex_unlock(&tp2854_CamRes_mutex);
			#endif
			break;
		}
		
        default:
        {
            printk("Invalid tp285x ioctl cmd!\n");
            return FAILURE;
        }
    }
	
    return SUCCESS;
}

static const struct file_operations tp285x_fops = 
{
	.owner		= THIS_MODULE,
	.open		= tp2850_open,
	.release	= tp2850_release,
    .unlocked_ioctl = tp2850_ioctl,
};

static int setup_dev_int(struct i2c_client *client)
{
	int res;
	
	struct tp285x_data *data;
	
	data = i2c_get_clientdata(client);
	
	cdev_init(&data->cdev, &tp285x_fops);
	
	data->cdev.owner = THIS_MODULE;
	data->client = client;

	mutex_init(&tp2854_CamRes_mutex);

	res = cdev_add(&data->cdev, MKDEV(TP2854B_MAJOR, 0), 1);
	if (res)
	{
		return -1;
	}

	data->class = class_create(THIS_MODULE, "tp2854");

	data->dev = device_create(data->class, NULL, MKDEV(TP2854B_MAJOR, 0), NULL, "tp2854");

	data->open_count = 0;
	
	return 0;
}

static int __init tp285x_driver_init(void)
{
	struct i2c_adapter* i2c_adap=NULL;

	printk("TP2854B driver init\n");	
    //tp_poweron_reset();

	//use i2c3
    i2c_adap = i2c_get_adapter(3);//获取adapter总线上的相应的I2C设备,参数是设备号
    tp2854b_client = i2c_new_device(i2c_adap, &tp2854b_info);
    i2c_put_adapter(i2c_adap);//释放adapter总线上的相应的I2C设备

    tp2854_init_data(tp2854b_client);

	return 0;
}

static void __exit tp285x_driver_exit(void)
{
	struct tp285x_data *data = i2c_get_clientdata(tp2854b_client);
	printk(KERN_INFO "%s\n", __func__);
	proc_debug_tp_uninit();
	cdev_del(&data->cdev);
    device_destroy(data->class, MKDEV(TP2854B_MAJOR, 0));
    class_destroy(data->class);
    devm_kfree(&tp2854b_client->dev, data);
    i2c_unregister_device(tp2854b_client);
}

module_init(tp285x_driver_init);
module_exit(tp285x_driver_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TechPoint TP2854B Linux Module");
