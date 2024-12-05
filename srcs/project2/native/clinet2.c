#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define DEVICE_PATH "/dev/led_control"

void set_mode(int mode) {
    int fd = open(DEVICE_PATH, O_WRONLY);
    char command[8];

    if (fd < 0) {
        perror("Failed to open device");
        return;
    }

    snprintf(command, sizeof(command), "%d\n", mode);
    if (write(fd, command, strlen(command)) < 0) {
        perror("Failed to write mode to device");
    }
    close(fd);
}

void manual_control(int led_num, int state) {
    int fd = open(DEVICE_PATH, O_WRONLY);
    char command[8];

    if (fd < 0) {
        perror("Failed to open device");
        return;
    }

    snprintf(command, sizeof(command), "3:%d:%d\n", led_num, state);
    if (write(fd, command, strlen(command)) < 0) {
        perror("Failed to send manual command");
    }
    close(fd);
}

int main() {
    int choice, led, state;

    printf("LED Control Program\n");
    printf("0: All LEDs blink\n");
    printf("1: Sequential LEDs blink\n");
    printf("2: Manual LED control\n");
    printf("-1: Reset LEDs\n");
    printf("99: Exit program\n");

    while (1) {
        printf("Enter your choice: ");
        if (scanf("%d", &choice) != 1 || (choice < -1 && choice != 99)) {
            printf("Invalid input! Please enter a valid option.\n");
            while (getchar() != '\n'); // Clear input buffer
            continue;
        }

        if (choice == 99) {
            printf("Exiting program...\n");
            break;
        }

        if (choice == 3) {
            printf("Manual Control Mode\n");
            printf("Enter LED number (0-3): ");
            if (scanf("%d", &led) != 1 || led < 0 || led > 3) {
                printf("Invalid LED number! Must be between 0 and 3.\n");
                while (getchar() != '\n'); // Clear input buffer
                continue;
            }

            printf("Enter state (0: OFF, 1: ON): ");
            if (scanf("%d", &state) != 1 || (state != 0 && state != 1)) {
                printf("Invalid state! Must be 0 (OFF) or 1 (ON).\n");
                while (getchar() != '\n'); // Clear input buffer
                continue;
            }

            manual_control(led, state);
        } else {
            set_mode(choice);
        }
    }

    return 0;
}
