#include "pdfgen.h"
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    struct pdf_doc *pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, NULL);
    int pagecount = 10;
    char filename[128];

    if (argc > 1) {
        char *end;
        errno = 0;
        long val = strtol(argv[1], &end, 10);
        if (errno != 0 || end == argv[1] || *end != '\0' || val < 1 ||
            val > INT_MAX) {
            fprintf(stderr, "Invalid page count: %s\n", argv[1]);
            return 1;
        }
        pagecount = (int)val;
    }
    pdf_set_font(pdf, "Times-Roman");
    for (int i = 0; i < pagecount; i++) {
        char str[64];
        pdf_append_page(pdf);
        sprintf(str, "page %d", i);
        pdf_add_text(pdf, NULL, str, 12, 50, 20, PDF_BLACK);

        pdf_add_image_file(pdf, NULL, 100, 500, 50, 150, "data/penguin.jpg");
    }

    sprintf(filename, "massive-%d.pdf", pagecount);
    pdf_save(pdf, filename);
    pdf_destroy(pdf);
    return 0;
}