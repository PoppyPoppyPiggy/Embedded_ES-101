#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/timer.h>

#define HIGH	1
#define LOW	0

int sw[4] = {4, 17, 27, 22};
int led[4] = {23, 24, 25, 1};

struct task_struct *thread_id = NULL;
static struct timer_list timer;
int flag = 0;

// 커널 스레드 함수
static int kthread_function(void *arg) {
    char value = 1, temp = 0;
    int ret, i;
    printk(KERN_INFO "kthread_function started!\n");
    while (!kthread_should_stop()) {
        temp = value;
        for (i = 0; i < 4; i++) {
            if (temp & 0x01)
                ret = gpio_direction_output(led[i], HIGH);
            else
                ret = gpio_direction_output(led[i], LOW);
            temp = temp >> 1;
        }
        value = value << 1;
        if (value == 0x10)
            value = 0x01;
        ssleep(1);
    }
    printk(KERN_INFO "kthread_should_stop called!\n");
    return 0;
}

// 타이머 콜백 함수
static void timer_cb(struct timer_list *timer) {
    int ret, i;
    printk(KERN_INFO "timer callback function!\n");
    if (flag == 0) {
        for (i = 0; i < 4; i++) {
            ret = gpio_direction_output(led[i], HIGH);
        }
        flag = 1;
    } else {
        for (i = 0; i < 4; i++) {
            ret = gpio_direction_output(led[i], LOW);
        }
        flag = 0;
    }
    timer->expires = jiffies + HZ * 2;
    add_timer(timer);
}

// 인터럽트 핸들러
irqreturn_t irq_handler(int irq, void *dev_id) {
    printk(KERN_INFO "Interrupt occurred: %d\n", irq);
    if (irq == gpio_to_irq(sw[0])) { // Only handle interrupt for sw[0]
        if (thread_id == NULL) {
            // 스레드가 실행 중이지 않으면 실행
            thread_id = kthread_run(kthread_function, NULL, "led_thread");
            if (IS_ERR(thread_id)) {
                printk(KERN_ERR "Failed to create kthread\n");
                thread_id = NULL;
            }
        } else {
            // 스레드가 이미 실행 중이면 중지
            kthread_stop(thread_id);
            thread_id = NULL;
        }
    } else if (irq == gpio_to_irq(sw[1])) { // Handle interrupt for sw[1]
        printk(KERN_INFO "Starting timer for sw[1] interrupt\n");
        timer_setup(&timer, timer_cb, 0);
        timer.expires = jiffies + HZ * 2;
        add_timer(&timer);
    }
    return IRQ_HANDLED;
}

// 모듈 초기화 함수
static int switch_interrupt_init(void) {
    int res, i;
    printk(KERN_INFO "switch_interrupt_init!\n");
    // GPIO 요청 및 인터럽트 설정
    for (i = 0; i < 4; i++) {
        res = gpio_request(sw[i], "sw");
        if (res < 0) {
            printk(KERN_INFO "gpio_request failed for sw[%d]!\n", i);
            return res;
        }
        res = request_irq(gpio_to_irq(sw[i]), (irq_handler_t)irq_handler, IRQF_TRIGGER_RISING, "IRQ", NULL);
        if (res < 0) {
            printk(KERN_INFO "request_irq failed for sw[%d]!\n", i);
            return res;
        }
    }
    // LED GPIO 요청
    for (i = 0; i < 4; i++) {
        res = gpio_request(led[i], "LED");
        if (res < 0) {
            printk(KERN_INFO "gpio_request failed for led[%d]!\n", i);
            return res;
        }
    }
    return 0;
}

// 모듈 종료 함수
static void switch_interrupt_exit(void) {
    int i;
    printk(KERN_INFO "switch_interrupt_exit!\n");
    // 스레드 중지
    if (thread_id) {
        kthread_stop(thread_id);
        thread_id = NULL;
    }
    // 타이머 제거
    del_timer(&timer);
    // 인터럽트 해제 및 GPIO 해제
    for (i = 0; i < 4; i++) {
        free_irq(gpio_to_irq(sw[i]), NULL);
        gpio_free(sw[i]);
    }
    // LED GPIO 해제
    for (i = 0; i < 4; i++) {
        gpio_free(led[i]);
    }
}

module_init(switch_interrupt_init);
module_exit(switch_interrupt_exit);
MODULE_LICENSE("GPL");
