#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define DEVICE_PATH "/dev/led_control"

void print_menu() {
    printf("\n--- Project 2 device + native ---\n");
    printf("1: 전체 모드\n");
    printf("2: 개별 모드\n");
    printf("3: 수동 모드\n");
    printf("4: 리셋 모드\n");
    printf("-1: 프로그램 종료\n");
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

    print_menu();
    while (1) {
        printf("모드를 선택해주세요: ");
        scanf("%d", &choice);

        if (choice == -1) { // 프로그램 종료
            printf("프로그램을 종료합니다\n");
            break;
        }

        // 0~4 모드 입력값 처리
        if (choice >= 0 && choice <= 4) {
            if (choice == 3) { // Manual LED control
                snprintf(buffer, sizeof(buffer), "%d", choice);
                ret = write(fd, buffer, strlen(buffer));
                if (ret < 0) {
                    perror("Failed");
                }
                while (1) {
                    printf("개별 LED를 골라주세요 (0-3): ");
                    scanf("%d", &choice);

                    if (choice == 4){ // Manual mode 종료
                        snprintf(buffer, sizeof(buffer), "%d", choice);
                        ret = write(fd, buffer, strlen(buffer));
                        if (ret < 0) {
                            perror("Failed");
                        }
                        break;
                    }
                    if (choice >= 0 && choice <= 3) {
                        snprintf(buffer, sizeof(buffer), "%d", choice);
                        ret = write(fd, buffer, strlen(buffer));
                        if (ret < 0) {
                            perror("LED 선택 실패");
                        } else {
                            printf("LED %d 켜졌습니다.\n", choice);
                        }
                    } else {
                        printf("올바른 번호를 입력해주세요.\n");
                    }
                }
            } else { // 다른 모드 선택
                snprintf(buffer, sizeof(buffer), "%d", choice);
                ret = write(fd, buffer, strlen(buffer));
                if (ret < 0) {
                    perror("Failed");
                }
            }
        } else {
            printf("올바른 번호를 입력해주세요.\n");
        }
    }

    close(fd); // 장치 닫기
    return 0;
}
