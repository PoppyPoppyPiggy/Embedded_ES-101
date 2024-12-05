#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/timer.h>

#define HIGH 1
#define LOW  0

int sw[4] = {4, 17, 27, 22}; // 스위치 핀 번호
int led[4] = {23, 24, 25, 1}; // LED 핀 번호

static struct timer_list timer;
static int mode = -1;        // 동작 모드 (-1: 없음, 0: 모든 LED 깜박임, 1: 순차 점등, 2: 토글 모드)
static int led_state[4] = {0, 0, 0, 0}; // 각 LED의 상태 저장 (모드 2에서 사용)
static int flag = 0;         // LED 토글 상태
static int led_index = 0;    // 순차 점등용 인덱스

// 타이머 콜백 함수
static void timer_cb(struct timer_list *timer) {
    int i;

    if (mode == 0) {
        printk(KERN_INFO "Timer callback for all LEDs blinking!\n");
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], flag ? LOW : HIGH);
        }
        flag = !flag;
    } else if (mode == 1) {
        printk(KERN_INFO "Timer callback for sequential LED lighting! Index: %d\n", led_index);
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], (i == led_index) ? HIGH : LOW);
        }
        led_index = (led_index + 1) % 4; // 다음 LED로 이동
    }

    mod_timer(timer, jiffies + HZ * 2);
}

// 스위치 인터럽트 핸들러
irqreturn_t irq_handler(int irq, void *dev_id) {
    int i;

    switch (irq) {
    case 60: // SW[0]: 모든 LED 깜박임 or 토글 모드에서 LED 0 토글
        printk(KERN_INFO "SW1 interrupt occurred!\n");
        if (mode == -1 || mode == 1) {
            mode = 0;
            flag = 0;
            mod_timer(&timer, jiffies + HZ * 2);
        } else if (mode == 2) {
            led_state[0] = !led_state[0];
            gpio_set_value(led[0], led_state[0]);
        }
        break;

    case 61: // SW[1]: 순차 점등 or 토글 모드에서 LED 1 토글
        printk(KERN_INFO "SW2 interrupt occurred!\n");
        if (mode == -1 || mode == 0) {
            mode = 1;
            led_index = 0;
            mod_timer(&timer, jiffies + HZ * 2);
        } else if (mode == 2) {
            led_state[1] = !led_state[1];
            gpio_set_value(led[1], led_state[1]);
        }
        break;

    case 62: // SW[2]: 토글 모드 진입 or 토글 모드에서 LED 2 토글
        printk(KERN_INFO "SW3 interrupt occurred!\n");
        if (mode != 2) {
            mode = 2;
            del_timer(&timer); // 타이머 중지
            for (i = 0; i < 4; i++) {
                led_state[i] = 0; // 초기화
                gpio_set_value(led[i], LOW);
            }
        } else {
            led_state[2] = !led_state[2];
            gpio_set_value(led[2], led_state[2]);
        }
        break;

    case 63: // SW[3]: 모든 LED 끄기 및 타이머 중지
        printk(KERN_INFO "SW4 interrupt occurred!\n");
        mode = -1;
        del_timer(&timer);
        for (i = 0; i < 4; i++) {
            gpio_set_value(led[i], LOW);
        }
        return IRQ_HANDLED;
    }

    return IRQ_HANDLED;
}

// 모듈 초기화 함수
static int __init led_module_init(void) {
    int ret, i;

    printk(KERN_INFO "led_module_init!\n");

    // LED 핀 초기화
    for (i = 0; i < 4; i++) {
        ret = gpio_request(led[i], "LED");
        if (ret < 0) {
            printk(KERN_ERR "LED gpio_request failed for pin %d!\n", led[i]);
            return ret;
        }
        gpio_direction_output(led[i], LOW);
    }

    // 스위치 핀 초기화 및 인터럽트 설정
    for (i = 0; i < 4; i++) {
        ret = gpio_request(sw[i], "SW");
        if (ret < 0) {
            printk(KERN_ERR "SW gpio_request failed for pin %d!\n", sw[i]);
            return ret;
        }
        gpio_direction_input(sw[i]);
        ret = request_irq(gpio_to_irq(sw[i]), irq_handler, IRQF_TRIGGER_RISING, "sw_irq", NULL);
        if (ret < 0) {
            printk(KERN_ERR "Request IRQ failed for pin %d!\n", sw[i]);
            return ret;
        }
    }

    // 타이머 초기화
    timer_setup(&timer, timer_cb, 0);

    return 0;
}

// 모듈 종료 함수
static void __exit led_module_exit(void) {
    int i;

    printk(KERN_INFO "led_module_exit!\n");

    // 타이머 제거
    del_timer(&timer);

    // GPIO 및 IRQ 해제
    for (i = 0; i < 4; i++) {
        free_irq(gpio_to_irq(sw[i]), NULL);
        gpio_free(sw[i]);
        gpio_free(led[i]);
    }
}

module_init(led_module_init);
module_exit(led_module_exit);
MODULE_LICENSE("GPL");
