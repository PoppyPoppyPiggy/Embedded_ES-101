#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#define HIGH 1
#define LOW  0

int led[4] = {23, 24, 25, 1};  // LED GPIO 번호
int sw[4] = {4, 17, 27, 22};   // 스위치 GPIO 번호
int irq_num[4];  // IRQ 번호 배열

// 인터럽트 핸들러 함수
irqreturn_t irq_handler(int irq, void *dev_id) {
    int i;
    int condition = -1;

    for (i = 0; i < 4; i++) {
        if (irq == irq_num[i]) {
            printk(KERN_INFO "Interrupt received on SW[%d]\n", i + 1);
            condition = i;
            break;
        }
    }

    if (condition != -1) {
        for (i = 0; i < 4; i++) {
            if (i == condition) {
                gpio_set_value(led[i], HIGH);  // 조건에 맞는 LED 켜기
                printk(KERN_INFO "LED[%d] is ON\n", i + 1);
            } else {
                gpio_set_value(led[i], LOW);   // 나머지 LED 끄기
                printk(KERN_INFO "LED[%d] is OFF\n", i + 1);
            }
        }
    }

    return IRQ_HANDLED;
}

// 모듈 초기화 함수
static int __init led_irq_module_init(void) {
    int ret, i;

    printk(KERN_INFO "Initializing LED IRQ module...\n");

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

        // 스위치 GPIO 요청
        ret = gpio_request(sw[i], "SW");
        if (ret) {
            printk(KERN_ERR "Failed to request GPIO for SW[%d]\n", i);
            return ret;
        }

        // 스위치의 IRQ 번호 얻기 및 핸들러 등록
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

    printk(KERN_INFO "LED IRQ module initialized successfully.\n");
    return 0;
}

// 모듈 종료 함수
static void __exit led_irq_module_exit(void) {
    int i;

    printk(KERN_INFO "Exiting LED IRQ module...\n");

    // IRQ 및 GPIO 해제
    for (i = 0; i < 4; i++) {
        free_irq(irq_num[i], NULL);
        gpio_free(sw[i]);
        gpio_free(led[i]);
    }

    printk(KERN_INFO "LED IRQ module exited successfully.\n");
}

module_init(led_irq_module_init);
module_exit(led_irq_module_exit);

MODULE_LICENSE("GPL");
