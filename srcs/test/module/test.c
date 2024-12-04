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

static int __init pj1_module_init(void) {
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


    printk(KERN_INFO"Module exited successfully.\n");
}


module_init(pj1_module_init);
module_exit(pj1_module_exit);
MODULE_LICENSE("GPL");


