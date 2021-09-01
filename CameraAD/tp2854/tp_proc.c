#include <linux/irq.h>
#include <linux/gfp.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>

#include "tp_proc.h"

#define DIR_NAME 		"tp285x"
#define KBUFFER_SIZE	(1024*4)

static struct proc_dir_entry *root_tp_dir = NULL;
static struct proc_dir_entry *file = NULL;

static struct i2c_client *g_client = NULL;

#if 0
static void *tp_start(struct seq_file *seq, loff_t *pos)
{
	if( *pos > KBUFFER_SIZE-1){
		return NULL;
	}else{
		
		return  pos;
	}
}

static void *tp_next(struct seq_file *seq, void *v, loff_t *pos)
{

	(*pos)++;
	if(*pos > KBUFFER_SIZE-1 ){
		return  NULL;
	}else{
		*(((unsigned char *)seq->private)+ *pos) = proc_i2c_read(g_client, 0x42);
		return pos; 
	}
}

static void tp_stop(struct seq_file *seq, void *v)
{
	return ;
}

static int tp_show(struct seq_file *seq, void *v)
{
//	loff_t *pos = v;

	seq_printf(seq, "%x\n", *((unsigned char *)seq->private) );
	return 0;
}
#endif

#if 0
static const struct seq_operations tp_seq_fops = {
	.start	= tp_start,
	.next	= tp_next,
	.stop	= tp_stop,
	.show	= tp_show,
};
#endif

static int proc_seq_tp_open(struct inode *inode, struct file *file)
{
#if 0
	struct seq_file *seq;
	int ret = 0;
	
	ret = seq_open(file, &tp_seq_fops);

	seq = file->private_data;
	seq->private = (void *)kzalloc(KBUFFER_SIZE, GFP_KERNEL);
	return ret;
#endif
	return 0;	
}

#if 1

#if 0
static unsigned char register_addr[] = {
	0X40,0X41,0X46,0X47,0X4C,0X4E, 0X4F,0X50,
	0XAF,0XB0,0XB1,0XB2,0XB3,0XB4, 0XB5,0XB6,
	0XB7,0XB8,0XB9,0XBE,0XE5,0XE6, 0XE7,0XE8,
	0XE9,0XEA,0XEB,0XEC,0XED,0XEE, 0XEF,0XF0,
	0XF3,0XF4,0XF5,0XF6,0XFA,0XFC, 0XFD,0XFE,
	0XFF};

static unsigned char register_addr_decoder[]={
	0x00,0x01,0x02,0x03,0x04,0x06,0x07,
	0x08,0x09,0x0a,0x10,0x11,0x12,0x13,0x14,
	0x15,0x20,0x21,0x22,0x23,0x24,0x25,0x26,
	0x27,0x28,0x31,0x32,0x33,0x34,0x35,0x36,
	0x37,0x38
};
#endif

static ssize_t proc_seq_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	int count = 0;
	
	printk("\n......MIPI.......\n");
	proc_i2c_write(g_client, 0x40, 0x8);
	for(count=0; count < 0x40; count++){
		printk("addr:0x%02x	value:0x%02x\n",count, proc_i2c_read(g_client, count));
	}

	printk("\n......VIDEO...........\n");
	proc_i2c_write(g_client, 0x40, 0x0);
	for(count = 0; count< 0xff; count++){
		printk("addr:0x%02x	value:0x%02x\n", count,
			proc_i2c_read(g_client, count));
	}
	
	printk("\n......VIDEO.vin0..........\n");
	proc_i2c_write(g_client, 0x40, 0x0);
	for(count = 0x0; count< 0xff; count++){
		printk("addr:0x%02x	value:0x%02x\n", count,
			proc_i2c_read(g_client, count));			
	}

	printk("\n......VIDEO.vin1..........\n");
	proc_i2c_write(g_client, 0x40, 0x1);
	for(count = 0x0; count< 0xff; count++){
		printk("addr:0x%02x	value:0x%02x\n", count,
			proc_i2c_read(g_client, count));			
	}

	printk("\n......VIDEO.vin2..........\n");
	proc_i2c_write(g_client, 0x40,0x02);
	for(count = 0; count< 0xff; count++){
		printk("addr:0x%02x	value:0x%02x\n", count,
			proc_i2c_read(g_client, count));			
	}

	printk("\n......VIDEO.vin3..........\n");
	proc_i2c_write(g_client, 0x40,0x03);
	for(count = 0; count< 0xff; count++){
		printk("addr:0x%02x	value:0x%02x\n", count,
			proc_i2c_read(g_client, count));			
	}
	return 0;
}

static ssize_t proc_seq_write (struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	unsigned char buffer[20] = {0};
	int count = 0;
	int ret = 0;
	int index = 0;
	int flag = 0;
	int elem =0;
	unsigned char addr = 0;
	unsigned char value = 0;
	unsigned char temp[2][10] = {0};
	if(size >= 20){
		count = 20;
	}else{
		count = size;
	}
	ret = copy_from_user(buffer, buf, count);
	if(ret){
		return -EFAULT;
	}
//	printk("buffer %s\n", buffer);
	
	for(count = 0; count < 20; count++ ){
		
		if(buffer[count] != ' ' && index <= 1 && elem < 2 ){
			if((buffer[count]<='9' && buffer[count] >= '0')||
				(buffer[count]>='a' && buffer[count] <= 'f')||
				(buffer[count]>='A' && buffer[count] <= 'F')){
				
					temp[index][elem++] = buffer[count];
					flag = 1;
			}
		}else{
			if(flag == 1){
				index++;
				elem = 0;
				flag = 0;
			}
		}
	}
	ret = kstrtou8(temp[0],16, &addr);
	if(ret){
		printk("convter is failed of from str to hex\n");
	}
	ret = kstrtou8(temp[1], 16, &value);
	if(ret){
		printk("convter is failed of from str to hex\n");
	}
	//printk("addr: %02x  value %02x\n", addr, value);
	proc_i2c_write(g_client, addr, value);
	
	printk("add:%x value:%x\n", addr, proc_i2c_read(g_client, addr));
	return size;
}
#endif

int proc_seq_release(struct inode *inode, struct file *file)
{
#if 0
	 struct seq_file *s = file->private_data;

     kfree(s->private);
     return seq_release(inode, file);
#endif
	return  0;
}

static struct file_operations proc_seq_fops = 
{
	.owner		= THIS_MODULE,
	.open		= proc_seq_tp_open,
	.read		= proc_seq_read,
	.write		= proc_seq_write,
	//.llseek		= seq_lseek,
	.release	= proc_seq_release,
};

int proc_debug_tp_init(struct i2c_client *client)
{
	g_client = client;
	printk("proc_debug_tp_init\n");

	root_tp_dir = proc_mkdir(DIR_NAME, NULL); 
	if (!root_tp_dir)
		return -1;

	file = proc_create_data("tp285x", 0666, root_tp_dir, &proc_seq_fops, NULL);
	if (!file)
	{
		printk("create tp285x file is failed\n");
		return  -1;
	}
	
	return 0;
}

int proc_debug_tp_uninit(void)
{	
	printk("uninstall tp2854 proc fs\n");
	
	if(file && root_tp_dir){
		remove_proc_entry("tp285x", root_tp_dir);
		proc_remove(root_tp_dir);
		return 0;
	}
	return -1;
	
}

