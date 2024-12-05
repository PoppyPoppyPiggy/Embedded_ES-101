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

    printf("Mode 1: All blink\n");
    printf("Mode 2: Sequential blink\n");
    printf("Mode 3: Manual toggle mode\n");
    printf("Mode 4: Reset mode\n");
    printf("Mode 5: Quit\n");

    while (1) {
        printf("Type a mode: ");
        if (scanf("%d", &mode) != 1 || mode <= 0 || mode > 5) {
            printf("Invalid mode! Please enter a value between 1 and 5.\n");
            while (getchar() != '\n');
            continue;
        }
        else if(mode == 5)
        {break;}
        mode--;
        set_mode(mode);
    }

    return 0;
}
