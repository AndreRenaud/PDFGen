#include <stdio.h>
#include <string.h>

#include "pdfgen.h"

#define filename "./fuzz.jpg"
int LLVMFuzzerTestOneInput(char *data, int size)
{
    FILE *temfile = fopen(filename, "w");
    fwrite(data, 1, size, temfile);
    fclose(temfile);

    struct pdf_doc *pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, NULL);
    pdf_set_font(pdf, "Times-Roman");
    pdf_append_page(pdf);
    pdf_add_jpeg(pdf, NULL, 100, 500, 50, 150, filename);
    pdf_save(pdf, "fuzz.pdf");
    pdf_destroy(pdf);
    return 0;
}
