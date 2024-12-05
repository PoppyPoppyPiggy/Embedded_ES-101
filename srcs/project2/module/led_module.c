#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

#define HIGH 1
#define LOW  0

int sw[4] = {4, 17, 27, 22};
int led[4] = {23, 24, 25, 1};

static struct timer_list timer;
static int mode = -1;
static int led_state[4] = {0, 0, 0, 0};
static int flag = 0;
static int led_index = 0;

static struct kobject *led_kobj;

static void timer_cb(struct timer_list *timer) {
    int i;

    if (mode == 0) {
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], flag ? LOW : HIGH);
        }
        flag = !flag;
    } else if (mode == 1) {
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], (i == led_index) ? HIGH : LOW);
        }
        led_index = (led_index + 1) % 4;
    }

    mod_timer(timer, jiffies + HZ * 2);
}

static ssize_t mode_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    return sprintf(buf, "%d\n", mode);
}

static ssize_t mode_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
    int new_mode, i;

    if (kstrtoint(buf, 10, &new_mode) != 0 || new_mode < -1 || new_mode > 3) {
        printk(KERN_ERR "Invalid mode: %s\n", buf);
        return -EINVAL;
    }

    mode = new_mode;

    if (mode == -1) {
        del_timer(&timer);
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], LOW);
        }
    } else {
        mod_timer(&timer, jiffies + HZ * 2);
    }

    return count;
}

static struct kobj_attribute mode_attribute = __ATTR(mode, 0664, mode_show, mode_store);

static int __init led_module_init(void) {
    int ret, i;

    for (i = 0; i < 4; i++) {
        ret = gpio_request(led[i], "LED");
        if (ret < 0) {
            printk(KERN_ERR "LED gpio_request failed for pin %d\n", led[i]);
            goto cleanup_gpio_led;
        }
        gpio_direction_output(led[i], LOW);
    }

    for (i = 0; i < 4; i++) {
        ret = gpio_request(sw[i], "SW");
        if (ret < 0) {
            printk(KERN_ERR "SW gpio_request failed for pin %d\n", sw[i]);
            goto cleanup_gpio_sw;
        }
        gpio_direction_input(sw[i]);
    }

    timer_setup(&timer, timer_cb, 0);

    led_kobj = kobject_create_and_add("led_control", kernel_kobj);
    if (!led_kobj) {
        ret = -ENOMEM;
        goto cleanup_gpio_sw;
    }

    ret = sysfs_create_file(led_kobj, &mode_attribute.attr);
    if (ret) {
        kobject_put(led_kobj);
        goto cleanup_gpio_sw;
    }

    return 0;

cleanup_gpio_sw:
    for (i = 0; i < 4; i++) {
        gpio_free(sw[i]);
    }
cleanup_gpio_led:
    for (i = 0; i < 4; i++) {
        gpio_free(led[i]);
    }
    return ret;
}

static void __exit led_module_exit(void) {
    int i;

    del_timer(&timer);

    for (i = 0; i < 4; i++) {
        gpio_free(sw[i]);
        gpio_free(led[i]);
    }

    sysfs_remove_file(led_kobj, &mode_attribute.attr);
    kobject_put(led_kobj);
}

module_init(led_module_init);
module_exit(led_module_exit);
MODULE_LICENSE("GPL");
