#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/gpio.h>

#define HIGH 1
#define LOW 0

// GPIO 번호 정의
int sw[4] = {4, 17, 27, 22};
int led[4] = {23, 24, 25, 1};

// 상태 변수
enum Mode { MODE_OFF, MODE_ALL, MODE_INDIVIDUAL, MODE_MANUAL };
volatile enum Mode current_mode = MODE_OFF;
volatile int manual_led_state[4] = {0, 0, 0, 0}; // 수동 모드 LED 상태

struct task_struct *thread_id = NULL;

// LED 상태 업데이트 함수
void update_leds(int state[]) {
    int i;
    for (i = 0; i < 4; i++) {
        gpio_set_value(led[i], state[i]);
    }
}

// 전체 모드 스레드
static int mode_all_function(void *arg) {
    int state = LOW;
    while (!kthread_should_stop()) {
        int led_state[4] = {state, state, state, state};
        update_leds(led_state);
        state = !state;
        ssleep(2); // 2초 간격
    }
    return 0;
}

// 개별 모드 스레드
static int mode_individual_function(void *arg) {
    int i = 0;
    while (!kthread_should_stop()) {
        int led_state[4] = {LOW, LOW, LOW, LOW};
        led_state[i] = HIGH; // 하나씩만 켜기
        update_leds(led_state);
        i = (i + 1) % 4;
        ssleep(2); // 2초 간격
    }
    return 0;
}

// 인터럽트 핸들러
irqreturn_t irq_handler(int irq, void *dev_id) {
    int i;
    for (i = 0; i < 4; i++) {
        if (irq == gpio_to_irq(sw[i])) {
            switch (i) {
                case 0: // 전체 모드
                    if (current_mode != MODE_ALL) {
                        current_mode = MODE_ALL;
                        if (thread_id) kthread_stop(thread_id);
                        thread_id = kthread_run(mode_all_function, NULL, "mode_all");
                    }
                    break;
                case 1: // 개별 모드
                    if (current_mode != MODE_INDIVIDUAL) {
                        current_mode = MODE_INDIVIDUAL;
                        if (thread_id) kthread_stop(thread_id);
                        thread_id = kthread_run(mode_individual_function, NULL, "mode_individual");
                    }
                    break;
                case 2: // 수동 모드
                    if (current_mode == MODE_MANUAL) {
                        manual_led_state[i] = !manual_led_state[i];
                        gpio_set_value(led[i], manual_led_state[i]);
                    } else {
                        current_mode = MODE_MANUAL;
                        if (thread_id) kthread_stop(thread_id);
                        update_leds((int[]){LOW, LOW, LOW, LOW});
                    }
                    break;
                case 3: // 리셋 모드
                    if (thread_id) kthread_stop(thread_id);
                    thread_id = NULL;
                    current_mode = MODE_OFF;
                    update_leds((int[]){LOW, LOW, LOW, LOW});
                    break;
            }
            break;
        }
    }
    return IRQ_HANDLED;
}

// 모듈 초기화 함수
static int __init switch_led_init(void) {
    int res, i;

    printk(KERN_INFO "Initializing switch_led module.\n");

    // GPIO 요청 및 초기화
    for (i = 0; i < 4; i++) {
        res = gpio_request(sw[i], "sw");
        if (res) {
            printk(KERN_ERR "Failed to request GPIO %d for sw[%d]\n", sw[i], i);
            goto fail_gpio;
        }
        res = request_irq(gpio_to_irq(sw[i]), irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "gpio_irq", NULL);
        if (res) {
            printk(KERN_ERR "Failed to request IRQ for sw[%d]\n", i);
            goto fail_irq;
        }
        res = gpio_request(led[i], "led");
        if (res) {
            printk(KERN_ERR "Failed to request GPIO %d for led[%d]\n", led[i], i);
            goto fail_led;
        }
        gpio_direction_output(led[i], LOW); // 초기 LED OFF
    }
    return 0;

fail_led:
    for (i = 0; i < 4; i++) gpio_free(led[i]);
fail_irq:
    for (i = 0; i < 4; i++) free_irq(gpio_to_irq(sw[i]), NULL);
fail_gpio:
    for (i = 0; i < 4; i++) gpio_free(sw[i]);
    return res;
}

// 모듈 종료 함수
static void __exit switch_led_exit(void) {
    int i;

    printk(KERN_INFO "Exiting switch_led module.\n");

    if (thread_id) kthread_stop(thread_id);

    for (i = 0; i < 4; i++) {
        free_irq(gpio_to_irq(sw[i]), NULL);
        gpio_free(sw[i]);
        gpio_free(led[i]);
    }
}

module_init(switch_led_init);
module_exit(switch_led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Switch and LED control module");
