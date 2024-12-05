#include <stdio.h>
#include <stdlib.h>

#define SYSFS_PATH "/sys/kernel/led_control/mode"

void set_mode(int mode) {
    FILE *file = fopen(SYSFS_PATH, "w");
    if (file == NULL) {
        perror("Failed to open sysfs file");
        return;
    }
    fprintf(file, "%d\n", mode);
    fclose(file);
}

int main() {
    int mode;

    printf("Mode 0: All blink\n");
    printf("Mode 1: Sequential blink\n");
    printf("Mode 2: Manual toggle mode\n");
    printf("Mode -1: Reset mode\n");

    while (1) {
        printf("Enter mode: ");
        if (scanf("%d", &mode) != 1 || mode < -1 || mode > 3) {
            printf("Invalid mode! Please enter a value between -1 and 3.\n");
            while (getchar() != '\n');
            continue;
        }
        set_mode(mode);
    }

    return 0;
}
