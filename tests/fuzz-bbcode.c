#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pdfgen.h"

int LLVMFuzzerTestOneInput(char *data, int size)
{
    char *text;
    struct pdf_doc *pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, NULL);
    pdf_set_font(pdf, "Helvetica");
    pdf_append_page(pdf);
    text = malloc(size + 1);
    memcpy(text, data, size);
    text[size] = '\0';
    pdf_add_bbcode(pdf, NULL, text, 10, 20, 30, PDF_RGB(0xff, 0, 0));
    free(text);
    pdf_save_encrypted(pdf, "fuzz.pdf", "secret");
    pdf_destroy(pdf);
    return 0;
}
