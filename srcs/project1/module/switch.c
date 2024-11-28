#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/delay.h>

#define HIGH 1
#define LOW  0

int led[4] = {23, 24, 25, 1};  // LED GPIO 번호
int sw[4] = {4, 17, 27, 22};   // 스위치 GPIO 번호

struct task_struct *thread_id = NULL;
static struct timer_list timer;

enum Mode { MODE_OFF, MODE_ALL, MODE_INDIVIDUAL, MODE_MANUAL };
volatile enum Mode current_mode = MODE_OFF;
volatile int manual_led_state[4] = {0, 0, 0, 0}; // 수동 모드 LED 상태

spinlock_t mode_lock; // 모드 변경 보호용 스핀락

// 타이머 콜백 함수
static void timer_cb(struct timer_list *timer) {
    int i;

    spin_lock(&mode_lock); // 모드 변경 보호용 락

    if (current_mode == MODE_ALL) {
        static int state = LOW;
        state = !state; // LED 상태 반전
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], state);
        }
        mod_timer(timer, jiffies + HZ * 2); // 2초 후 타이머 재설정
    } else if (current_mode == MODE_INDIVIDUAL) {
        static int current_led = 0;
        int led_state[4] = {LOW, LOW, LOW, LOW};
        led_state[current_led] = HIGH;
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], led_state[i]);
        }
        current_led = (current_led + 1) % 4;
        mod_timer(timer, jiffies + HZ * 2); // 2초 후 타이머 재설정
    }

    spin_unlock(&mode_lock); // 락 해제
}

// 커널 스레드 함수
static int kthread_function(void *arg) {
    while (!kthread_should_stop()) {
        if (current_mode == MODE_MANUAL) {
            int i;
            spin_lock(&mode_lock); // 모드 변경 보호용 락
            for (i = 0; i < 4; i++) {
                gpio_set_value(led[i], manual_led_state[i]);
            }
            spin_unlock(&mode_lock); // 락 해제
        }
        ssleep(1); // 1초 간격으로 수동 모드 상태 확인
    }
    return 0;
}

// 인터럽트 핸들러 함수
irqreturn_t irq_handler(int irq, void *dev_id) {
    int i;

    spin_lock(&mode_lock); // 모드 변경 보호용 락

    for (i = 0; i < 4; i++) {
        if (irq == gpio_to_irq(sw[i])) {
            printk(KERN_INFO "Interrupt received on SW[%d]\n", i);
            
            // 기존 실행 중인 스레드나 타이머 정지
            if (thread_id) {
                kthread_stop(thread_id);
                thread_id = NULL;
            }
            del_timer_sync(&timer);

            switch (i) {
                case 0: // 전체 모드
                    current_mode = MODE_ALL;
                    mod_timer(&timer, jiffies + HZ * 2); // 2초 후 타이머 설정
                    break;

                case 1: // 개별 모드
                    current_mode = MODE_INDIVIDUAL;
                    mod_timer(&timer, jiffies + HZ * 2); // 2초 후 타이머 설정
                    break;

                case 2: // 수동 모드
                    current_mode = MODE_MANUAL;
                    thread_id = kthread_run(kthread_function, NULL, "manual_mode_thread");
                    break;

                case 3: // 리셋 모드
                    current_mode = MODE_OFF;
                    for (i = 0; i < 4; i++) {
                        gpio_set_value(led[i], LOW);
                    }
                    break;
            }
            break;
        }
    }

    spin_unlock(&mode_lock); // 락 해제
    return IRQ_HANDLED;
}

// 모듈 초기화 함수
static int __init led_module_init(void) {
    int i, ret;

    printk(KERN_INFO "LED module initializing...\n");

    spin_lock_init(&mode_lock); // 스핀락 초기화

    // GPIO 요청 및 초기화
    for (i = 0; i < 4; i++) {
        if (!gpio_is_valid(led[i]) || !gpio_is_valid(sw[i])) {
            printk(KERN_ERR "Invalid GPIO %d or %d\n", led[i], sw[i]);
            return -EINVAL;
        }

        ret = gpio_request(led[i], "LED");
        if (ret) {
            printk(KERN_ERR "Failed to request GPIO for LED[%d]\n", i);
            return ret;
        }
        gpio_direction_output(led[i], LOW);

        ret = gpio_request(sw[i], "SW");
        if (ret) {
            printk(KERN_ERR "Failed to request GPIO for SW[%d]\n", i);
            return ret;
        }

        ret = request_irq(gpio_to_irq(sw[i]), irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "gpio_irq", NULL);
        if (ret) {
            printk(KERN_ERR "Failed to request IRQ for SW[%d]\n", i);
            return ret;
        }
    }

    // 타이머 설정
    timer_setup(&timer, timer_cb, 0);

    printk(KERN_INFO "LED module initialized successfully.\n");
    return 0;
}

// 모듈 종료 함수
static void __exit led_module_exit(void) {
    int i;

    printk(KERN_INFO "LED module exiting...\n");

    // 타이머 제거
    del_timer_sync(&timer);

    // 스레드 종료
    if (thread_id) {
        kthread_stop(thread_id);
    }

    // GPIO 및 IRQ 해제
    for (i = 0; i < 4; i++) {
        free_irq(gpio_to_irq(sw[i]), NULL);
        gpio_free(sw[i]);
        gpio_free(led[i]);
    }

    printk(KERN_INFO "LED module exited successfully.\n");
}

module_init(led_module_init);
module_exit(led_module_exit);
MODULE_LICENSE("GPL");
