#include <stdio.h>
#include <string.h>

#include "pdfgen.h"

#define filename "./fuzz-ttf.ttf"
int LLVMFuzzerTestOneInput(char *data, int size)
{
    FILE *temfile = fopen(filename, "w");
    if (!temfile)
        return 0;
    fwrite(data, 1, size, temfile);
    fclose(temfile);

    struct pdf_doc *pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, NULL);
    pdf_append_page(pdf);
    pdf_set_font_ttf(pdf, filename);
    pdf_save(pdf, "fuzz-ttf.pdf");
    pdf_destroy(pdf);
    return 0;
}
