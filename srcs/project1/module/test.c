#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#define HIGH 1
#define LOW  0

int led[4] = {23, 24, 25, 1};  // LED GPIO 번호
int sw[4] = {4, 17, 27, 22};   // 스위치 GPIO 번호
int irq_num[4];  // 스위치의 IRQ 번호 배열

enum Mode { MODE_OFF, MODE_ALL, MODE_INDIVIDUAL, MODE_MANUAL };
volatile enum Mode current_mode = MODE_OFF;
volatile int manual_led_state[4] = {0, 0, 0, 0};  // 수동 모드 LED 상태

static struct timer_list led_timer;
struct task_struct *thread_id = NULL;

DEFINE_SPINLOCK(mode_lock);  // 모드 변경 보호용 스핀락

// 타이머 콜백 함수: 전체 모드 및 개별 모드 처리
static void timer_cb(struct timer_list *timer) {
    int i;
    static int current_led = 0;
    static int state = LOW;

    spin_lock(&mode_lock);  // 모드 변경 보호용 락

    if (current_mode == MODE_ALL) {
        state = !state;  // LED 상태 반전
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], state);
        }
    } else if (current_mode == MODE_INDIVIDUAL) {
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], LOW);
        }
        gpio_set_value(led[current_led], HIGH);
        current_led = (current_led + 1) % 4;
    }

    mod_timer(&led_timer, jiffies + HZ * 2);  // 2초 후 타이머 재설정

    spin_unlock(&mode_lock);  // 락 해제
}

// 수동 모드에서 LED 상태를 갱신하는 함수
static int manual_mode_thread(void *arg) {
    while (!kthread_should_stop()) {
        int i;
        spin_lock(&mode_lock);
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], manual_led_state[i]);
        }
        spin_unlock(&mode_lock);
        msleep(100);
    }
    return 0;
}

// 인터럽트 핸들러 함수
irqreturn_t irq_handler(int irq, void *dev_id) {
    int i;

    spin_lock(&mode_lock);

    for (i = 0; i < 4; i++) {
        if (irq == irq_num[i]) {
            printk(KERN_INFO "Interrupt received on SW[%d]\n", i);

            // 기존 실행 중인 스레드 종료 및 타이머 중지
            if (thread_id) {
                kthread_stop(thread_id);
                thread_id = NULL;
            }
            del_timer_sync(&led_timer);

            // 모드 설정
            switch (i) {
                case 0: // 전체 모드
                    current_mode = MODE_ALL;
                    mod_timer(&led_timer, jiffies + HZ * 2);
                    break;

                case 1: // 개별 모드
                    current_mode = MODE_INDIVIDUAL;
                    mod_timer(&led_timer, jiffies + HZ * 2);
                    break;

                case 2: // 수동 모드
                    current_mode = MODE_MANUAL;
                    manual_led_state[i] = !manual_led_state[i];
                    if (!thread_id) {
                        thread_id = kthread_run(manual_mode_thread, NULL, "manual_mode_thread");
                    }
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

    spin_unlock(&mode_lock);
    return IRQ_HANDLED;
}

// 모듈 초기화 함수
static int __init led_module_init(void) {
    int ret, i;

    printk(KERN_INFO "Initializing LED module\n");

    // GPIO 요청 및 초기화
    for (i = 0; i < 4; i++) {
        if (!gpio_is_valid(led[i]) || !gpio_is_valid(sw[i])) {
            printk(KERN_ERR "Invalid GPIO: LED[%d] or SW[%d]\n", led[i], sw[i]);
            return -EINVAL;
        }

        ret = gpio_request(led[i], "LED");
        if (ret) {
            printk(KERN_ERR "Failed to request GPIO for LED[%d]\n", i);
            return ret;
        }
        gpio_direction_output(led[i], LOW);  // 초기 LED 상태를 OFF로 설정

        ret = gpio_request(sw[i], "SW");
        if (ret) {
            printk(KERN_ERR "Failed to request GPIO for SW[%d]\n", i);
            return ret;
        }

        irq_num[i] = gpio_to_irq(sw[i]);
        if (irq_num[i] < 0) {
            printk(KERN_ERR "Failed to get IRQ number for SW[%d]\n", i);
            return irq_num[i];
        }

        ret = request_irq(irq_num[i], irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "gpio_irq", NULL);
        if (ret) {
            printk(KERN_ERR "Failed to request IRQ for SW[%d]\n", i);
            return ret;
        }
    }

    // 타이머 설정
    timer_setup(&led_timer, timer_cb, 0);

    printk(KERN_INFO "LED module initialized successfully.\n");
    return 0;
}

// 모듈 종료 함수
static void __exit led_module_exit(void) {
    int i;

    printk(KERN_INFO "Exiting LED module\n");

    // 타이머 제거
    del_timer_sync(&led_timer);

    // 스레드 종료
    if (thread_id) {
        kthread_stop(thread_id);
    }

    // IRQ 및 GPIO 해제
    for (i = 0; i < 4; i++) {
        free_irq(irq_num[i], NULL);
        gpio_free(sw[i]);
        gpio_free(led[i]);
    }

    printk(KERN_INFO "LED module exited successfully.\n");
}

module_init(led_module_init);
module_exit(led_module_exit);
MODULE_LICENSE("GPL");
