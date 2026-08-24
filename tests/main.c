#include <locale.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "pdfgen.h"

extern unsigned char data_penguin_jpg[];
extern unsigned int data_penguin_jpg_len;

extern unsigned char data_rgb[];

/* Abort the tests if 'cond' is false */
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
                    #cond);                                                  \
            pdf_destroy(doc);                                                \
            return -1;                                                       \
        }                                                                    \
    } while (0)

/* Shorthand for the calls that are exercised repeatedly by the tests */
#define TEXT(s) pdf_add_text(doc, NULL, s, 12, 10, 10, PDF_BLACK)
#define WRAP(s, w, align)                                                    \
    pdf_add_text_wrap(doc, NULL, s, 12, 10, 700, 0, PDF_BLACK, w, align, NULL)
#define WIDTH(font, s) pdf_get_font_text_width(doc, font, s, 12, &width)
#define BARCODE(type, w, h, s)                                               \
    pdf_add_barcode(doc, NULL, PDF_BARCODE_##type, 0, 0, w, h, s, PDF_BLACK)
#define IMG(data, n)                                                         \
    pdf_add_image_data(doc, NULL, 0, 0, 10, 10, (const uint8_t *)(data), n)
#define DATA(s) IMG(s, sizeof(s) - 1)
#define PNG_HDR(ct, depth, deflate)                                          \
    IMG(buf, png_build(buf, ct, depth, deflate, NULL, 0, NULL, 0))
#define PNG(ct, chunk1, size1, chunk2, size2)                                \
    IMG(buf, png_build(buf, ct, 8, 0, chunk1, size1, chunk2, size2))
/* The template BMP, with the bytes at 'off' replaced by 'val' */
#define BMP(off, val)                                                        \
    (memcpy(bmp, bmp_template, sizeof(bmp)),                                 \
     memcpy(&bmp[off], val, sizeof(val) - 1), IMG(bmp, sizeof(bmp)))

/* A valid 2x2 24-bit BMP; biCompression & the pixel data are all zero */
static const uint8_t bmp_template[70] = {
    'B', 'M', 70, 0, 0, 0, /* bfSize */
    0,   0,   0,  0,       /* reserved */
    54,  0,   0,  0,       /* bfOffBits */
    40,  0,   0,  0,       /* biSize */
    2,   0,   0,  0,       /* biWidth */
    2,   0,   0,  0,       /* biHeight */
    1,   0,                /* biPlanes */
    24,  0,                /* biBitCount */
};

/* Append a zero-filled PNG chunk to buf, returning the new length. Only the
 * low byte of 'size' is honoured, so bogus chunk lengths can be declared */
static size_t png_chunk(uint8_t *buf, size_t len, const char *type,
                        uint32_t size)
{
    for (int i = 0; i < 4; i++)
        buf[len + i] = (uint8_t)(size >> (24 - i * 8));
    memcpy(&buf[len + 4], type, 4);
    memset(&buf[len + 8], 0, (size & 0xff) + 4); /* data & CRC */
    return len + 12 + (size & 0xff);
}

/* Build a 2x2 PNG with the given header values and up to two extra chunks */
static size_t png_build(uint8_t *buf, uint8_t colour_type, uint8_t bit_depth,
                        uint8_t deflate, const char *type1, uint32_t size1,
                        const char *type2, uint32_t size2)
{
    size_t len = png_chunk(buf, 8, "IHDR", 13);
    memcpy(buf, "\x89PNG\r\n\x1a\n", 8);
    buf[19] = buf[23] = 2; /* width & height */
    buf[24] = bit_depth;
    buf[25] = colour_type;
    buf[26] = deflate;
    if (type1)
        len = png_chunk(buf, len, type1, size1);
    if (type2)
        len = png_chunk(buf, len, type2, size2);
    return len;
}

/* Exercise error handling and less commonly used code paths in a standalone
 * document, so that the primary output.pdf is unaffected */
static int error_path_tests(void)
{
    struct pdf_doc *doc = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, NULL);
    struct pdf_path_operation ops[] = {
        {.op = 'm', .x1 = 10, .y1 = 10},
        {.op = 'v', .x1 = 50, .y1 = 50, .x2 = 100, .y2 = 10},
        {.op = 'y', .x1 = 150, .y1 = 50, .x2 = 200, .y2 = 10},
        {.op = 'h'}};
    uint8_t buf[256], bmp[sizeof(bmp_template)];
    char big[600];
    float width;

    CHECK(doc != NULL);
    memset(big, 'A', sizeof(big) - 1);
    big[sizeof(big) - 1] = '\0';

    /* Nothing that needs a page should work before one exists */
    CHECK(pdf_page_set_size(doc, NULL, 100, 100) < 0);
    CHECK(!pdf_get_page(doc, 0));
    CHECK(!pdf_get_page(doc, 1));
    CHECK(pdf_add_link(doc, NULL, 0, 0, 10, 10, NULL, 0, 0) < 0);
    CHECK(BARCODE(39, 100, 20, "ABC") < 0);
    CHECK(BARCODE(EAN13, 100, 50, "4003994155486") < 0);
    CHECK(BARCODE(UPCA, 100, 50, "003994155480") < 0);
    CHECK(BARCODE(EAN8, 100, 50, "95012346") < 0);
    CHECK(BARCODE(UPCE, 100, 50, "012345000058") < 0);
    CHECK(BARCODE(QR, 100, 100, "hello") < 0);
    CHECK(pdf_append_page(doc) != NULL);

    /* Links & bookmarks: object 0 exists, but is not a bookmark */
    CHECK(pdf_add_link(doc, NULL, 0, 0, 10, 10, NULL, 0, 0) < 0);
    CHECK(pdf_add_bookmark(doc, NULL, 0, "Not a bookmark") < 0);
    CHECK(pdf_add_bookmark(doc, NULL, 999999, "Bad parent") < 0);

    /* Fonts & text encoding */
    CHECK(pdf_set_font(doc, "NotAFont") < 0);
    CHECK(WIDTH("NotAFont", "x") < 0);
    CHECK(WIDTH(NULL, "\xff") < 0);
    CHECK(WIDTH("Times-Bold", "x") >= 0);
    CHECK(WIDTH("Times-Italic", "x") >= 0);
    CHECK(WIDTH("Symbol", "x") >= 0);
    CHECK(WIDTH("ZapfDingbats", "x") >= 0);
    CHECK(!pdf_set_font_ttf(doc, "data/does-not-exist.ttf"));
    CHECK(!pdf_set_font_ttf(doc, "data/penguin.jpg"));
    CHECK(TEXT("emoji \xf0\x9f\x98\x80") < 0);
    CHECK(pdf_set_font_ttf(doc, "data/Ithaca.ttf") != NULL);
    CHECK(TEXT("bad \xff utf8") < 0);
    CHECK(WIDTH(NULL, "\xff") < 0);
    CHECK(pdf_set_font(doc, "Helvetica") >= 0);

    /* Text wrapping: alignments, oversized lines & impossible wraps */
    CHECK(WRAP("Right aligned text", 200, PDF_ALIGN_RIGHT) >= 0);
    CHECK(WRAP("Justify all lines\nshort", 200, PDF_ALIGN_JUSTIFY_ALL) >= 0);
    CHECK(WRAP(big, 100000, PDF_ALIGN_LEFT) >= 0);
    CHECK(WRAP("Hello", 0.5f, PDF_ALIGN_LEFT) < 0);
    /* Malformed bbcode tags are treated as literal text */
    CHECK(pdf_add_bbcode(doc, NULL, "[color=#12]bad[/color] [b=1]x[/b]", 12,
                         10, 450, PDF_BLACK) >= 0);

    /* Custom paths & polygons */
    CHECK(pdf_add_custom_path(doc, NULL, ops, 4, 1, PDF_BLACK,
                              PDF_TRANSPARENT) >= 0);
    ops[0].op = 'z';
    CHECK(pdf_add_custom_path(doc, NULL, ops, 4, 1, PDF_BLACK,
                              PDF_TRANSPARENT) < 0);
    CHECK(pdf_add_polygon(doc, NULL, NULL, NULL, 0, 1, PDF_BLACK) < 0);

    /* Barcodes */
    CHECK(pdf_add_barcode(doc, NULL, 999, 0, 0, 10, 10, "X", PDF_BLACK) < 0);
    CHECK(BARCODE(128A, 0, 20, "ABC") < 0);
    CHECK(BARCODE(128A, 100, 20, "tab\there") < 0);
    CHECK(BARCODE(39, 100, 20, "lowercase") < 0);
    CHECK(BARCODE(EAN13, 100, 50, "12345") < 0);
    CHECK(BARCODE(EAN13, 100, 50, "X003994155486") < 0);
    CHECK(BARCODE(UPCA, 100, 50, "123") < 0);
    CHECK(BARCODE(EAN8, 100, 50, "123") < 0);
    CHECK(BARCODE(UPCE, 100, 50, "123") < 0);
    CHECK(BARCODE(UPCE, 100, 50, "112345000058") < 0);
    CHECK(BARCODE(UPCE, 100, 50, "01234500005a") < 0);
    CHECK(BARCODE(UPCE, 100, 50, "012345678901") < 0);
    /* The remaining UPC-E compression patterns */
    CHECK(BARCODE(UPCE, 100, 50, "012340000005") >= 0);
    CHECK(BARCODE(UPCE, 100, 50, "012000004567") >= 0);
    CHECK(BARCODE(UPCE, 100, 50, "012300000678") >= 0);
    CHECK(BARCODE(QR, 100, 100, big) < 0);
    CHECK(BARCODE(QR, 0, 0, "hello") < 0);

    /* Raw images: dimension validation & aspect ratio handling */
    CHECK(pdf_add_rgb24(doc, NULL, 0, 0, 10, 10, data_rgb, 0, 8) < 0);
    CHECK(pdf_add_grayscale8(doc, NULL, 0, 0, 10, 10, data_rgb, 16, 0) < 0);
    CHECK(pdf_add_grayscale8(doc, NULL, 0, 0, -1, -1, data_rgb, 4, 4) < 0);
    CHECK(pdf_add_grayscale8(doc, NULL, 0, 0, -1, 20, data_rgb, 4, 4) >= 0);

    /* Image files, unrecognised data & JPEGs */
    CHECK(pdf_add_image_file(doc, NULL, 0, 0, 10, 10, "data/none.png") < 0);
    CHECK(pdf_add_image_file(doc, NULL, 0, 0, 10, 10, "data") < 0);
    CHECK(DATA("not an image") < 0);
    CHECK(DATA("\xff\xd8\xff\xc0\x00\x11") < 0); /* Truncated JPEG */
    CHECK(pdf_add_image_data(doc, NULL, 0, 0, -1, -1, data_penguin_jpg,
                             data_penguin_jpg_len) < 0);

    /* PNG data */
    CHECK(PNG_HDR(PNG_COLOR_RGB, 8, 0) < 0); /* Nothing after IHDR */
    CHECK(IMG(buf, 8) < 0);                  /* Signature only */
    CHECK(IMG(buf, 12) < 0);                 /* Truncated chunk header */
    CHECK(IMG(buf, 20) < 0);                 /* Truncated IHDR */
    memcpy(&buf[12], "tEXt", 4);
    CHECK(IMG(buf, 33) < 0);                  /* First chunk is not IHDR */
    CHECK(PNG_HDR(PNG_COLOR_RGB, 8, 1) < 0);  /* Bad deflate */
    CHECK(PNG_HDR(PNG_COLOR_RGB, 0, 0) < 0);  /* Zero bit depth */
    CHECK(PNG_HDR(PNG_COLOR_RGBA, 8, 0) < 0); /* Unsupported colour */
    /* A chunk longer than the file, then the palette chunk handling */
    CHECK(PNG(PNG_COLOR_RGB, "IDAT", 0x7fffff00, NULL, 0) < 0);
    CHECK(PNG(PNG_COLOR_INDEXED, "PLTE", 4, NULL, 0) < 0);   /* Not % 3 */
    CHECK(PNG(PNG_COLOR_INDEXED, "PLTE", 0, NULL, 0) < 0);   /* Empty */
    CHECK(PNG(PNG_COLOR_INDEXED, "PLTE", 3, "PLTE", 3) < 0); /* Duplicate */
    CHECK(PNG(PNG_COLOR_GREYSCALE, "PLTE", 3, NULL, 0) < 0); /* Unexpected */
    CHECK(PNG(PNG_COLOR_RGB, "IEND", 0, NULL, 0) < 0); /* No image data */
    CHECK(PNG(PNG_COLOR_INDEXED, "IDAT", 4, "IEND", 0) < 0); /* No palette */
    CHECK(PNG(PNG_COLOR_RGB, "IDAT", 4, "IEND", 0) >= 0);
    CHECK(pdf_add_image_data(doc, NULL, 0, 0, -1, -1, buf, 61) < 0);

    /* BMP data */
    CHECK(BMP(0, "BM") >= 0); /* The unmodified template is valid */
    CHECK(IMG(bmp, 20) < 0);  /* Too short */
    CHECK(BMP(18, "\xff\xff\xff\xff") < 0); /* Negative width */
    CHECK(BMP(22, "\0\0\0\x80") < 0);       /* Height overflow */
    CHECK(BMP(2, "\x47") < 0);              /* Wrong file size */
    CHECK(BMP(14, "\x0c") < 0);             /* Wrong header size */
    CHECK(BMP(30, "\x01") < 0);             /* Compressed */
    CHECK(BMP(18, "\0") < 0);               /* Zero width */
    CHECK(BMP(22, "\0") < 0);               /* Zero height */
    CHECK(BMP(28, "\x08") < 0);             /* Unsupported bit depth */
    CHECK(BMP(10, "\x64") < 0);             /* Data offset beyond file */
    CHECK(BMP(10, "\x3c") < 0);             /* Insufficient pixel data */

    /* PPM data */
    CHECK(DATA("P6\n") < 0);                             /* No header */
    CHECK(DATA("P6\nfoo\n") < 0);                        /* No size */
    CHECK(DATA("P6\n0 0\n") < 0);                        /* Zero size */
    CHECK(DATA("P6\n2 2\n") < 0);                        /* No maxval */
    CHECK(DATA("P6\n2 2\n65535\n") < 0);                 /* Bad maxval */
    CHECK(DATA("P6\n2 2\n255\nabc") < 0);                /* Short data */
    CHECK(DATA("P2\n2 1\n255\n# comment\n300 1\n") < 0); /* > maxval */
    CHECK(DATA("P3\n1 1\n255\nx y z\n") < 0);            /* Non-numeric */
    CHECK(DATA("P2\n1 1\n255\n7") >= 0); /* No trailing space */

    /* Saving to somewhere impossible */
    CHECK(pdf_save(doc, "/nonexistent-directory/output.pdf") < 0);
    CHECK(pdf_save_encrypted(doc, "/nonexistent-directory/o.pdf", "x") < 0);

    pdf_destroy(doc);
    return 0;
}

