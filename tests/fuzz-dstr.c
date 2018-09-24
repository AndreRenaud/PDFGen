//#include <stdio.h>
//#include <string.h>

#include "pdfgen.c"

int LLVMFuzzerTestOneInput(const char *data, int size)
{
    struct dstr str = INIT_DSTR;
    int len;

    if (size == 0)
        return 0;

    char *new_data = malloc(size);
    if (!new_data)
        return -1;
    memcpy(new_data, data, size);

    // Ensure it's null terminated
    new_data[size - 1] = '\0';
    len = strlen(new_data);

    dstr_append(&str, new_data);
    if (strcmp(dstr_data(&str), new_data) != 0) {
        printf("Comparison failed:\n%s\n%s\n", dstr_data(&str), new_data);
        return -1;
    }
    free(new_data);
    dstr_free(&str);
    return 0;
}
