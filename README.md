PDFGen
======

Simple C PDF Creation/Generation library

Provides a very simple C api for creating PDF documents.

Useful for embedded into other programs that require rudimentary PDF output.

Example usage
=============
```c
#include "pdfgen.h"
 ...
struct pdf_doc *pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT);
pdf_append_page(pdf);
pdf_add_text(pdf, NULL, "This is text", 12, 50, 20);
pdf_add_line(pdf, NULL, 50, 24, 150, 24);
pdf_save(pdf, "output.pdf");
pdf_destroy(pdf);
```
