#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/initval.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <mach/hardware.h>
#include <linux/gpio.h>
#include "FM1288.h"

static short FM1288_FastMemoryRead(struct i2c_client *client, u8 uc_addressHightByte, u8 uc_addressLowByte );
static int FM1288_BurstModeMemoryWrite(struct i2c_client *client, s32 len, u8 *buf);
static int FM1288_SingleMemoryWrite(struct i2c_client *client, u8 uc_addressHightByte, u8 uc_addressLowByte, u8 uc_dataHightByte, u8 uc_dataLowByte);


static s32 FM1288_singleReadReg(struct i2c_client *client)
{
    s32 ret = -1;
	
    ret = FM1288_FastMemoryRead(client, 0x23, 0x90);
    	
    return ret;
}

static s32 FM1288_singleWriteReg(struct i2c_client *client)
{
    s32 ret = -1;
    
    ret = FM1288_SingleMemoryWrite(client, 0x23, 0x6E, 0x55, 0x55);
    	
    return ret;
}


static ssize_t is_cfm1288_get(struct device *dev, struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%s\n", "write reg to interface read all registers");
}


static ssize_t is_cfm1288_set(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    int ret = -1;
    struct i2c_client *client = to_i2c_client(dev);

    if(!client)
    {
        printk("error: func:%s; get null", __func__);
        return count;
    }

    if(!strncmp(buf, "read", 4))
    {
		ret = FM1288_singleReadReg(client);
    }

	if(!strncmp(buf, "write", 5))
	{
		ret = FM1288_singleWriteReg(client);
	}
	
    return count;
}

//************* sysfs node ************//

static DEVICE_ATTR(fm1288, S_IWUGO | S_IRUGO,
        is_cfm1288_get, is_cfm1288_set);

static struct attribute *fm1288_sysfs_attrs[] = {
    &dev_attr_fm1288.attr,
    NULL
};

static const struct attribute_group fm1288_sysfs = {
    .attrs  = fm1288_sysfs_attrs,
};


static int FM1288_GPIO_Init(void)
{
	if(0 > gpio_request(FM1288_RESET_PIN, "FM1288 reset"))
	{
		printk("%s %d error\n", __FUNCTION__, __LINE__);
		return -1;
	}
	
	if (0 > gpio_request(FM1288_PWDN_PIN, "FM1288 Power down"))
	{
		printk("%s %d error\n", __FUNCTION__, __LINE__);
		return -1;
	}
	
	return 0;
}

static int FM1288_SingleMemoryWrite(struct i2c_client *client,
									 u8 uc_addressHightByte,
									 u8 uc_addressLowByte,
									 u8 uc_dataHightByte,
									 u8 uc_dataLowByte)
{
	struct i2c_msg msg;
	s32 ret = -1;
	s32 retries = 0;
	u8 tmp[7] = {0xFC, 0xF3, 0x3B, uc_addressHightByte, uc_addressLowByte, uc_dataHightByte, uc_dataLowByte};

	msg.flags = !I2C_M_RD;
	msg.addr  = client->addr;
	msg.len   = 7;
	msg.buf   = tmp;

	while(retries < 5)
	{
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)break;
		retries++;
	}
	if((retries >= 5))
	{
		FM1288_ERROR("FM1288 single write fail %s %d\n", __FUNCTION__, __LINE__);  
	}

	printk("write 0x%x%x, data 0x%x%x\n ", tmp[3], tmp[4], tmp[5], tmp[6]);
	return ret;
}

static int FM1288_BurstModeMemoryWrite(struct i2c_client *client, s32 len, u8 *buf)
{
	struct i2c_msg msg;
	s32 ret = -1;
	s32 retries = 0;

	msg.flags = !I2C_M_RD;
	msg.addr  = client->addr;
	msg.len   = len;
	msg.buf   = buf;

	while(retries < 5)
	{
		printk("i2c cicle %d\n", retries);
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)break;
		retries++;
	}
	if (retries >= 5)
	{
		FM1288_ERROR("FM1288 burst mode write fail %s %d\n", __FUNCTION__, __LINE__);  
	}
	return ret;
}

