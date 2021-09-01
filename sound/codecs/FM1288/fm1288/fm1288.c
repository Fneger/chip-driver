/*
 * Copyright (C) 2012 Senodia.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/miscdevice.h>
#include <linux/i2c-dev.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/compiler.h>
#include "fm1288.h"

#define FM1288_I2C_ADDR 0x60
#define DRV_NAME "fm1288"
#define FM1288_MAJOR         	152

enum {
    DEBUG_INIT = 1U << 0,
    DEBUG_CONTROL_INFO = 1U << 1,
    DEBUG_ACCESS_REGS = 1U << 2,
};
static u32 debug_mask = 0x07;
#define dprintk(level_mask, fmt, arg...)	if (unlikely(debug_mask & level_mask)) \
    printk(KERN_DEBUG fmt , ## arg)

static const fm1288_register fm1288_regs[] = {

    {0x1E34 , 0x008B},
    {0x22C8 , 0x0024}, /* pll_div_type :8.192M / 4 根据MCLK配置*/
//    {0x22D0 , 0x3F03}, /* i2s_clk_set_8k :0F07 */

    /* MIC PGA 控制
       bit[3~0] mic0
       bit[7~4] mic1
       bit[11~8] linein
    */
    {0x22E5 , 0x0202},

    /* SPK out PGA
       bit[3~0] spk out gain
    */
    {0x22E9 , 0x0010},

    {0x22F2 , 0x0034}, /* mips_setting :003C */

    /* IIS master or slave */
    {0x22F9 , 0x005F},
    /* IIS   or  PCM  */
    {0x22FA , 0x003D},

    {0x2301 , 0x0012}, /* sample_rate :0012 */
    {0x2302 , 0x0001}, /* _num_of_mics :0012 */
    {0x2303 , 0x5DF1}, /* _kl_config : 5991 */


    /* MIC HPF */
    {0x2304 , 0x83DF},
    {0x2305 , 0x0033}, /* _ft_flag :0031 */



    /* MIC VOL CTL
       bit[7~0] mic0
       bit[15~8] mic1
    */
    {0x2307 , 0xF0F0},
    {0x2309 , 0x0000}, /* _lin_vol_ctl :0000 */




    /* MIC volume
       0x0000 : min
       0x7fff : max
       0x0100 : same as input
    */
    {0x230C , 0x0A00},
    {0x230D , 0x0100},  /* _spk_volume :0180 */



    /* MIC Mute */
    //{0x230F , 0xFFFF},
    /* MIC0 LPF */

    {0x2310 , 0x1280},  /* _linein_HPF_sel :1205 */
    {0x232D , 0x0001}, /* _reserve_232D[0] :0000 */
    /* AEC */
    {0x232F , 0x0088}, /* _aec_ref_gain :0080 */

    //{0x2332 , 0x0020},
    //{0x2333 , 0x0004},
    {0x2337 , 0x8000},
    {0x2339 , 0x0010}, /* _tdaec_delay_length :0010 */



    /* mic0 Gain */
    {0x2348 , 0x6000},

    /* mic1 Gain */
    {0x2349 , 0x4000},

    /* MIC Emph l滤波器系数*/
    {0x234A , 0x3487},
    {0x234B , 0x1144},
    //{0x234C , 0x3487},
    //{0x234D , 0x1144},
    {0x234E , 0x3487},
    {0x234F , 0x1144},

    /* FFP Setting */
    //{0x2360 , 0x07ff},
    //{0x2361 , 0x07ff},
    //{0x2362 , 0x07ff},

    /* Noise Suppression */
    {0x236E , 0x1500}, /* 值约小，噪音抑制越厉害，7fff不抑制 */
    {0x236F , 0x0500}, /* 噪音抑制因子 ，值越大，抑制越厉害 */
    {0x2370 , 0x0300},  /* 当没有大的回音时的噪音抑制因子，值越大，抑制越厉害 */

    {0x2382 , 0x0200},
    {0x2383 , 0x0300},
    {0x2384 , 0x0C00},  /* 信噪比阈值，值越大，抑制越厉害 */
    {0x2385 , 0x0E00},
    {0x2386 , 0x1000},

    {0x2390 , 0x4444},
    {0x2391 , 0x4444},
    {0x2393 , 0x4444},
    {0x2394 , 0x4444},
    {0x2395 , 0x4444},
    /* 当SNR非常高时，降低NS。 较大的值可以改善安静环境中的语音质量，但也可以提高本底噪声。
    因此，它需要与空闲噪声抑制一起调整*/
    //{0x239C , 0x1000},

    /* Non-Linear AEC */
    {0x23B3 , 0x0002},
    {0x23B4 , 0x0001},

    {0x23B5 , 0x3000},
    {0x23B7 , 0x0000},
    {0x23B8 , 0x0001},
    //{0x23BA , 0x0001},

    {0x23BE , 0x00E0},
    {0x23BF , 0x0054},
    //{0x23BB , 0x0054},


    {0x23ED , 0x0400},  /* idle noise threshold */
    {0x23EE , 0x2000},  /* idle ins attn */


    {0x22FB , 0x0000}
};
    

