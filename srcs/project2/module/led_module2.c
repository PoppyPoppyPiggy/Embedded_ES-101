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
static int mode = 4; // Default mode: No operation
static int led_state[4] = {0, 0, 0, 0};

static int major_number;
static struct class *led_class = NULL;
static struct device *led_device = NULL;

// 타이머 콜백 함수 (Mode 1, 2 전용)
static void timer_cb(struct timer_list *timer) {
    int i;

    if (mode == 1) { // Mode 1: All LEDs blink
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], !led_state[i]);
            led_state[i] = !led_state[i];
        }
    } else if (mode == 2) { // Mode 2: Sequential LED lighting
        static int current_led = 0;

        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], (i == current_led) ? HIGH : LOW);
        }
        current_led = (current_led + 1) % 4;
    }

    mod_timer(timer, jiffies + HZ * 2);
}

// 읽기 함수: 현재 모드 반환
static ssize_t dev_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {
    char mode_str[3];
    int ret;

    snprintf(mode_str, sizeof(mode_str), "%d\n", mode);
    ret = copy_to_user(buf, mode_str, strlen(mode_str));
    return (ret == 0) ? strlen(mode_str) : -EFAULT;
}

// 쓰기 함수: 모드 변경 또는 LED 제어
static ssize_t dev_write(struct file *file, const char __user *buf, size_t len, loff_t *offset) {
    char input[3];
    int val, i;

    if (copy_from_user(input, buf, len)) {
        return -EFAULT;
    }
    input[len] = '\0';

    if (kstrtoint(input, 10, &val) != 0) {
        return -EINVAL; // Invalid input
    }

    if (val >= 0 && val <= 3 && mode == 3) { // Mode 3: Manual LED control
        led_state[val] = !led_state[val]; // Toggle LED state
        gpio_set_value(led[val], led_state[val]);
        printk(KERN_INFO "LED[%d] toggled to %d\n", val, led_state[val]);
    } else if (val == 4) { // Reset mode
        mode = 4;
        del_timer(&timer);
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], LOW);
            led_state[i] = 0;
        }
        printk(KERN_INFO "Mode reset. All LEDs off.\n");
    } else if (val == 1 || val == 2) { // Mode 1 or Mode 2
        mode = val;
        mod_timer(&timer, jiffies + HZ * 2);
        printk(KERN_INFO "Mode set to %d.\n", mode);
    } else if (val == 3) { // Enter Mode 3
        mode = 3;
        del_timer(&timer);
        printk(KERN_INFO "Mode set to 3: Manual control.\n");
    } else {
        printk(KERN_WARNING "Invalid input: %d. Ignored.\n", val);
    }

    return len;
}

// 파일 연산 구조체
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = dev_read,
    .write = dev_write,
};

// 모듈 초기화 함수
static int __init led_module_init(void) {
    int ret, i;

    printk(KERN_INFO "Initializing LED module\n");

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

    // GPIO 초기화
    for (i = 0; i < 4; i++) {
        ret = gpio_request(led[i], "LED");
        if (ret < 0) {
            printk(KERN_ERR "Failed to request GPIO %d\n", led[i]);
            return ret;
        }
        gpio_direction_output(led[i], LOW);
    }

    // 타이머 초기화
    timer_setup(&timer, timer_cb, 0);

    return 0;
}

// 모듈 종료 함수
static void __exit led_module_exit(void) {
    int i;

    del_timer(&timer);

    for (i = 0; i < 4; i++) {
        gpio_free(led[i]);
    }

    device_destroy(led_class, MKDEV(major_number, 0));
    class_destroy(led_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    printk(KERN_INFO "LED module exited\n");
}

module_init(led_module_init);
module_exit(led_module_exit);

MODULE_LICENSE("GPL");
