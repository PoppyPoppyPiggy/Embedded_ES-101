#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>

#define HIGH 1
#define LOW  0

int led[4] = {23, 24, 25, 1}; // LED GPIO 번호
int sw[4] = {4, 17, 27, 22};  // 스위치 GPIO 번호

struct task_struct *thread_id = NULL;

enum Mode { MODE_OFF, MODE_ALL, MODE_INDIVIDUAL, MODE_MANUAL };
volatile enum Mode current_mode = MODE_OFF;
volatile int manual_led_state[4] = {0, 0, 0, 0}; // 수동 모드 상태

static int kthread_function(void *arg) {
    int i = 0;
    printk(KERN_INFO "kthread_function started\n");

    while (!kthread_should_stop()) {
        if (current_mode == MODE_ALL) { // 전체 모드
            for (i = 0; i < 4; i++) {
                gpio_set_value(led[i], HIGH);
            }
            ssleep(2);
            for (i = 0; i < 4; i++) {
                gpio_set_value(led[i], LOW);
            }
            ssleep(2);
        } else if (current_mode == MODE_INDIVIDUAL) { // 개별 모드
            gpio_set_value(led[i], HIGH);
            ssleep(2);
            gpio_set_value(led[i], LOW);
            i = (i + 1) % 4;
        }
    }

    for (i = 0; i < 4; i++) {
        gpio_set_value(led[i], LOW); // 종료 시 모든 LED 끄기
    }

    printk(KERN_INFO "kthread_function stopped\n");
    return 0;
}

irqreturn_t irq_handler(int irq, void *dev_id) {
    int i;
    for (i = 0; i < 4; i++) {
        if (irq == gpio_to_irq(sw[i])) {
            switch (i) {
                case 0: // 전체 모드
                    current_mode = MODE_ALL;
                    if (thread_id) kthread_stop(thread_id);
                    thread_id = kthread_run(kthread_function, NULL, "led_thread");
                    break;
                case 1: // 개별 모드
                    current_mode = MODE_INDIVIDUAL;
                    if (thread_id) kthread_stop(thread_id);
                    thread_id = kthread_run(kthread_function, NULL, "led_thread");
                    break;
                case 2: // 수동 모드
                    current_mode = MODE_MANUAL;
                    if (thread_id) {
                        kthread_stop(thread_id);
                        thread_id = NULL;
                    }
                    manual_led_state[i] = !manual_led_state[i];
                    gpio_set_value(led[i], manual_led_state[i]);
                    break;
                case 3: // 리셋 모드
                    current_mode = MODE_OFF;
                    if (thread_id) {
                        kthread_stop(thread_id);
                        thread_id = NULL;
                    }
                    for (i = 0; i < 4; i++) {
                        gpio_set_value(led[i], LOW);
                    }
                    break;
            }
            break;
        }
    }
    return IRQ_HANDLED;
}

static int led_kthread_init(void) {
    int i, ret;
    printk(KERN_INFO "Initializing LED module\n");

    for (i = 0; i < 4; i++) {
        if (!gpio_is_valid(led[i]) || !gpio_is_valid(sw[i])) {
            printk(KERN_ERR "Invalid GPIO %d or %d\n", led[i], sw[i]);
            return -EINVAL;
        }
        ret = gpio_request(led[i], "LED");
        if (ret < 0) {
            printk(KERN_ERR "Failed to request LED GPIO %d\n", led[i]);
            return ret;
        }
        gpio_direction_output(led[i], LOW);

        ret = gpio_request(sw[i], "SW");
        if (ret < 0) {
            printk(KERN_ERR "Failed to request SW GPIO %d\n", sw[i]);
            return ret;
        }

        ret = request_irq(gpio_to_irq(sw[i]), irq_handler, IRQF_TRIGGER_RISING, "gpio_irq", NULL);
        if (ret < 0) {
            printk(KERN_ERR "Failed to request IRQ for SW[%d]\n", i);
            return ret;
        }
    }

    return 0;
}

static void led_kthread_exit(void) {
    int i;
    printk(KERN_INFO "Exiting LED module\n");

    if (thread_id) {
        kthread_stop(thread_id);
        thread_id = NULL;
    }

    for (i = 0; i < 4; i++) {
        free_irq(gpio_to_irq(sw[i]), NULL);
        gpio_free(sw[i]);
        gpio_free(led[i]);
    }
}

module_init(led_kthread_init);
module_exit(led_kthread_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Simple LED control module");
