/**
 * @author: Madhav dubey
 *
 * @descripsion : A driver for i2c based tcs34725 to learn Linux device drivers
 *
 * @file : tcs_driver.c
 */

#include<linux/module.h>
#include<linux/fs.h>
#include<linux/gpio.h>
#include<linux/cdev.h>
#include<linux/i2c.h>
#include<linux/uaccess.h>

#define MAX_BUFFER_SIZE 50
#define I2C_ADDRESS 0x29
#define I2C_BUS_AVAILABLE 1
#define SLAVE_DEVICE_NAME "TCS3725"

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("this driver will read data from tcs31725 colour sensor");
MODULE_AUTHOR("Madhav dubey");

/*
 * global variables
 */
static dev_t device_num;
struct cdev my_cdev;
static struct class *my_class;
char buffer[MAX_BUFFER_SIZE] = "empty_buffer";
static struct i2c_adapter *tcs_adapter;
static struct i2c_client *sensor;

static const struct i2c_device_id tcs_id[] = {
        { SLAVE_DEVICE_NAME, 0},
        { }
};

static struct i2c_driver tcs_driver = {
        .driver = {
                .name =SLAVE_DEVICE_NAME,
                .owner = THIS_MODULE
        }
};

static struct i2c_board_info tcs_board_info = {
        I2C_BOARD_INFO(SLAVE_DEVICE_NAME, I2C_ADDRESS)
};

/**
 * entry point for read syscall
 */
static ssize_t device_read(struct file *File, char *usr_buff, size_t count, loff_t *offs) {
        int to_copy, not_copied;
        int red = 0, green = 0, blue = 0;
        size_t buff_size = 0;
        printk("madhav: device read  is called, count:%ld\n", count);

        red = i2c_smbus_read_byte_data(sensor, 0x97);
        red = red *256;
        red += i2c_smbus_read_byte_data(sensor, 0x96);

        green = i2c_smbus_read_byte_data(sensor, 0x99);
        green = green *256;
        green += i2c_smbus_read_byte_data(sensor, 0x98);

        blue = i2c_smbus_read_byte_data(sensor, 0x9B);
        blue = blue *256;
        blue += i2c_smbus_read_byte_data(sensor, 0x9A);
        printk("r:%d g:%d, b:%d\n", red, green, blue);
        buff_size = snprintf(buffer, MAX_BUFFER_SIZE, "r:%d\ng:%d\nb:%d\n", red, green, blue);
        to_copy = min(count, buff_size);
        not_copied = copy_to_user(usr_buff, buffer, to_copy);
        if(*offs >= to_copy) return 0;
        *offs += to_copy;
        return to_copy - not_copied;
}
/**
 * entry point for write syscall, implemented for learning, no real use
 */
static ssize_t device_write(struct file *File, const char *usr_buff, size_t count, loff_t *offs) {
        int to_copy, not_copied;
        printk("madhav: device write  is called\n");
        to_copy = min(count, sizeof(buffer));
        not_copied = copy_from_user(buffer, usr_buff, to_copy);
        return to_copy-not_copied;
}

static int driver_open(struct inode *device_file, struct file *instance) {
        int ret = 0;
        char config[2] = {0};

        // Select enable register(0x80)
        // Power ON, RGBC enable, wait time disable(0x03)
        printk("madhav: device open is called\n");

        config[0] = 0x80;
        config[1] = 0x03;
        if(i2c_master_send(sensor, config, 2) < 0) {
                printk("%d, i2c master write failed", __LINE__);
        }
        // Select ALS time register(0x81)
        // Atime = 700 ms(0x00)
        config[0] = 0x81;
        config[1] = 0x00;
        ret = i2c_master_send(sensor, config, 2);

        // Select Wait Time register(0x83)
        // WTIME : 2.4ms(0xFF)
        config[0] = 0x83;
        config[1] = 0xFF;
        ret = i2c_master_send(sensor, config, 2);

        // Select control register(0x8F)
        // AGAIN = 1x(0x00)
        config[0] = 0x8F;
        config[1] = 0x00;
        ret = i2c_master_send(sensor, config, 2);
        printk("%d, ret:%d",__LINE__, ret);
        return 0;
}

int driver_close(struct inode *device_file, struct file *instance) {
        printk("madhav: device release is called\n");
        return 0;
}

static struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = driver_open,
        .release = driver_close,
        .read = device_read,
        .write = device_write
};

static int __init module_entry(void) {
        int ret = 0;

        printk("this is module madhav's init function\n");
        ret = alloc_chrdev_region(&device_num, 0, 1, "gpio_driver_dev");
        if(ret<0) printk("alloc_region failed\n");
        printk("device number major:%d, minor:%d", MAJOR(device_num), MINOR(device_num));
        //device_num
        my_class = class_create(THIS_MODULE, "gpio_mad_class");
        device_create(my_class, NULL, device_num, NULL, "tcs_34725"); //create device file
        cdev_init(&my_cdev, &fops);
        ret = cdev_add(&my_cdev, device_num, 1);
        if(ret<0) printk("cdev_add failed\n");

        tcs_adapter = i2c_get_adapter(I2C_BUS_AVAILABLE);

        if(tcs_adapter) {
                sensor = i2c_new_client_device(tcs_adapter, &tcs_board_info);
                if(sensor) {
                        if(i2c_add_driver(&tcs_driver) != -1) {
                                ret = 0;
                        }
                        else
                                printk("can't add driver maddy\n");
                }
                i2c_put_adapter(tcs_adapter);
        }
        else {
        printk("i2c_get_adapter failed\n");
        }
                return 0;
}

static void __exit mod_exit(void) {
        i2c_unregister_device(sensor);
        i2c_del_driver(&tcs_driver);
        printk("tcs_driver exit, this is module exit\n");
        cdev_del(&my_cdev);
        device_destroy(my_class, device_num);
        class_destroy(my_class);
        unregister_chrdev_region(device_num, 1);
}

module_init(module_entry);
module_exit(mod_exit);