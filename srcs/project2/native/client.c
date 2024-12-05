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
    printf("Mode 1 : 1 (all blink)\n");
    printf("Mode 2 : 2 (solo blink)\n");
    printf("Mode 3 : 3 (manual mode)\n");
    printf("Mode 4 : 4 (reset mode)\n");
    while (1) {
        
        scanf("%d", &mode);
        if (mode == -1) {
            printf("Turning off LEDs and stopping timer\n");
        }
        set_mode(mode);
    }
    return 0;
}

