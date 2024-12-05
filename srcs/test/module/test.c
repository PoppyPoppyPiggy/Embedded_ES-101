#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/delay.h>

#define HIGH 1
#define LOW 0

int led[4] = {23,24,25,1};      //LED GPIO
int sw[4] = {4,17,27,22};       //스위치 GPIO
int mod = 0;                    //현재 모드
static struct timer_list timer; //timer 구조체

// 모드 설명:
// 0: 전체 LED가 2초 간격으로 동시에 켜졌다 꺼짐
// 1: LED가 2초 간격으로 하나씩 순차적으로 켜짐
// 2: 수동 모드: 스위치 눌 때마다 해당 LED를 수동으로 켜거나 끄는 모드
// 3: 모드 리셋: LED 동작 중지 및 모드 초기화

// 스위치 인터럽트 핸들러
irqreturn_t switch_irq_handler(int irq, void *dev_id) {
    int switch_mod = *(int *)dev_id;

    switch (switch_mod) {
        case 0:
            mod = 0;    // 전체 모드  
            printk(KERN_INFO "Mode changed: (Mode 0)\n");
            break;
        case 1:
            mod = 1;    // 개별 모드
            printk(KERN_INFO "Mode changed: (Mode 1)\n");
            break;
        case 2:
            mod = 2;    // 수동 모드
            printk(KERN_INFO "Mode changed: (Mode 2)\n");
            break;
        case 3:
            mod = 0;    // 모드 리셋 (모두 초기화)
            printk(KERN_INFO "Mode changed: (Mode 3)\n");
            break;
    }

    if (mod == 2) {
        int led_index = irq - gpio_to_irq(sw[0]); // 스위치 인덱스를 통해 LED 인덱스를 계산
        if (gpio_get_value(led[led_index]) == LOW) {
            gpio_set_value(led[led_index], HIGH);  // LED 켜기
        } else {
            gpio_set_value(led[led_index], LOW);   // LED 끄기
        }
    }

    return IRQ_HANDLED;

}

// 타이머 콜백 함수
static void timer_callback(struct timer_list *t) {
    static int current_led = 0;

    switch (mod) {
        case 0:  
            for(int i=0;i<4;i++){
                gpio_set_value(led[0], HIGH);
            }
            msleep(2000);
            for(int i=0;i<4;i++){
                gpio_set_value(led[0], LOW);
            }

        case 1:  
            gpio_set_value(led[current_led], HIGH);
            msleep(2000);  
            gpio_set_value(led[current_led], LOW);
            current_led = (current_led + 1) % 4;  
            break;

        case 2:  // 모드 2: 수동 모드 (스위치 눌 때마다 LED 토글)
            break;
    }

    // 타이머 다시 등록
    mod_timer(&timer, jiffies + msecs_to_jiffies(2000));
}


static int pj1_module_init(void) {
    int i;

    printk(KERN_INFO "Initializing module...\n");

    // LED와 스위치 GPIO 초기화
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
    // 타이머 설정
    timer_setup(&timer, timer_callback, 0);
    // 모드 초기화
    mod = 3;  

    return 0;
}




static void pj1_module_exit(void){

    printk(KERN_INFO "Exiting module...\n");

    // 타이머 삭제
    del_timer(&timer);

    // GPIO 핀 해제
    for (int i = 0; i < 4; i++) {
        gpio_set_value(led[i], LOW);  // LED 끄기
        gpio_free(led[i]);
        gpio_free(sw[i]);
    }

    printk(KERN_INFO "Module exited successfully.\n");
}


module_init(pj1_module_init);
module_exit(pj1_module_exit);
MODULE_LICENSE("GPL");


