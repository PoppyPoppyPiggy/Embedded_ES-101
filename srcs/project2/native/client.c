#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SYSFS_PATH "/sys/kernel/led_control/mode"

void set_mode(int mode) {
    FILE *file = fopen(SYSFS_PATH, "w");
    if (file == NULL) {
        perror("Failed to open sysfs file");
        return;
    }
    fprintf(file, "%d", mode);
    fclose(file);
}

int main() {
    int mode;
    while (1) {
        printf("Enter mode (0: Blink, 1: Sequential, 2: Toggle, -1: Off): ");
        scanf("%d", &mode);
        if (mode == -1) {
            printf("Turning off LEDs and stopping timer\n");
        }
        set_mode(mode);
    }
    return 0;
}

