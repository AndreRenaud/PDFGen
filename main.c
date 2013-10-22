#include <stdio.h>
#include <string.h>

#include "pdfgen.h"

int main(int argc, char *argv[])
{
    struct pdf_doc *pdf;
    struct pdf_info info;
    int i;

    memset(&info, 0, sizeof(info));
    strcpy(info.author, "Andre Renaud");

    pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, &info);
    pdf_set_font(pdf, "Helvetica");
    pdf_append_page(pdf);

    pdf_add_ppm(pdf, NULL, 10, 10, 20, 30, "teapot.ppm");

    pdf_add_barcode(pdf, NULL, PDF_BARCODE_128A, 50, 300, 200, 100, "ABCDEF", PDF_RGB(0, 0, 0));

    pdf_add_text(pdf, NULL, "Page One", 10, 20, 30, PDF_RGB(0xff, 0, 0));
    pdf_add_text(pdf, NULL, "PjGQji", 18, 20, 130, PDF_RGB(0, 0xff, 0xff));
#if 1
    pdf_add_line(pdf, NULL, 10, 24, 100, 24, 4, PDF_RGB(0xff, 0, 0));
    pdf_add_rectangle(pdf, NULL, 150, 150, 100, 100, 4, PDF_RGB(0, 0, 0xff));
    pdf_add_filled_rectangle(pdf, NULL, 150, 450, 100, 100, 4, PDF_RGB(0, 0xff, 0));
    pdf_add_text(pdf, NULL, "", 20, 20, 30, PDF_RGB(0, 0, 0));
    pdf_add_text(pdf, NULL, "Date (YYYY-MM-DD):", 20, 220, 30, PDF_RGB(0, 0, 0));

    pdf_add_bookmark(pdf, NULL, "First page");

    pdf_append_page(pdf);
    pdf_add_text(pdf, NULL, "Page Two", 10, 20, 30, PDF_RGB(0, 0, 0));
    pdf_add_text(pdf, NULL, "This is some weird text () \\ # : - Wi-Fi", 10, 50, 60, PDF_RGB(0, 0, 0));
    pdf_add_text(pdf, NULL, "This one has a new line in it\nThere it was", 10, 50, 80, PDF_RGB(0, 0, 0));
    pdf_add_text(pdf, NULL, "This is a really long line that will go off the edge of the screen, because it is so long. I like long text. The quick brown fox jumped over the lazy dog. The quick brown fox jumped over the lazy dog", 10, 100, 100, PDF_RGB(0, 0, 0));
    pdf_set_font(pdf, "Helvetica-Bold");
    pdf_add_text(pdf, NULL, "This is a really long line that will go off the edge of the screen, because it is so long. I like long text. The quick brown fox jumped over the lazy dog. The quick brown fox jumped over the lazy dog", 10, 100, 130, PDF_RGB(0, 0, 0));
    pdf_set_font(pdf, "ZapfDingbats");
    pdf_add_text(pdf, NULL, "This is a really long line that will go off the edge of the screen, because it is so long. I like long text. The quick brown fox jumped over the lazy dog. The quick brown fox jumped over the lazy dog", 10, 100, 150, PDF_RGB(0, 0, 0));

    pdf_set_font(pdf, "Courier-Bold");
    pdf_add_text(pdf, NULL, "(5.6.5) RS232 shutdown", 8, 317, 546, PDF_RGB(0, 0, 0));
    pdf_add_text(pdf, NULL, "", 8, 437, 546, PDF_RGB(0, 0, 0));
    pdf_add_text(pdf, NULL, "Pass", 8, 567, 556, PDF_RGB(0, 0, 0));
    pdf_add_text(pdf, NULL, "(5.6.3) RS485 pins", 8, 317, 556, PDF_RGB(0, 0, 0));

    pdf_add_bookmark(pdf, NULL, "Another Page");
    pdf_append_page(pdf);

    pdf_set_font(pdf, "Times-Roman");
    for (i = 0; i < 1000; i++)
        pdf_add_text(pdf, NULL, "Text blob", 10, (i / 100) * 100, (i % 100) * 12, PDF_RGB(i, (i * 4) & 0xff, (i * 8) & 0xff));
    pdf_add_text(pdf, NULL, "", 10, (i / 100) * 100, (i % 100) * 12, PDF_RGB(0xff, 0, 0));
#endif
    pdf_save(pdf, "output.pdf");

    pdf_destroy(pdf);

    return 0;
}
