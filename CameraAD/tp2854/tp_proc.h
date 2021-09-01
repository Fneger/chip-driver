#ifndef TP_PROC__H
#define  TP_PROC__H

int proc_debug_tp_init(struct i2c_client *client);
int proc_debug_tp_uninit(void);
int proc_i2c_read(struct i2c_client *client, unsigned char reg);
int proc_i2c_write(struct i2c_client *client, unsigned char reg, unsigned char value);

#endif