struct fm1288_data {
	struct i2c_client *client;
    struct cdev cdev;
    struct device *dev;
    struct class *class;
    int   open_count;
};

struct i2c_client *fm1288_client;

struct semaphore fm1288_rw_lock;

static struct fm1288_data *fm1288 = NULL;

static int fm1288_open(struct inode *inode, struct file *filp)
{
    filp->private_data = fm1288;

    printk(KERN_INFO"%s\n",__func__);

    return 0;
}

static int fm1288_release(struct inode *inode, struct file *file)
{
    // struct i2c_client *client = file->private_data;

    printk(KERN_INFO"%s\n",__func__);

    return 0;
}

static int fm1288_single_mem_write(struct i2c_client *client, u16 uc_addr,u16 uc_data)
{
    struct i2c_msg msg;
    int ret = -1;
    s32 retries = 0;
    u8 uc_addr_hight = uc_addr >> 8;
    u8 uc_addr_low = uc_addr;
    u8 uc_data_hight = uc_data >> 8;
    u8 uc_data_low = uc_data;
    u8 tmp[7] = {0xFC, 0xF3, 0x3B, uc_addr_hight, uc_addr_low, uc_data_hight, uc_data_low};

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
        dprintk(DEBUG_ACCESS_REGS, "[%s, %d] single write fail \n", __func__, __LINE__);
        return -1;
    }

    printk("[%s, %d]write 0x%x, data 0x%x\n ",  __func__, __LINE__, uc_addr, uc_data);
    return ret;
}

static int fm1288_fast_mem_read(struct i2c_client *client, u16 uc_addr, u16 *uc_data)
{
    struct i2c_msg msg;
    int ret = -1;
    s32 retries = 0;
    u8 tmp[5] = {0xFC, 0xF3, 0x37, (u8)(uc_addr >> 8), (u8)uc_addr};
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
        dprintk(DEBUG_ACCESS_REGS, "[%s, %d] single write fail %s %d\n", __func__, __LINE__);
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
        dprintk(DEBUG_ACCESS_REGS, "[%s, %d] single write fail %s %d\n", __func__, __LINE__);
        return -1;
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
        dprintk(DEBUG_ACCESS_REGS, "[%s, %d] single write fail %s %d\n", __func__, __LINE__);
        return -1;
    }


    *uc_data = val[1];
    *uc_data = (((*uc_data) << 8) | (val[0]));
    dprintk(DEBUG_ACCESS_REGS, "[%s, %d] read 0x%x data 0x%x\n", __func__, __LINE__,uc_addr, *uc_data);

    return 0;
}

static int fm1288_i2c_init(void)
{
    int ret	= -1;

    if(fm1288 != NULL)
    {
        int i = 0;
        int reg_num = sizeof (fm1288_regs) / sizeof (fm1288_register);
        for(i =0; i < reg_num; i++){
            fm1288_single_mem_write(fm1288->client, fm1288_regs[i].reg_addr, fm1288_regs[i].value);
        }
        return 0;
    }
    return ret;
}