int main(int argc, char *argv[])
{
    struct pdf_info info = {.creator = "My software",
                            .producer = "My software",
                            .title = "My document",
                            .author = "My name",
                            .subject = "My subject"};
    struct pdf_doc *pdf = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, &info);
    struct pdf_object *first_page, *second_page;
    int i;
    float height, width = 0.0f;
    int bm;
    int err;

    setlocale(LC_ALL, "");

    /* Unused */
    (void)argc;
    (void)argv;

    if (!pdf) {
        fprintf(stderr, "Unable to create PDF\n");
        return -1;
    }

    if (pdf_width(pdf) != PDF_A4_WIDTH || pdf_height(pdf) != PDF_A4_HEIGHT) {
        fprintf(stderr, "PDF Size mismatch: %fx%f\n", pdf_width(pdf),
                pdf_height(pdf));
        return -1;
    }

    err = pdf_get_font_text_width(pdf, "Times-BoldItalic", "foo", 14, &width);
    if (err < 0 || width < 18) {
        fprintf(stderr, "Font width invalid: %d/%f\n", err, width);
        return -1;
    }

    /* These calls should fail, since we haven't added a page yet */
    if (pdf_add_image_file(pdf, NULL, 10, 10, 20, 30, "data/teapot.ppm") >= 0)
        return -1;

    if (pdf_add_image_file(pdf, NULL, 100, 500, 50, 150,
                           "data/penguin.jpg") >= 0)
        return -1;

    if (pdf_add_image_file(pdf, NULL, 200, 500, 100, 100, "data/coal.png") >=
        0)
        return -1;

    if (pdf_add_image_file(pdf, NULL, 300, 500, 243, 204, "data/bee.bmp") >=
        0)
        return -1;

    if (pdf_add_text(pdf, NULL, "Page One", 10, 20, 30,
                     PDF_RGB(0xff, 0, 0)) >= 0)
        return -1;

    if (pdf_add_bookmark(pdf, NULL, -1, "Another Page") >= 0)
        return -1;

    if (!pdf_get_err(pdf, &err))
        return -1;

    pdf_clear_err(pdf);
    /* From now on, we shouldn't see any errors */

    pdf_set_font(pdf, "Times-BoldItalic");
    pdf_append_page(pdf);
    first_page = pdf_get_page(pdf, 1);

    pdf_add_text_wrap(
        pdf, NULL,
        "This is a great big long string that I hope will wrap properly "
        "around several lines.\nThere are some odd length "
        "linesthatincludelongwords to check the justification. "
        "I've put some embedded line breaks in to "
        "see how it copes with them. Hopefully it all works properly.\n\n\n"
        "We even include multiple breaks\n"
        "And special stuff €ÜŽžŠšÁ that áüöä should ÄÜÖß— “”‘’ break\n"
        "————————————————————————————————————————————————\n"
        "thisisanenourmouswordthatwillneverfitandwillhavetobecut",
        16, 60, 800, 0, PDF_RGB(0, 0, 0), 300, PDF_ALIGN_JUSTIFY, &height);
    pdf_add_rectangle(pdf, NULL, 58, 800 + 16, 304, -height, 2,
                      PDF_RGB(0, 0, 0));
    pdf_add_image_file(pdf, NULL, 10, 10, 20, 30, "data/teapot.ppm");
    pdf_add_image_file(pdf, NULL, 50, 10, 30, 30, "data/coal.png");
    pdf_add_image_file(pdf, NULL, 100, 10, 30, 30, "data/bee.bmp");
    pdf_add_image_file(pdf, NULL, 150, 10, 30, 30, "data/bee-32-flip.bmp");

    pdf_add_image_file(pdf, NULL, 150, 50, 50, 150, "data/grey.jpg");
    pdf_add_image_file(pdf, NULL, 200, 50, 50, -1, "data/bee.pgm");
    pdf_add_image_file(pdf, NULL, 250, 50, 50, -1, "data/bee-ascii.pgm");
    pdf_add_image_file(pdf, NULL, 300, 50, 50, -1, "data/bee.pbm");
    pdf_add_image_file(pdf, NULL, 350, 50, 50, -1, "data/bee-binary.pbm");
    pdf_add_image_file(pdf, NULL, 250, 10, 20, 30, "data/teapot-ascii.ppm");
    pdf_add_image_file(pdf, NULL, 400, 100, 100, 100, "data/grey.png");
    pdf_add_image_file(pdf, NULL, 400, 210, 100, 100, "data/indexed.png");

    pdf_add_image_data(pdf, NULL, 100, 500, 50, 150, data_penguin_jpg,
                       data_penguin_jpg_len);

    pdf_add_text(pdf, NULL, "Page One", 10, 20, 30, PDF_RGB(0xff, 0, 0));
    pdf_add_text(pdf, NULL, "PjGQji", 18, 20, 130, PDF_RGB(0, 0xff, 0xff));
    pdf_add_line(pdf, NULL, 10, 24, 100, 24, 4, PDF_RGB(0xff, 0, 0));
    float dashed[] = {6, 3};
    pdf_add_line_pattern(pdf, NULL, 10, 34, 100, 34, 2, PDF_RGB(0, 0x80, 0),
                         dashed, 2, 0);
    float dotted[] = {1, 2};
    pdf_add_line_pattern(pdf, NULL, 10, 40, 100, 40, 2, PDF_RGB(0, 0, 0xff),
                         dotted, 2, 0);
    /* Invalid patterns should be rejected */
    float all_zero[] = {0, 0};
    if (pdf_add_line_pattern(pdf, NULL, 10, 46, 100, 46, 2, PDF_BLACK,
                             all_zero, 2, 0) >= 0) {
        fprintf(stderr, "All-zero line pattern not rejected\n");
        return -1;
    }
    float negative[] = {3, -1};
    if (pdf_add_line_pattern(pdf, NULL, 10, 46, 100, 46, 2, PDF_BLACK,
                             negative, 2, 0) >= 0) {
        fprintf(stderr, "Negative line pattern not rejected\n");
        return -1;
    }
    if (pdf_add_line_pattern(pdf, NULL, 10, 46, 100, 46, 2, PDF_BLACK, NULL,
                             2, 0) >= 0) {
        fprintf(stderr, "NULL line pattern not rejected\n");
        return -1;
    }
    pdf_clear_err(pdf);
    pdf_add_cubic_bezier(pdf, NULL, 10, 100, 150, 100, 20, 30, 60, 30, 4,
                         PDF_RGB(0, 0xff, 0));
    pdf_add_quadratic_bezier(pdf, NULL, 10, 140, 150, 140, 50, 160, 4,
                             PDF_RGB(0, 0, 0xff));
    struct pdf_path_operation operations[] = {
        {.op = 'm', .x1 = 100, .y1 = 100},
        {.op = 'l', .x1 = 130, .y1 = 100},
        {.op = 'c',
         .x1 = 150,
         .y1 = 150,
         .x2 = 100,
         .y2 = 100,
         .x3 = 130,
         .y3 = 130},
        {.op = 'l', .x1 = 150, .y1 = 120},
        {.op = 'h'},
    };
    int operation_count = (sizeof(operations) / sizeof((operations)[0]));
    pdf_add_custom_path(pdf, NULL, operations, operation_count, 1,
                        PDF_RGB(0xff, 0, 0), PDF_ARGB(0x80, 0xff, 0, 0));
    pdf_add_circle(pdf, NULL, 100, 240, 50, 5, PDF_RGB(0xff, 0, 0),
                   PDF_TRANSPARENT);
    pdf_add_ellipse(pdf, NULL, 100, 240, 40, 30, 2, PDF_RGB(0xff, 0xff, 0),
                    PDF_RGB(0, 0, 0));
    pdf_add_rectangle(pdf, NULL, 150, 150, 100, 100, 4, PDF_RGB(0, 0, 0xff));
    pdf_add_filled_rectangle(pdf, NULL, 150, 450, 100, 100, 4,
                             PDF_RGB(0, 0xff, 0), PDF_TRANSPARENT);
    pdf_add_text_rotate(pdf, NULL, "This should be transparent", 20, 160, 500,
                        M_PI / 4, PDF_ARGB(0x80, 0, 0, 0));

    float p1X[] = {200, 200, 300, 300};
    float p1Y[] = {200, 300, 200, 300};
    pdf_add_polygon(pdf, NULL, p1X, p1Y, 4, 4, PDF_RGB(0xaa, 0xff, 0xee));
    float p2X[] = {400, 400, 500, 500};
    float p2Y[] = {400, 500, 400, 500};
    pdf_add_filled_polygon(pdf, NULL, p2X, p2Y, 4, 4,
                           PDF_RGB(0xff, 0x77, 0x77));
    pdf_add_text(pdf, NULL, "", 20, 20, 30, PDF_RGB(0, 0, 0));
    pdf_add_text(pdf, NULL, "Date (YYYY-MM-DD):", 20, 220, 30,
                 PDF_RGB(0, 0, 0));

    pdf_add_bookmark(pdf, NULL, -1, "First page");

    pdf_append_page(pdf);
    second_page = pdf_get_page(pdf, 2);

    pdf_add_text(pdf, second_page, "Page Two", 10, 20, 30, PDF_RGB(0, 0, 0));
    pdf_add_text(pdf, NULL, "This is some weird text () \\ # : - Wi-Fi 27°C",
                 10, 50, 60, PDF_RGB(0, 0, 0));
    pdf_add_text(
        pdf, NULL,
        "Control characters ( ) < > [ ] { } / % \n \r \t \b \f ending", 10,
        50, 45, PDF_RGB(0, 0, 0));
    pdf_add_text(pdf, NULL, "Special characters: €ÜŽžŠšÁáüöäÄÜÖß—“”‘’Æ", 10,
                 50, 15, PDF_RGB(0, 0, 0));
    pdf_add_text(pdf, NULL, "This one has a new line in it\nThere it was", 10,
                 50, 80, PDF_RGB(0, 0, 0));
    pdf_add_text(
        pdf, NULL,
        "This is a really long line that will go off the edge of the screen, "
        "because it is so long. I like long text. The quick brown fox jumped "
        "over the lazy dog. The quick brown fox jumped over the lazy dog",
        10, 100, 100, PDF_RGB(0, 0, 0));
    pdf_set_font(pdf, "Helvetica-Bold");
    pdf_add_text(
        pdf, NULL,
        "This is a really long line that will go off the edge of the screen, "
        "because it is so long. I like long text. The quick brown fox jumped "
        "over the lazy dog. The quick brown fox jumped over the lazy dog",
        10, 100, 130, PDF_RGB(0, 0, 0));
    pdf_set_font(pdf, "ZapfDingbats");
    pdf_add_text(
        pdf, NULL,
        "This is a really long line that will go off the edge of the screen, "
        "because it is so long. I like long text. The quick brown fox jumped "
        "over the lazy dog. The quick brown fox jumped over the lazy dog",
        10, 100, 150, PDF_RGB(0, 0, 0));

    pdf_set_font(pdf, "Courier-Bold");
    pdf_add_text(pdf, NULL, "(5.6.5) RS232 shutdown", 8, 317, 546,
                 PDF_RGB(0, 0, 0));
    pdf_add_text(pdf, NULL, "", 8, 437, 546, PDF_RGB(0, 0, 0));
    pdf_add_text(pdf, NULL, "Pass", 8, 567, 556, PDF_RGB(0, 0, 0));
    pdf_add_text(pdf, NULL, "(5.6.3) RS485 pins", 8, 317, 556,
                 PDF_RGB(0, 0, 0));

    /* bbcode formatted text: nested bold/italic/colour and links */
    pdf_set_font(pdf, "Helvetica");
    pdf_add_bbcode(pdf, NULL,
                   "bbcode: [b]bold[/b], [i]italic[/i], [b][i]nested "
                   "[color=#ff0000]red[/color][/i][/b], "
                   "[url=https://github.com/AndreRenaud/PDFGen]a link[/url] "
                   "and [not a tag]",
                   12, 50, 470, PDF_RGB(0, 0, 0));

    bm = pdf_add_bookmark(pdf, NULL, -1, "Another Page");
    bm = pdf_add_bookmark(pdf, NULL, bm, "Another Page again");
    pdf_add_bookmark(pdf, NULL, bm, "A child page");
    pdf_add_bookmark(pdf, NULL, bm, "Another child page");
    pdf_add_bookmark(pdf, NULL, -1, "Top level again");
    pdf_append_page(pdf);

    pdf_set_font(pdf, "Times-Roman");
    for (i = 0; i < 3000; i++) {
        float xpos = (i / 100) * 40.0f;
        float ypos = (i % 100) * 10.0f;
        pdf_add_text(pdf, NULL, "Text blob", 8, xpos, ypos,
                     PDF_RGB(i, (i * 4) & 0xff, (i * 8) & 0xff));
    }
    pdf_add_text(pdf, NULL, "", 10, (i / 100) * 100, (i % 100) * 12,
                 PDF_RGB(0xff, 0, 0));

    struct pdf_object *page = pdf_append_page(pdf);

    if (pdf_page_width(page) != PDF_A4_WIDTH ||
        pdf_page_height(page) != PDF_A4_HEIGHT) {
        fprintf(stderr, "PDF Size mismatch: %fx%f\n", pdf_page_width(page),
                pdf_page_height(page));
        return -1;
    }

    pdf_add_barcode(pdf, NULL, PDF_BARCODE_39, PDF_MM_TO_POINT(20),
                    PDF_MM_TO_POINT(240), PDF_MM_TO_POINT(60),
                    PDF_MM_TO_POINT(20), "CODE39", PDF_RGB(0, 0, 0));

    pdf_add_barcode(pdf, NULL, PDF_BARCODE_128A, PDF_MM_TO_POINT(20),
                    PDF_MM_TO_POINT(210), PDF_MM_TO_POINT(60),
                    PDF_MM_TO_POINT(20), "Code128", PDF_RGB(0, 0, 0));

    pdf_add_text_wrap(pdf, NULL, "EAN13 Barcode", 10, PDF_MM_TO_POINT(20),
                      PDF_MM_TO_POINT(155), 0, PDF_RGB(0, 0, 0),
                      PDF_MM_TO_POINT(60), PDF_ALIGN_CENTER, NULL);
    pdf_add_rectangle(pdf, NULL, PDF_MM_TO_POINT(20), PDF_MM_TO_POINT(160),
                      PDF_MM_TO_POINT(60), PDF_MM_TO_POINT(40), 1,
                      PDF_RGB(0xff, 0, 0xff));
    pdf_add_barcode(pdf, NULL, PDF_BARCODE_EAN13, PDF_MM_TO_POINT(20),
                    PDF_MM_TO_POINT(160), PDF_MM_TO_POINT(60),
                    PDF_MM_TO_POINT(40), "4003994155486", PDF_BLACK);
    pdf_add_text_wrap(pdf, NULL, "UPCA Barcode", 10, PDF_MM_TO_POINT(100),
                      PDF_MM_TO_POINT(155), 0, PDF_RGB(0, 0, 0),
                      PDF_MM_TO_POINT(60), PDF_ALIGN_CENTER, NULL);
    pdf_add_rectangle(pdf, NULL, PDF_MM_TO_POINT(100), PDF_MM_TO_POINT(160),
                      PDF_MM_TO_POINT(60), PDF_MM_TO_POINT(80), 1,
                      PDF_RGB(0, 0, 0xff));
    pdf_add_barcode(pdf, NULL, PDF_BARCODE_UPCA, PDF_MM_TO_POINT(100),
                    PDF_MM_TO_POINT(160), PDF_MM_TO_POINT(60),
                    PDF_MM_TO_POINT(80), "003994155480", PDF_BLACK);

    pdf_add_text_wrap(pdf, NULL, "EAN8 Barcode", 10, PDF_MM_TO_POINT(20),
                      PDF_MM_TO_POINT(55), 0, PDF_RGB(0, 0, 0),
                      PDF_MM_TO_POINT(60), PDF_ALIGN_CENTER, NULL);
    pdf_add_rectangle(pdf, NULL, PDF_MM_TO_POINT(20), PDF_MM_TO_POINT(60),
                      PDF_MM_TO_POINT(60), PDF_MM_TO_POINT(40), 1,
                      PDF_RGB(0, 0xff, 0xff));
    pdf_add_barcode(pdf, NULL, PDF_BARCODE_EAN8, PDF_MM_TO_POINT(20),
                    PDF_MM_TO_POINT(60), PDF_MM_TO_POINT(60),
                    PDF_MM_TO_POINT(40), "95012346", PDF_BLACK);
    pdf_add_text_wrap(pdf, NULL, "UPCE Barcode", 10, PDF_MM_TO_POINT(100),
                      PDF_MM_TO_POINT(55), 0, PDF_RGB(0, 0, 0),
                      PDF_MM_TO_POINT(60), PDF_ALIGN_CENTER, NULL);
    pdf_add_rectangle(pdf, NULL, PDF_MM_TO_POINT(100), PDF_MM_TO_POINT(60),
                      PDF_MM_TO_POINT(60), PDF_MM_TO_POINT(80), 1,
                      PDF_RGB(0, 0xff, 0));
    pdf_add_barcode(pdf, NULL, PDF_BARCODE_UPCE, PDF_MM_TO_POINT(100),
                    PDF_MM_TO_POINT(60), PDF_MM_TO_POINT(60),
                    PDF_MM_TO_POINT(80), "012345000058", PDF_BLACK);

    pdf_add_text_wrap(pdf, NULL, "QR Code", 10, PDF_MM_TO_POINT(20),
                      PDF_MM_TO_POINT(145), 0, PDF_RGB(0, 0, 0),
                      PDF_MM_TO_POINT(60), PDF_ALIGN_CENTER, NULL);
    pdf_add_barcode(pdf, NULL, PDF_BARCODE_QR, PDF_MM_TO_POINT(30),
                    PDF_MM_TO_POINT(103), PDF_MM_TO_POINT(40),
                    PDF_MM_TO_POINT(40),
                    "https://github.com/AndreRenaud/PDFGen", PDF_BLACK);

    pdf_append_page(pdf);
    pdf_page_set_size(pdf, NULL, PDF_A3_HEIGHT, PDF_A3_WIDTH);
    pdf_add_bookmark(pdf, NULL, -1, "Last Page");

    pdf_add_text(pdf, NULL, "This is an A3 landscape page", 10, 20, 30,
                 PDF_RGB(0xff, 0, 0));
    if (pdf_get_font_text_width(pdf, NULL, "This is an A3 landscape page", 10,
                                &width) == 0) {
        pdf_add_link(pdf, NULL, 20, 30, width, 10, first_page, 0,
                     pdf_page_height(first_page) / 2);
    }
    pdf_add_rgb24(pdf, NULL, 72, 72, 288, 144, data_rgb, 16, 8);

    const char *ttf_path = "data/Ithaca.ttf";
    pdf_append_page(pdf);
    pdf_add_bookmark(pdf, NULL, -1, "TrueType Font (Unicode)");
    if (pdf_set_font_ttf(pdf, ttf_path) == NULL) {
        fprintf(stderr, "Failed to load TTF font: %s\n",
                pdf_get_err(pdf, &err));
        pdf_destroy(pdf);
        return -1;
    }

    // ASCII — should always work
    pdf_add_text(pdf, NULL, "TrueType (Type0/CIDFont): the quick brown fox.",
                 14, 50, 750, PDF_BLACK);

    // Latin Extended — é (U+00E9), ï (U+00EF), ü (U+00FC)
    // These are valid WinAnsi but now encoded as glyph IDs via Type0
    pdf_add_text(pdf, NULL,
                 "Latin-1: caf\xc3\xa9, na"
                 "\xc3\xaf"
                 "ve, "
                 "\xc3\xbc"
                 "ber",
                 12, 50, 720, PDF_RGB(0, 0, 0x80));

    // Greek — αβγ (U+03B1..U+03B3) — outside WinAnsiEncoding
    pdf_add_text(pdf, NULL,
                 "Greek: \xce\xb1\xce\xb2\xce\xb3 (alpha beta gamma)", 12, 50,
                 692, PDF_RGB(0, 0x80, 0));

    // Cyrillic — мир (U+043C U+0438 U+0440) — outside WinAnsiEncoding
    pdf_add_text(pdf, NULL,
                 "Cyrillic: \xd0\xbc\xd0\xb8\xd1\x80 (mir = peace)", 12, 50,
                 664, PDF_RGB(0x80, 0, 0));

    // Mixed ASCII and non-ASCII
    pdf_add_text(pdf, NULL,
                 "Mixed: caf\xc3\xa9 \xce\xb1\xce\xb2\xce\xb3 "
                 "\xd0\xbc\xd0\xb8\xd1\x80",
                 12, 50, 636, PDF_BLACK);

    // Verify text width works for Unicode text
    if (pdf_get_font_text_width(pdf, NULL,
                                "\xce\xb1\xce\xb2\xce\xb3", // αβγ
                                12, &width) < 0) {
        fprintf(stderr, "TTF Unicode width failed\n");
        pdf_destroy(pdf);
        return -1;
    }
    if (width <= 0) {
        fprintf(stderr, "TTF Unicode width unexpectedly zero\n");
        pdf_destroy(pdf);
        return -1;
    }

    // Test text_wrap with Unicode characters
    pdf_add_text_wrap(pdf, NULL,
                      "Wrapped: \xce\xb1\xce\xb2\xce\xb3 alpha beta gamma "
                      "\xd0\xbc\xd0\xb8\xd1\x80 mir peace",
                      12, 50, 600, 0, PDF_BLACK, 300, PDF_ALIGN_LEFT, NULL);

    // Reload the same font (should reuse the existing object)
    if (pdf_set_font_ttf(pdf, ttf_path) == NULL) {
        fprintf(stderr, "Failed to reload TTF font\n");
        pdf_destroy(pdf);
        return -1;
    }
    pdf_add_text(pdf, NULL, "Reloaded font: same object reused.", 12, 50, 560,
                 PDF_BLACK);

    // Switch back to a standard font
    pdf_set_font(pdf, "Times-Roman");

    pdf_save(pdf, "output.pdf");

    const char *err_str = pdf_get_err(pdf, &err);
    if (err_str) {
        fprintf(stderr, "PDF Error: %d - %s\n", err, err_str);
        pdf_destroy(pdf);
        return -1;
    }

    /* Save an encrypted copy with password "secret" */
    if (pdf_save_encrypted(pdf, "output_encrypted.pdf", "secret") < 0) {
        err_str = pdf_get_err(pdf, &err);
        fprintf(stderr, "PDF Encryption Error: %d - %s\n", err,
                err_str ? err_str : "unknown");
        pdf_destroy(pdf);
        return -1;
    }
    err_str = pdf_get_err(pdf, &err);
    if (err_str) {
        fprintf(stderr, "PDF Encryption Error: %d - %s\n", err, err_str);
        pdf_destroy(pdf);
        return -1;
    }

    pdf_destroy(pdf);

    if (error_path_tests() < 0)
        return -1;

    return 0;
}
