
#ifndef _GPIO_I2C_H
#define _GPIO_I2C_H

#if 1
#define GPIO_I2C_MAGIC_BASE	'I'
//#define GPIO_I2C_READ_BYTE   _IOR(GPIO_I2C_MAGIC_BASE,0x01,int)
//#define GPIO_I2C_WRITE_BYTE  _IOW(GPIO_I2C_MAGIC_BASE,0x02,int)

//#define GPIO_I2C_READ_DWORD   _IOR(GPIO_I2C_MAGIC_BASE,0x03,int)
//#define GPIO_I2C_WRITE_DWORD  _IOR(GPIO_I2C_MAGIC_BASE,0x04,int)

#define GPIO_I2C_READ_SHORT   _IOR(GPIO_I2C_MAGIC_BASE,0x05,int)
#define GPIO_I2C_WRITE_SHORT  _IOR(GPIO_I2C_MAGIC_BASE,0x06,int)
#else
#define GPIO_I2C_READ_BYTE    0x01
#define GPIO_I2C_WRITE_BYTE   0x02

#define GPIO_I2C_READ_DWORD   0x03
#define GPIO_I2C_WRITE_DWORD  0x04
#endif

#define MEM_WRITE 0x3B 
#define MEM_READ  0x37
#define REG_READ  0x60


typedef struct _SHISeq
{
    unsigned char devAddress;
    
    unsigned short syncWord; //0xFCF3

    unsigned short commandEntry; //MEM_WRITE or MEM_READ or REG_READ

    unsigned short address;

    unsigned short data;
    
}SHI_Seq_S;

unsigned char gpio_i2c_read(unsigned char devaddress, unsigned char address);
void          gpio_i2c_write(unsigned char devaddress, unsigned char address, unsigned char value);
unsigned int  gpio_i2c2_read(unsigned char devaddress, unsigned short address, int num_bytes);
void          gpio_i2c2_write(unsigned char devaddress, unsigned short address, unsigned int data, int num_bytes);

#endif