static long fm1288_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    unsigned int __user 	*argp = (unsigned int __user *)arg;
    fm1288_register		dev_reg;
    long ret = 0;
    switch (cmd) {
    case FM1288_READ_REG:
    {
        if (copy_from_user(&dev_reg, argp, sizeof(fm1288_register))) {
            return -EINVAL;
        }

        down(&fm1288_rw_lock);
        ret = fm1288_fast_mem_read(fm1288_client, dev_reg.reg_addr, &dev_reg.value);
        up(&fm1288_rw_lock);
        if(ret < 0) {
            return -EINVAL;
        }

        if (copy_to_user(argp, &dev_reg, sizeof(fm1288_register))) {
            return -EPERM;
        }
    }
        break;

    case FM1288_WRITE_REG:
    {
        if (copy_from_user(&dev_reg, argp, sizeof(fm1288_register))) {
            return -EPERM;
        }

        down(&fm1288_rw_lock);
        ret = fm1288_single_mem_write(fm1288_client, dev_reg.reg_addr, dev_reg.value);
        up(&fm1288_rw_lock);
        if(ret < 0)
            return -EINVAL;
    }
        break;

    default:
        dprintk(DEBUG_CONTROL_INFO, "[%s, %d] Unsupport cmd(0x%X).\n", __FUNCTION__, __LINE__, cmd);
        return -EINVAL;
    }

    return 0;
}

static const struct file_operations fm1288_fops =
{
    .owner		= THIS_MODULE,
    .open		= fm1288_open,
    .release	= fm1288_release,
    .unlocked_ioctl = fm1288_ioctl,
};



static int setup_dev_int(struct i2c_client *client)
{
    int res;

    struct fm1288_data *data;

    data = i2c_get_clientdata(client);

    cdev_init(&data->cdev, &fm1288_fops);

    data->cdev.owner = THIS_MODULE;
    data->client = client;

    res = cdev_add(&data->cdev, MKDEV(FM1288_MAJOR, 0), 1);
    if (res)
    {
        return -1;
    }

    data->class = class_create(THIS_MODULE, DRV_NAME);

    data->dev = device_create(data->class, NULL, MKDEV(FM1288_MAJOR, 0), NULL, DRV_NAME);

    data->open_count = 0;
    sema_init(&fm1288_rw_lock, 1);

    return 0;
}

static int fm1288_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int err = 0;
	//FROTE_MEDIA_FUNC("fm1288_probe");
    
    dprintk(DEBUG_INIT, "fm1288 probe.. i2c_addr is %x\n", client->addr);
    
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dprintk(DEBUG_INIT, "SENODIA fm1288_probe: check_functionality failed.\n");
		err = -ENODEV;
		goto exit0;
	}
	/* Allocate memory for driver data */
	fm1288 = kzalloc(sizeof(struct fm1288_data), GFP_KERNEL);
	if (!fm1288) {
        dprintk(DEBUG_INIT, "SENODIA fm1288_probe: memory allocation failed.\n");
		err = -ENOMEM;
		goto exit0;
	}
    fm1288_client = client;
    i2c_set_clientdata(client, fm1288);

    err = setup_dev_int(client);
    if(err)
    {
        kfree(fm1288);
        fm1288 = NULL;
        return -1;
    }
    fm1288_i2c_init();
    return 0;
exit0:
	return err;

}

static int fm1288_remove(struct i2c_client *client)
{
	return 0;
}



static const struct i2c_device_id fm1288_id_table[] = {
    { DRV_NAME, 0 },
	{ },
};

static struct of_device_id fm1288_match_table[] = {
	{ .compatible = "fortemedia,fm1288", },
	{ },
};

static struct i2c_driver fm1288_driver = {
	.probe		= fm1288_probe,
	.remove 	= fm1288_remove,
	.id_table	= fm1288_id_table,
	.driver = {
        .name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = fm1288_match_table,
	},
};

static int __init fm1288_init(void)
{
    dprintk(DEBUG_INIT, "%s\n",__func__);
    return i2c_add_driver(&fm1288_driver);
}

static void __exit fm1288_exit(void)
{
    struct fm1288_data *data = i2c_get_clientdata(fm1288_client);
    dprintk(DEBUG_INIT, "%s\n", __func__);
    cdev_del(&data->cdev);
    device_destroy(data->class, MKDEV(FM1288_MAJOR, 0));
    class_destroy(data->class);
    devm_kfree(&fm1288_client->dev, data);
	i2c_del_driver(&fm1288_driver);
}

module_init(fm1288_init);
module_exit(fm1288_exit);

MODULE_AUTHOR("duanqm <duanqm@reachxm.com>");
MODULE_DESCRIPTION("forte media fm1288 linux driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("2.0.0");
