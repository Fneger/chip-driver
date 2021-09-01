#include <stdio.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "strfunc.h"
#include "gpio_i2c.h"


int main(int argc , char* argv[])
{
	int fd = -1;
	int ret = 0;
	unsigned int device_addr, reg_addr ;
    int reg_value;

    SHI_Seq_S shiSeq;;
	memset(&shiSeq, 0, sizeof(SHI_Seq_S));
    
	if ((argc != 3) && (argc != 4))
    {
    	printf("usage: %s <device_addr> <reg_addr>. sample: %s 0x56 0x0\n", argv[0], argv[0]);
        return -1;
    }
	
	fd = open("/dev/i2c_gpio", 0);
    if (fd<0)
    {
    	printf("Open gpioi2c dev error!\n");
    	return -1;
    }
    
    if (StrToNumber(argv[1], &device_addr))
    {    	
    	return 0;
    }
       
    if (StrToNumber(argv[2], &reg_addr))
    {    
    	return 0;
    }
    
    if (3 == argc)
    {     
        shiSeq.devAddress = device_addr;  
        shiSeq.address = reg_addr;
        
        ret = ioctl(fd, GPIO_I2C_READ_SHORT, &shiSeq); 
        
        reg_value = shiSeq.data;
        
        printf("GPIO_I2C_READ_SHORT, dev_addr=0x%x reg_addr=0x%x value=0x%x\n", device_addr, reg_addr, reg_value);
        
    }
            
    return ret;
}
