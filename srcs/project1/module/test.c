#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#define HIGH 1
#define LOW  0

int led[4] = {23, 24, 25, 1};  // LED GPIO 번호
int sw[4] = {4, 17, 27, 22};   // 스위치 GPIO 번호
int irq_num[4];                // 스위치의 IRQ 번호 배열

enum Mode { MODE_OFF, MODE_ALL, MODE_INDIVIDUAL, MODE_MANUAL };
volatile enum Mode current_mode = MODE_OFF;
volatile bool stop_all = false; // 리셋 시 모드를 중지하기 위한 플래그
volatile int manual_led_state[4] = {0, 0, 0, 0}; // 수동 모드 LED 상태

// 인터럽트 핸들러 함수
irqreturn_t irq_handler(int irq, void *dev_id) {
    int i;

    for (i = 0; i < 4; i++) {
        if (irq == irq_num[i]) {
            printk(KERN_INFO "Interrupt received on SW[%d]\n", i);

            // 리셋 모드 처리
            if (i == 3) { // SW[3] 리셋 모드
                stop_all = true;  // 모든 동작 중지 플래그 설정
                current_mode = MODE_OFF;
                for (i = 0; i < 4; i++) {
                    gpio_set_value(led[i], LOW);
                }
                printk(KERN_INFO "All LEDs turned off and mode reset to MODE_OFF\n");
                return IRQ_HANDLED;
            }

            // 리셋 요청 플래그 해제 (리셋 후 다른 모드로 전환 가능)
            stop_all = false;

            // 모드 설정
            switch (i) {
                case 0: // SW[0]: 전체 모드
                    current_mode = MODE_ALL;
                    printk(KERN_INFO "MODE_ALL activated\n");
                    while (!stop_all && current_mode == MODE_ALL) {
                        for (i = 0; i < 4; i++) {
                            gpio_set_value(led[i], HIGH);
                        }
                        msleep(2000);
                        for (i = 0; i < 4; i++) {
                            gpio_set_value(led[i], LOW);
                        }
                        msleep(2000);
                    }
                    break;

                case 1: // SW[1]: 개별 모드
                    current_mode = MODE_INDIVIDUAL;
                    printk(KERN_INFO "MODE_INDIVIDUAL activated\n");
                    i = 0;
                    while (!stop_all && current_mode == MODE_INDIVIDUAL) {
                        gpio_set_value(led[i], HIGH);
                        msleep(2000);
                        gpio_set_value(led[i], LOW);
                        msleep(500);
                        i = (i + 1) % 4;  // 다음 LED로 이동
                    }
                    break;

                case 2: // SW[2]: 수동 모드
                    current_mode = MODE_MANUAL;
                    manual_led_state[i] = !manual_led_state[i]; // 해당 LED 토글
                    gpio_set_value(led[i], manual_led_state[i]);
                    printk(KERN_INFO "MODE_MANUAL: LED[%d] toggled to %d\n", i, manual_led_state[i]);
                    break;
            }

            break;
        }
    }

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

    printk(KERN_INFO "LED module initialized successfully.\n");
    return 0;
}

// 모듈 종료 함수
static void __exit led_module_exit(void) {
    int i;

    printk(KERN_INFO "Exiting LED module\n");

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

