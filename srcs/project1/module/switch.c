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
        int led_state[4] = {LOW, LOW, LOW, LOW};

        switch (current_mode) {
            case MODE_ALL: // 전체 모드
                for (i = 0; i < 4; i++) gpio_set_value(led[i], HIGH);
                ssleep(2);
                for (i = 0; i < 4; i++) gpio_set_value(led[i], LOW);
                ssleep(2);
                break;

            case MODE_INDIVIDUAL: // 개별 모드
                led_state[i] = HIGH;
                update_leds(led_state);
                ssleep(2);
                gpio_set_value(led[i], LOW);
                i = (i + 1) % 4;
                break;

            case MODE_MANUAL: // 수동 모드
                // IRQ 플래그 확인 및 처리
                for (i = 0; i < 4; i++) {
                    if (irq_event_flag[i]) {
                        irq_event_flag[i] = 0; // 플래그 초기화
                        manual_led_state[i] = !manual_led_state[i];
                        gpio_set_value(led[i], manual_led_state[i]);
                        printk(KERN_INFO "Manual mode: LED[%d] toggled.\n", i);
                    }
                }
                break;

            case MODE_OFF: // 리셋 모드
                // 모든 LED 끄기
                update_leds((int[]){LOW, LOW, LOW, LOW});
                ssleep(1);
                break;
        }
    }

    update_leds((int[]){LOW, LOW, LOW, LOW}); // 종료 시 모든 LED OFF
    printk(KERN_INFO "kthread_function stopped\n");
    return 0;
}


volatile int irq_event_flag[4] = {0, 0, 0, 0}; // 스위치별 IRQ 플래그

irqreturn_t irq_handler(int irq, void *dev_id) {
    int i;

    for (i = 0; i < 4; i++) {
        if (irq == gpio_to_irq(sw[i])) {
            printk(KERN_INFO "Interrupt received on SW[%d] in mode %d\n", i, current_mode);

            // 스위치별 플래그 설정
            irq_event_flag[i] = 1;

            switch (current_mode) {
                case MODE_ALL:
                    printk(KERN_INFO "Handling IRQ in MODE_ALL\n");
                    // 전체 모드에 필요한 추가 동작
                    break;

                case MODE_INDIVIDUAL:
                    printk(KERN_INFO "Handling IRQ in MODE_INDIVIDUAL\n");
                    // 개별 모드에서는 플래그만 설정
                    break;

                case MODE_MANUAL:
                    printk(KERN_INFO "Handling IRQ in MODE_MANUAL\n");
                    manual_led_state[i] = !manual_led_state[i];
                    gpio_set_value(led[i], manual_led_state[i]);
                    break;

                case MODE_OFF:
                    printk(KERN_INFO "Handling IRQ in MODE_OFF\n");
                    // 리셋 또는 OFF 상태에서는 필요한 동작 수행
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
