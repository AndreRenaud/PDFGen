PDFGen
======

Simple C PDF Creation/Generation library.
All contained a single C-file with header and no external library dependencies.

Useful for embedding into other programs that require rudimentary PDF output.

Supports the following PDF features
* Text of various fonts/sizes/colours
* Lines/Rectangles/Filled Rectangles/Polygons/Filled Polygons
* Bookmarks
* Barcodes (Code-128 & Code-39)
* PPM Images
* JPEG Images

Example usage
=============
```c
#include "pdfgen.h"

int main(void) {
    struct pdf_info info = {
        .creator = "My software",
        .producer = "My software",
        .title = "My document",
        .author = "My name",
        .subject = "My subject",
        .date = "Today"
    };
    struct pdf_doc *pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, &info);
    pdf_set_font(pdf, "Times-Roman");
    pdf_append_page(pdf);
    pdf_add_text(pdf, NULL, "This is text", 12, 50, 20, PDF_BLACK);
    pdf_add_line(pdf, NULL, 50, 24, 150, 24, 3, PDF_BLACK);
    pdf_save(pdf, "output.pdf");
    pdf_destroy(pdf);
    return 0;
}
```

License
=======
[![License: Unlicense](https://img.shields.io/badge/license-Unlicense-blue.svg)](http://unlicense.org/)

The source here is public domain.
If you find it useful, please drop me a line at andre@ignavus.net.

Builds
======

[CircleCI Build](https://circleci.com/gh/AndreRenaud/PDFGen) status: ![Build Status](https://circleci.com/gh/AndreRenaud/PDFGen.svg)

[Fuzzit](https://app.fuzzit.dev/admin/sjk2xLZ7DGKxBaTDwfZW/target)  [![fuzzit](https://app.fuzzit.dev/badge?org_id=sjk2xLZ7DGKxBaTDwfZW&target_id=V3lm359etEu0fwaT3vzC&branch=master)](https://app.fuzzit.dev/admin/sjk2xLZ7DGKxBaTDwfZW/target)

Code Coverage: [![Coverage Status](https://coveralls.io/repos/github/AndreRenaud/PDFGen/badge.svg?branch=master)](https://coveralls.io/github/AndreRenaud/PDFGen?branch=master)
