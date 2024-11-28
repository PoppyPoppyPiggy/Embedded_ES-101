#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/timer.h>

#define HIGH 1
#define LOW  0

int led[4] = {23, 24, 25, 1};
int current_led = 0; // 현재 활성화된 LED
enum Mode { MODE_ALL, MODE_INDIVIDUAL } current_mode = MODE_ALL; // 현재 모드
static struct timer_list timer;

// LED 상태 업데이트 함수
static void update_leds(int state[]) {
    int i;
    for (i = 0; i < 4; i++) {
        gpio_direction_output(led[i], state[i]);
    }
}

// 타이머 콜백 함수
static void timer_cb(struct timer_list *timer) {
    int i;
    printk(KERN_INFO "Timer callback function!\n");

    if (current_mode == MODE_ALL) {
        // 전체 LED 켜기/끄기
        static int state = LOW;
        state = !state; // 상태 반전
        for (i = 0; i < 4; i++) {
            gpio_direction_output(led[i], state);
        }
    } else if (current_mode == MODE_INDIVIDUAL) {
        // 개별 LED 순차적으로 켜기
        int led_state[4] = {LOW, LOW, LOW, LOW};
        led_state[current_led] = HIGH;
        update_leds(led_state);
        current_led = (current_led + 1) % 4; // 다음 LED로 이동
    }

    // 타이머 재설정
    mod_timer(timer, jiffies + HZ * 2);
}

// 모듈 초기화 함수
static int led_module_init(void) {
    int ret, i;

    printk(KERN_INFO "LED module initializing...\n");

    // GPIO 요청
    for (i = 0; i < 4; i++) {
        ret = gpio_request(led[i], "LED");
        if (ret < 0) {
            printk(KERN_ERR "Failed to request GPIO for LED[%d]\n", i);
            return ret;
        }
        gpio_direction_output(led[i], LOW); // 초기 LED OFF
    }

    // 타이머 설정
    timer_setup(&timer, timer_cb, 0);
    timer.expires = jiffies + HZ * 2; // 2초 후 첫 실행
    add_timer(&timer);

    printk(KERN_INFO "LED module initialized successfully.\n");
    return 0;
}

// 모듈 종료 함수
static void led_module_exit(void) {
    int i;

    printk(KERN_INFO "LED module exiting...\n");

    // 타이머 제거
    del_timer(&timer);

    // GPIO 해제
    for (i = 0; i < 4; i++) {
        gpio_free(led[i]);
    }

    printk(KERN_INFO "LED module exited successfully.\n");
}

module_init(led_module_init);
module_exit(led_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("LED Control Module using Timer.");