static short FM1288_FastMemoryRead(struct i2c_client *client, u8 uc_addressHightByte, u8 uc_addressLowByte )
{
	struct i2c_msg msg;
	s32 ret = -1;
	s32 retries = 0;
	s16 regValue = 0x00;
	u8 tmp[5] = {0xFC, 0xF3, 0x37, uc_addressHightByte, uc_addressLowByte};
	u8 val[2] = {0x00, 0x00};

	msg.flags = !I2C_M_RD;
	msg.addr  = client->addr;
	msg.len   = 5;
	msg.buf   = tmp;

	while(retries < 5)
	{
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)break;
		retries++;
	}
	if((retries >= 5))
	{
		FM1288_ERROR("FM1288 single write fail %s %d\n", __FUNCTION__, __LINE__);  
		return -1;
	}

	struct i2c_msg read_msgs[2];
	u8 tmp_readArrayOne[] = {0xFC, 0xF3, 0x60, 0x25,};
    read_msgs[0].flags = !I2C_M_RD;
    read_msgs[0].addr  = client->addr;
    read_msgs[0].len   = 4;
    read_msgs[0].buf   = tmp_readArrayOne;

    read_msgs[1].flags = I2C_M_RD;
    read_msgs[1].addr  = client->addr;
    read_msgs[1].len   = 1;
    read_msgs[1].buf   = &val[0];//low byte

    while(retries < 5)
    {
        ret = i2c_transfer(client->adapter, read_msgs, 2);
        if(ret == 2)break;
        retries++;
    }
    if((retries >= 5))
    {
		FM1288_ERROR("FM1288 single write fail %s %d\n", __FUNCTION__, __LINE__);  
	}

	u8 tmp_readArrayTwo[] = {0xFC, 0xF3, 0x60, 0x26,};
    read_msgs[0].flags = !I2C_M_RD;
    read_msgs[0].addr  = client->addr;
    read_msgs[0].len   = 4;
    read_msgs[0].buf   = tmp_readArrayTwo;

    read_msgs[1].flags = I2C_M_RD;
    read_msgs[1].addr  = client->addr;
    read_msgs[1].len   = 1;
    read_msgs[1].buf   = &val[1];//high byte

    while(retries < 5)
    {
        ret = i2c_transfer(client->adapter, read_msgs, 2);
        if(ret == 2)break;
        retries++;
    }
    if((retries >= 5))
    {
		FM1288_ERROR("FM1288 single write fail %s %d\n", __FUNCTION__, __LINE__);  
	}

	printk("FM1288 read 0x%x%x data 0x%x%x\n", tmp[3], tmp[4], val[1], val[0]);
	regValue = val[1];
	regValue = ((regValue << 8) | (val[0]));

	return regValue;
}


static void FM1288_NormalPowerUpTiming(void)
{
	gpio_direction_output(FM1288_RESET_PIN, 0);
	gpio_direction_output(FM1288_PWDN_PIN,  0);
	gpio_set_value(FM1288_RESET_PIN, 0);	
	gpio_set_value(FM1288_PWDN_PIN, 0);
	msleep(2);
	gpio_set_value(FM1288_PWDN_PIN, 1);
	msleep(15);
	gpio_set_value(FM1288_RESET_PIN, 1);
	msleep(15);
	
	/*Parameters Programming*/
	
	/*90ms software initialization*/

	printk("%s %d--\n", __FUNCTION__, __LINE__);
}

static int FM1288_i2c_probe(struct i2c_client *i2c_client,
	const struct i2c_device_id *id)
{
	s32 ret = 0x00;
	printk("%s %d\n", __FUNCTION__, __LINE__);
	
	if (FM1288_GPIO_Init() < 0)
	{
		FM1288_ERROR("request reset and pwdn gpio fail\n");
		return -1;
	}
	
	FM1288_NormalPowerUpTiming();

	printk("FM1288 data length %d\n", sizeof(_mode0)/sizeof(_mode0[0]));

	FM1288_BurstModeMemoryWrite(i2c_client, sizeof(_mode0)/sizeof(_mode0[0]), _mode0);

	ret = sysfs_create_group(&i2c_client->dev.kobj, &fm1288_sysfs);
    if (ret)
        return ret;
	
	return 0;
}

static int FM1288_i2c_remove(struct i2c_client *i2c_client)
{
	snd_soc_unregister_codec(&i2c_client->dev);
	return 0;
}


/*register i2c driver*/
static const struct i2c_device_id FM1288_id[] = {
	{FM1288_I2c_NAME, 0},
	{}
};
static const struct i2c_driver FM1288_i2c_driver = {
	.driver = {
		.name = FM1288_I2c_NAME,
		.owner = THIS_MODULE,
	},
	.id_table = FM1288_id,
	.probe = FM1288_i2c_probe,
	.remove = FM1288_i2c_remove,
};	
	
	
	
static int __init FM1288_init(void)
{
	return i2c_add_driver(&FM1288_i2c_driver);
}
module_init(FM1288_init);

static void __exit FM1288_exit(void)
{
	i2c_del_driver(&FM1288_i2c_driver);
}
module_exit(FM1288_exit);

MODULE_LICENSE("GPL");


