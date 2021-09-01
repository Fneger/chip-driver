static int fm1288_i2c_init(void)
{
    int ret    = -1;

    struct i2c_msg msgs[] = {
        {
             .addr = 0x60,
             .flags = 0,
             .len = sizeof(fm1288_init_data),
             .buf = fm1288_init_data,
         },
    };
    if(fm1288 != NULL)
    {
        int i = 0;
        for(i = 0; i< 3; i ++)
        {
            ret = i2c_transfer(fm1288->client->adapter, msgs, 1);
            if (ret < 0)
                dev_err(&fm1288->client->dev, "%s:ret=%d,i=%d, i2c write error.\n", __func__,ret,i);
        }
        FROTE_MEDIA_FUNC("ret = %d", ret);
        return 0;
    }
    return ret;
}

 

static int fm1288_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int err = 0;
    FROTE_MEDIA_FUNC("fm1288_probe");

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        printk(KERN_ERR "SENODIA fm1288_probe: check_functionality failed.\n");
        err = -ENODEV;
        goto exit0;
    }
    /* Allocate memory for driver data */
    fm1288 = kzalloc(sizeof(struct fm1288_data), GFP_KERNEL);
    if (!fm1288) {
        printk(KERN_ERR "SENODIA fm1288_probe: memory allocation failed.\n");
        err = -ENOMEM;
        goto exit0;
    }
    fm1288->client = client;
    i2c_set_clientdata(client, fm1288);
    if (client->dev.of_node) {
        err = fm1288_parse_dt(&client->dev);
        if (err) {
            dev_err(&client->dev, "DT parsing failed, err = %d\n", err);
            goto err_parse_dt;
        }
    }

    msleep(20);
    err = fm1288_gpio_configure();
    if(err){
        dev_err(&client->dev, "gpio configure failed,err = %d\n", err);
        goto err_gpio_configure;
    }
    msleep(20);
        gpio_direction_output(fm1288->reset_gpio, 1);
        msleep(20);
    err = fm1288_i2c_init();
    if(err){
        dev_err(&client->dev, "fm1288 init failed,err = %d\n", err);
        goto err_gpio_configure;
    }

    
err_gpio_configure:
err_power_on:

    //fm1288_power_init(false);
err_power_init:
/*
    if (!IS_ERR_OR_NULL(fm1288->fm1288_pinctrl)) {
        devm_pinctrl_put(fm1288->fm1288_pinctrl);
        fm1288->fm1288_pinctrl = NULL;
    }
    */
err_parse_dt:
    kfree(fm1288);
    fm1288 = NULL;
exit0:
    return err;

}