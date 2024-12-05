#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define DEVICE_PATH "/dev/led_control"

void print_menu() {
    printf("\n--- LED Control Program ---\n");
    printf("1: All LEDs blink\n");
    printf("2: Sequential LEDs blink\n");
    printf("3: Manual LED control\n");
    printf("4: Reset LEDs\n");
    printf("99: Exit program\n");
    printf("Enter your choice: ");
}

int main() {
    int fd, choice, ret;
    char buffer[3];

    // /dev/led_control 장치 열기
    fd = open(DEVICE_PATH, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }

    while (1) {
        print_menu();
        scanf("%d", &choice);

        if (choice == 99) { // 프로그램 종료
            printf("Exiting program.\n");
            break;
        }

        // 0~4 입력값 처리
        if (choice >= 0 && choice <= 4) {
            snprintf(buffer, sizeof(buffer), "%d", choice);
            ret = write(fd, buffer, strlen(buffer));
            if (ret < 0) {
                perror("Failed to write to device");
            } else {
                printf("Command sent: %d\n", choice);
            }
        } else if (choice == 3) { // Manual LED control
            printf("Enter LED number to toggle (0-3): ");
            scanf("%d", &choice);

            if (choice >= 0 && choice <= 3) {
                snprintf(buffer, sizeof(buffer), "%d", choice);
                ret = write(fd, buffer, strlen(buffer));
                if (ret < 0) {
                    perror("Failed to toggle LED");
                } else {
                    printf("LED %d toggled.\n", choice);
                }
            } else {
                printf("Invalid LED number! Please enter 0-3.\n");
            }
        } else {
            printf("Invalid choice! Please enter 0-4 or 99 to exit.\n");
        }
    }

    close(fd); // 장치 닫기
    return 0;
}
