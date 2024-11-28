#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#define HIGH 1
#define LOW  0

int led[4] = {23, 24, 25, 1};  // LED GPIO 번호
int sw[4] = {4, 17, 27, 22};   // 스위치 GPIO 번호
int irq_num[4];                // 스위치의 IRQ 번호 배열

enum Mode { MODE_OFF, MODE_ALL, MODE_INDIVIDUAL, MODE_MANUAL };
volatile enum Mode current_mode = MODE_OFF;

struct task_struct *main_thread_id = NULL;     // 항상 동작하는 메인 스레드 구조체 포인터
struct task_struct *temp_thread_id = NULL;     // 임시 스레드 구조체 포인터

// 임시 스레드 함수: LED 제어
static int temp_thread_function(void *arg) {
    int i = 0;

    printk(KERN_INFO "Temporary thread started\n");

    while (!kthread_should_stop()) {
        switch (current_mode) {
            case MODE_ALL: // 전체 모드: 모든 LED를 2초 간격으로 켜고 끄기
                for (i = 0; i < 4; i++) {
                    gpio_set_value(led[i], HIGH);
                }
                msleep(2000);
                for (i = 0; i < 4; i++) {
                    gpio_set_value(led[i], LOW);
                }
                msleep(2000);
                break;

            case MODE_INDIVIDUAL: // 개별 모드: LED를 순차적으로 2초 간격으로 켜기
                gpio_set_value(led[i], HIGH);
                msleep(2000);
                gpio_set_value(led[i], LOW);
                i = (i + 1) % 4;  // 다음 LED로 이동
                break;

            case MODE_MANUAL: // 수동 모드는 스레드에서 처리하지 않음
            case MODE_OFF:    // 리셋 모드는 LED를 끈 상태 유지
                msleep(100);  // 0.1초 대기
                break;
        }
    }

    // 스레드 종료 시 모든 LED 끄기
    for (i = 0; i < 4; i++) {
        gpio_set_value(led[i], LOW);
    }

    printk(KERN_INFO "Temporary thread stopping\n");
    return 0;
}

// 인터럽트 핸들러 함수
irqreturn_t irq_handler(int irq, void *dev_id) {
    int i;

    for (i = 0; i < 4; i++) {
        if (irq == irq_num[i]) {
            printk(KERN_INFO "Interrupt received on SW[%d]\n", i);

            // 현재 실행 중인 임시 스레드가 있으면 종료
            if (temp_thread_id) {
                kthread_stop(temp_thread_id);
                temp_thread_id = NULL;
            }

            // 모드 설정 및 임시 스레드 생성
            switch (i) {
                case 0: // SW[0]: 전체 모드
                    current_mode = MODE_ALL;
                    temp_thread_id = kthread_run(temp_thread_function, NULL, "temp_thread");
                    if (IS_ERR(temp_thread_id)) {
                        printk(KERN_ERR "Failed to create temp_thread for MODE_ALL\n");
                        temp_thread_id = NULL;
                    }
                    break;

                case 1: // SW[1]: 개별 모드
                    current_mode = MODE_INDIVIDUAL;
                    temp_thread_id = kthread_run(temp_thread_function, NULL, "temp_thread");
                    if (IS_ERR(temp_thread_id)) {
                        printk(KERN_ERR "Failed to create temp_thread for MODE_INDIVIDUAL\n");
                        temp_thread_id = NULL;
                    }
                    break;

                case 2: // SW[2]: 수동 모드
                    current_mode = MODE_MANUAL;
                    gpio_set_value(led[i], !gpio_get_value(led[i]));  // 해당 LED 상태 토글
                    break;

                case 3: // SW[3]: 리셋 모드
                    current_mode = MODE_OFF;
                    // 모든 LED 끄기
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

// 메인 스레드 함수: 주기적으로 시스템 상태 점검 (필요에 따라 기능 추가 가능)
static int main_thread_function(void *arg) {
    printk(KERN_INFO "Main thread started\n");

    while (!kthread_should_stop()) {
        msleep(1000);  // 주기적으로 1초마다 상태 확인
        // 메인 스레드는 필요시 다른 작업을 추가로 수행 가능
    }

    printk(KERN_INFO "Main thread stopping\n");
    return 0;
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

        // LED GPIO 요청 및 초기화
        ret = gpio_request(led[i], "LED");
        if (ret) {
            printk(KERN_ERR "Failed to request GPIO for LED[%d]\n", i);
            return ret;
        }
        gpio_direction_output(led[i], LOW);  // 초기 LED 상태를 OFF로 설정

        // 스위치 GPIO 요청 및 인터럽트 설정
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

        // 모든 스위치에 대해 동일한 인터럽트 핸들러 설정
        ret = request_irq(irq_num[i], irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "gpio_irq", NULL);
        if (ret) {
            printk(KERN_ERR "Failed to request IRQ for SW[%d]\n", i);
            return ret;
        }
    }

    // 메인 스레드 생성
    main_thread_id = kthread_run(main_thread_function, NULL, "main_thread");
    if (IS_ERR(main_thread_id)) {
        printk(KERN_ERR "Failed to create main_thread\n");
        main_thread_id = NULL;
        return PTR_ERR(main_thread_id);
    }

    printk(KERN_INFO "LED module initialized successfully.\n");
    return 0;
}

// 모듈 종료 함수
static void __exit led_module_exit(void) {
    int i;

    printk(KERN_INFO "Exiting LED module\n");

    // 실행 중인 메인 스레드가 있으면 종료
    if (main_thread_id) {
        kthread_stop(main_thread_id);
        main_thread_id = NULL;
    }

    // 실행 중인 임시 스레드가 있으면 종료
    if (temp_thread_id) {
        kthread_stop(temp_thread_id);
        temp_thread_id = NULL;
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
