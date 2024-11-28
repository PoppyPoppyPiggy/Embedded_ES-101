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
volatile int handler_busy = 0; // 인터럽트 핸들러 재진입 방지 플래그

// LED 상태 업데이트 함수
static void update_leds(int state[]) {
    int i;
    for (i = 0; i < 4; i++) {
        gpio_set_value(led[i], state[i]);
    }
}

// 커널 스레드 함수
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
                for (i = 0; i < 4; i++) {
                    if (manual_led_state[i]) {
                        gpio_set_value(led[i], manual_led_state[i]);
                        printk(KERN_INFO "Manual mode: LED[%d] is ON.\n", i);
                    } else {
                        gpio_set_value(led[i], LOW);
                        printk(KERN_INFO "Manual mode: LED[%d] is OFF.\n", i);
                    }
                }
                ssleep(1);
                break;

            case MODE_OFF: // 리셋 모드
                update_leds((int[]){LOW, LOW, LOW, LOW});
                ssleep(1);
                break;
        }
    }

    update_leds((int[]){LOW, LOW, LOW, LOW}); // 종료 시 모든 LED OFF
    printk(KERN_INFO "kthread_function stopped\n");
    return 0;
}

// 인터럽트 핸들러 함수
irqreturn_t irq_handler(int irq, void *dev_id) {
    int i;

    // 핸들러 재진입 방지
    if (handler_busy) {
        printk(KERN_INFO "Interrupt handler is busy, ignoring IRQ %d\n", irq);
        return IRQ_HANDLED;
    }

    handler_busy = 1; // 핸들러 시작

    for (i = 0; i < 4; i++) {
        if (irq == gpio_to_irq(sw[i])) {
            printk(KERN_INFO "Interrupt received on SW[%d] in mode %d\n", i, current_mode);

            // 기존 실행 중인 스레드 종료
            if (thread_id) {
                kthread_stop(thread_id);
                thread_id = NULL;
            }

            // 모드 설정
            switch (i) {
                case 0: // 전체 모드
                    current_mode = MODE_ALL;
                    thread_id = kthread_run(kthread_function, NULL, "all_mode_thread");
                    break;

                case 1: // 개별 모드
                    current_mode = MODE_INDIVIDUAL;
                    thread_id = kthread_run(kthread_function, NULL, "individual_mode_thread");
                    break;

                case 2: // 수동 모드
                    current_mode = MODE_MANUAL;
                    manual_led_state[i] = !manual_led_state[i];
                    thread_id = kthread_run(kthread_function, NULL, "manual_mode_thread");
                    break;

                case 3: // 리셋 모드
                    current_mode = MODE_OFF;
                    update_leds((int[]){LOW, LOW, LOW, LOW});
                    break;
            }
            break;
        }
    }

    handler_busy = 0; // 핸들러 종료
    return IRQ_HANDLED;
}

// 모듈 초기화 함수
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

        ret = request_irq(gpio_to_irq(sw[i]), irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "gpio_irq", NULL);
        if (ret < 0) {
            printk(KERN_ERR "Failed to request IRQ for SW[%d]\n", i);
            return ret;
        }
    }

    return 0;
}

// 모듈 종료 함수
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
