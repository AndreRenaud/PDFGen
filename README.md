PDFGen
======

Simple C PDF Creation/Generation library.
All contained a single C-file with header and no external library dependencies.

Useful for embedding into other programs that require rudimentary PDF output.

Supports the following PDF features
* Text of various sizes/colours
* Lines/Rectangles/Filled Rectangles
* Bookmarks
* Barcodes (Code-128)
* PPM Images

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

License
=======
This project is licensed under the BSD 3-Clause license. See the 'LICENSE' file for full details.
