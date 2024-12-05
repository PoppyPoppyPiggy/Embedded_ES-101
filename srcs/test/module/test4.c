#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#define HIGH 1
#define LOW 0

// GPIO 핀 배열
int led[4] = {23, 24, 25, 1};  // LED GPIO 핀 번호
int sw[4] = {4, 17, 27, 22};   // 스위치 GPIO 핀 번호
int mod = 0;                   // 현재 모드

static struct delayed_work led_work;  // 지연 작업 구조체
static struct workqueue_struct *wq;  // 워크큐 구조체

// 모드 설명:
// 0: 모든 LED가 2초 간격으로 동시에 켜졌다 꺼짐
// 1: LED가 2초 간격으로 하나씩 순차적으로 켜짐
// 2: 수동 모드: 스위치를 누를 때마다 해당 LED를 토글
// 3: 리셋 모드: 모든 LED 끄기 및 모드 초기화

// 스위치 인터럽트 핸들러
irqreturn_t switch_irq_handler(int irq, void *dev_id) {
    int switch_mod = *(int *)dev_id;

    // 모드 설정
    switch (switch_mod) {
        case 0:
            mod = 0;  // 모든 LED 동시 모드
            printk(KERN_INFO "Mode changed: (Mode 0)\n");
            break;
        case 1:
            mod = 1;  // LED 순차적 모드
            printk(KERN_INFO "Mode changed: (Mode 1)\n");
            break;
        case 2:
            mod = 2;  // 수동 모드
            printk(KERN_INFO "Mode changed: (Mode 2)\n");
            break;
        case 3:
            mod = 3;  // 리셋 모드
            printk(KERN_INFO "Mode changed: (Mode 3 - Reset)\n");
            break;
    }

    // 수동 모드에서 LED 토글
    if (mod == 2) {
        int led_index = irq - gpio_to_irq(sw[0]); // 스위치로부터 LED 인덱스 계산
        if (gpio_get_value(led[led_index]) == LOW) {
            gpio_set_value(led[led_index], HIGH);  // LED 켜기
        } else {
            gpio_set_value(led[led_index], LOW);   // LED 끄기
        }
    }

    return IRQ_HANDLED;
}

// LED 동작 함수 (워크큐에서 실행)
void led_work_function(struct work_struct *work) {
    static int current_led = 0; // 현재 활성화된 LED 인덱스
    int i;

    if (mod == 3) {
        // 리셋 모드: 모든 LED 끄기
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], LOW);
        }
        printk(KERN_INFO "All LEDs turned off. Reset mode activated.\n");
        return; // 더 이상 작업 실행 안 함
    }

    switch (mod) {
        case 0: // 모든 LED가 2초 간격으로 동시에 켜졌다 꺼짐
            for (i = 0; i < 4; i++) {
                gpio_set_value(led[i], HIGH); // 모든 LED 켜기
            }
            msleep(2000);
            for (i = 0; i < 4; i++) {
                gpio_set_value(led[i], LOW); // 모든 LED 끄기
            }
            break;

        case 1: // LED가 순차적으로 2초 간격으로 켜짐
            gpio_set_value(led[current_led], HIGH); // 현재 LED 켜기
            msleep(2000);
            gpio_set_value(led[current_led], LOW);  // 현재 LED 끄기
            current_led = (current_led + 1) % 4;   // 다음 LED로 이동
            break;

        case 2: // 수동 모드: 동작 없음
            return;
    }

    // 작업을 다시 예약 (2초 후)
    queue_delayed_work(wq, &led_work, msecs_to_jiffies(2000));
}

// 모듈 초기화 함수
static int pj1_module_init(void) {
    int i;
    printk(KERN_INFO "Initializing module...\n");

    // GPIO 핀 초기화
    for (i = 0; i < 4; i++) {
        if (gpio_request(led[i], "LED") || gpio_direction_output(led[i], LOW)) {
            printk(KERN_ALERT "Failed to initialize LED GPIO %d\n", led[i]);
            return -1;
        }
        if (gpio_request(sw[i], "Switch") || gpio_direction_input(sw[i])) {
            printk(KERN_ALERT "Failed to initialize Switch GPIO %d\n", sw[i]);
            return -1;
        }
    }

    // 스위치에 인터럽트 요청
    for (i = 0; i < 4; i++) {
        int irq = gpio_to_irq(sw[i]);
        if (request_irq(irq, switch_irq_handler, IRQF_TRIGGER_RISING, "switch_irq", &sw[i])) {
            printk(KERN_ALERT "Failed to request IRQ for switch %d\n", sw[i]);
            return -1;
        }
    }

    // 워크큐 생성
    wq = create_singlethread_workqueue("led_workqueue");
    if (!wq) {
        printk(KERN_ALERT "Failed to create workqueue\n");
        return -1;
    }

    // 지연 작업 초기화 및 시작
    INIT_DELAYED_WORK(&led_work, led_work_function);
    queue_delayed_work(wq, &led_work, msecs_to_jiffies(2000));

    printk(KERN_INFO "Module initialized successfully.\n");
    return 0;
}

// 모듈 종료 함수
static void pj1_module_exit(void) {
    int i;
    printk(KERN_INFO "Exiting module...\n");

    // 워크큐 삭제
    cancel_delayed_work_sync(&led_work);
    destroy_workqueue(wq);

    // GPIO 핀 해제
    for (i = 0; i < 4; i++) {
        gpio_set_value(led[i], LOW);  // LED 끄기
        gpio_free(led[i]);
        gpio_free(sw[i]);
    }

    printk(KERN_INFO "Module exited successfully.\n");
}

module_init(pj1_module_init);
module_exit(pj1_module_exit);
MODULE_LICENSE("GPL");
