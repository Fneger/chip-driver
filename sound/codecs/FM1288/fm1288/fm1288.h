#ifndef FM1288_H
#define FM1288_H

typedef struct _fm1288_register
{
    unsigned short reg_addr;
    unsigned short value;
} fm1288_register;

#define FM1288_IOC_MAGIC		'F'
#define FM1288_READ_REG			_IOWR(FM1288_IOC_MAGIC, 1, fm1288_register)
#define FM1288_WRITE_REG		_IOW(FM1288_IOC_MAGIC, 2, fm1288_register)

#endif // FM1288_H
