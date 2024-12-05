#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#define DEVICE_NAME "led_control"
#define CLASS_NAME "led_class"

#define HIGH 1
#define LOW  0

int sw[4] = {4, 17, 27, 22};
int led[4] = {23, 24, 25, 1};

static struct timer_list timer;
static int mode = 4;
static int led_state[4] = {0, 0, 0, 0};
static int flag = 0;
static int led_index = 0;

static int major_number;
static struct class *led_class = NULL;
static struct device *led_device = NULL;
static struct cdev led_cdev;

static void timer_cb(struct timer_list *timer) {
    int i;

    if (mode == 1) {
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], flag ? LOW : HIGH);
        }
        flag = !flag;
    } else if (mode == 2) {
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], (i == led_index) ? HIGH : LOW);
        }
        led_index = (led_index + 1) % 4;
    }

    mod_timer(timer, jiffies + HZ * 2);
}

// File operations
static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {
    char mode_str[3];
    int ret;

    sprintf(mode_str, "%d\n", mode);
    ret = copy_to_user(buf, mode_str, strlen(mode_str));
    if (ret != 0) {
        return -EFAULT;
    }
    return strlen(mode_str);
}

static ssize_t dev_write(struct file *file, const char __user *buf, size_t len, loff_t *offset) {
    char input[3];
    int new_mode, i;

    if (copy_from_user(input, buf, len)) {
        return -EFAULT;
    }
    input[len] = '\0';
    if (kstrtoint(input, 10, &new_mode) != 0 || new_mode < -1 || new_mode > 4) {
        printk(KERN_ERR "Invalid mode: %s\n", input);
        return -EINVAL;
    }

    mode = new_mode;

    if (mode == 4) {
        del_timer(&timer);
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], LOW);
        }
    } else {
        mod_timer(&timer, jiffies + HZ * 2);
    }

    return len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = dev_read,
    .write = dev_write,
};

static int __init led_module_init(void) {
    int ret, i;

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ERR "Failed to register char device\n");
        return major_number;
    }

    led_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(led_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(led_class);
    }

    led_device = device_create(led_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(led_device)) {
        class_destroy(led_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(led_device);
    }

    for (i = 0; i < 4; i++) {
        ret = gpio_request(led[i], "LED");
        if (ret < 0) {
            printk(KERN_ERR "LED gpio_request failed for pin %d\n", led[i]);
            goto cleanup_gpio_led;
        }
        gpio_direction_output(led[i], LOW);
    }

    timer_setup(&timer, timer_cb, 0);

    return 0;

cleanup_gpio_led:
    for (i = 0; i < 4; i++) {
        gpio_free(led[i]);
    }
    device_destroy(led_class, MKDEV(major_number, 0));
    class_destroy(led_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    return ret;
}

static void __exit led_module_exit(void) {
    int i;

    del_timer(&timer);

    for (i = 0; i < 4; i++) {
        gpio_free(led[i]);
    }

    device_destroy(led_class, MKDEV(major_number, 0));
    class_destroy(led_class);
    unregister_chrdev(major_number, DEVICE_NAME);
}

module_init(led_module_init);
module_exit(led_module_exit);
MODULE_LICENSE("GPL");
