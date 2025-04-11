#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

void log_variable(const char *file_name, const char *var_name, uint32_t value) {
    char log_file[256];
    snprintf(log_file, sizeof(log_file), "/tmp/%s", file_name);

    FILE *file = fopen(log_file, "a");
    if (file == NULL) {
        perror("Error opening log file");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "%s: %d\n", var_name, value);
    fclose(file);
}
