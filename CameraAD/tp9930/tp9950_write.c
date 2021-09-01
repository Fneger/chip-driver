
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define	TP9930_DEV_PATH		"/dev/ahdCamera"

typedef struct _tp9930_register
{
    unsigned short reg_addr;
    unsigned short value;
} tp9930_register;

typedef struct _tp9930_power_set
{
	unsigned int	power_set;	/*0---PowerOff, 1---PowerOn*/
}tp9930_power_set;


#define TP9930_IOC_MAGIC		'v'
#define TP9930_WRITE_REG		_IOW(TP9930_IOC_MAGIC, 2, tp9930_register)
#define	TP9930_POWER_SET		_IOW(TP9930_IOC_MAGIC, 5, tp9930_power_set)

int main(int argc, char *argv[])
{
	int		nTmpVal = 0;

	char	*pRegStr = NULL;
	int		idx = 0;

	int		fd = -1;

	tp9930_register dev_reg = {0};
	tp9930_power_set dev_power = {0};
	
	if (argc != 3)
	{
		printf("[Usage]\n");
		printf("	./tp9930_write [reg_addr] [reg_val]\n\n");
		return -1;
	}

	/*reg_addr*/
	if (strlen(argv[1]) <= 2)
	{
		printf("reg addr len error(%d)\n", strlen(argv[1]));
		return -1;
	}

	if ((strncmp(argv[1], "0x", 2) != 0)
		&& (strncmp(argv[1], "0X", 2) != 0))
	{
		printf("reg addr invalid, should be [0xaa] or [0Xaa]\n");
		return -1;
	}

	pRegStr = argv[1];

	nTmpVal = 0;
	
	for (idx = 2; idx < strlen(argv[1]); idx++)
	{
		nTmpVal = nTmpVal*16;
		
		if ((pRegStr[idx] >= '0') && (pRegStr[idx] <= '9'))
		{
			nTmpVal += pRegStr[idx] - '0';
		}
		else if ((pRegStr[idx] >= 'a') && (pRegStr[idx] <= 'f'))
		{
			nTmpVal += pRegStr[idx] - 'a' + 10;
		}
		else if ((pRegStr[idx] >= 'A') && (pRegStr[idx] <= 'F'))
		{
			nTmpVal += pRegStr[idx] - 'A' + 10;
		}
		else
		{
			printf("reg addr [idx=%d, data=%d] invalid\n", idx, pRegStr[idx]);
			return -1;
		}
	}

	printf("reg_add=0x%X\n", nTmpVal);
	
	dev_reg.reg_addr = nTmpVal;

	/*reg_val*/
	if (strlen(argv[2]) <= 2)
	{
		printf("reg val len error(%d)\n", strlen(argv[2]));
		return -1;
	}

	if ((strncmp(argv[2], "0x", 2) != 0)
		&& (strncmp(argv[2], "0X", 2) != 0))
	{
		printf("reg al invalid, should be [0xaa] or [0Xaa]\n");
		return -1;
	}

	pRegStr = argv[2];

	nTmpVal = 0;
	
	for (idx = 2; idx < strlen(argv[2]); idx++)
	{
		nTmpVal = nTmpVal*16;
		
		if ((pRegStr[idx] >= '0') && (pRegStr[idx] <= '9'))
		{
			nTmpVal += pRegStr[idx] - '0';
		}
		else if ((pRegStr[idx] >= 'a') && (pRegStr[idx] <= 'f'))
		{
			nTmpVal += pRegStr[idx] - 'a' + 10;
		}
		else if ((pRegStr[idx] >= 'A') && (pRegStr[idx] <= 'F'))
		{
			nTmpVal += pRegStr[idx] - 'A' + 10;
		}
		else
		{
			printf("reg val [idx=%d, data=%d] invalid\n", idx, pRegStr[idx]);
			return -1;
		}
	}

	printf("reg_val=0x%X\n", nTmpVal);
	
	dev_reg.value = nTmpVal;

	fd = open(TP9930_DEV_PATH, O_RDWR);
	if (fd < 0)
	{
		printf("open file(%s) failed!\n", TP9930_DEV_PATH);
		return -1;
	}

#if 0
	dev_power.power_set = 0x1;
	if (ioctl(fd, TP9930_POWER_SET, &dev_power) != 0x0)
	{
		printf("[%s, %d] ioctl TP9930_POWER_SET failed!\n", __FILE__, __LINE__);
	}
#endif
	
	if (ioctl(fd, TP9930_WRITE_REG, &dev_reg) != 0)
	{
		printf("ioctl TP9930_WRITE_REG failed!\n");
	}
	else
	{
		printf("write reg(0x%X) to val(0x%X) success\n", dev_reg.reg_addr, dev_reg.value);
	}

	close(fd);
	fd = -1;
	
	return 0;
}

