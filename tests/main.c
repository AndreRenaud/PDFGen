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

/* Fail the test if 'call' does not report an error */
#define EXPECT_FAIL(doc, call)                                               \
    do {                                                                     \
        pdf_clear_err(doc);                                                  \
        if ((call) >= 0) {                                                   \
            fprintf(stderr, "%s:%d: expected failure from %s\n", __FILE__,   \
                    __LINE__, #call);                                        \
            pdf_destroy(doc);                                                \
            return -1;                                                       \
        }                                                                    \
        if (!pdf_get_err(doc, NULL)) {                                       \
            fprintf(stderr, "%s:%d: no error message set by %s\n", __FILE__, \
                    __LINE__, #call);                                        \
            pdf_destroy(doc);                                                \
            return -1;                                                       \
        }                                                                    \
    } while (0)

/* Fail the test if 'call' (which returns a pointer) does not report an
 * error */
#define EXPECT_NULL(doc, call)                                               \
    do {                                                                     \
        pdf_clear_err(doc);                                                  \
        if ((call) != NULL || !pdf_get_err(doc, NULL)) {                     \
            fprintf(stderr, "%s:%d: expected NULL/error from %s\n",          \
                    __FILE__, __LINE__, #call);                              \
            pdf_destroy(doc);                                                \
            return -1;                                                       \
        }                                                                    \
    } while (0)

/* Fail the test if 'call' reports an error */
#define EXPECT_OK(doc, call)                                                 \
    do {                                                                     \
        int _err;                                                            \
        pdf_clear_err(doc);                                                  \
        if ((call) < 0) {                                                    \
            fprintf(stderr, "%s:%d: unexpected failure from %s: %s\n",       \
                    __FILE__, __LINE__, #call, pdf_get_err(doc, &_err));     \
            pdf_destroy(doc);                                                \
            return -1;                                                       \
        }                                                                    \
    } while (0)

static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void put_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void put_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static const uint8_t png_sig[8] = {0x89, 'P',  'N',  'G',
                                   0x0d, 0x0a, 0x1a, 0x0a};

/* Append a PNG chunk (with a dummy CRC) to 'out', returning the new length */
static size_t png_chunk(uint8_t *out, size_t pos, const char *type,
                        const uint8_t *data, uint32_t len)
{
    put_be32(&out[pos], len);
    memcpy(&out[pos + 4], type, 4);
    if (len)
        memcpy(&out[pos + 8], data, len);
    memset(&out[pos + 8 + len], 0, 4);
    return pos + 12 + len;
}

/* Build a minimal PNG: signature + IHDR + optional extra chunks */
static size_t png_build(uint8_t *out, uint8_t colour_type, uint8_t bit_depth,
                        uint8_t deflate)
{
    uint8_t ihdr[13] = {0};
    put_be32(&ihdr[0], 2); /* width */
    put_be32(&ihdr[4], 2); /* height */
    ihdr[8] = bit_depth;
    ihdr[9] = colour_type;
    ihdr[10] = deflate;
    memcpy(out, png_sig, sizeof(png_sig));
    return png_chunk(out, sizeof(png_sig), "IHDR", ihdr, sizeof(ihdr));
}

/* Build a valid 2x2 24-bit BMP (70 bytes) */
#define BMP_LEN 70
static void bmp_build(uint8_t *out)
{
    memset(out, 0, BMP_LEN);
    out[0] = 'B';
    out[1] = 'M';
    put_le32(&out[2], BMP_LEN); /* bfSize */
    put_le32(&out[10], 54);     /* bfOffBits */
    put_le32(&out[14], 40);     /* biSize */
    put_le32(&out[18], 2);      /* biWidth */
    put_le32(&out[22], 2);      /* biHeight */
    put_le16(&out[26], 1);      /* biPlanes */
    put_le16(&out[28], 24);     /* biBitCount */
    put_le32(&out[30], 0);      /* biCompression */
}

/* Exercise error handling and less commonly used code paths in a
 * standalone document, so that the primary output.pdf is unaffected */
static int error_path_tests(void)
{
    struct pdf_info info = {.creator = "Error tests",
                            .producer = "Error tests",
                            .title = "Error tests",
                            .author = "Nobody",
                            .subject = "Errors"};
    struct pdf_doc *doc = pdf_create(PDF_A4_WIDTH, PDF_A4_HEIGHT, &info);
    struct pdf_object *page;
    uint8_t buf[256];
    uint8_t bmp[BMP_LEN];
    char err_msg[128];
    struct pdf_img_info img_info;
    size_t len;
    float width, height;
    int link;

    if (!doc) {
        fprintf(stderr, "Unable to create PDF\n");
        return -1;
    }

    /* Nothing that needs a page should work before one exists */
    EXPECT_FAIL(doc, pdf_page_set_size(doc, NULL, 100, 100));
    EXPECT_NULL(doc, pdf_get_page(doc, 0));
    EXPECT_NULL(doc, pdf_get_page(doc, 1));
    EXPECT_FAIL(doc, pdf_add_link(doc, NULL, 0, 0, 10, 10, NULL, 0, 0));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_39, 0, 0, 100, 20,
                                     "ABC", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_EAN13, 0, 0, 100,
                                     50, "4003994155486", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_UPCA, 0, 0, 100,
                                     50, "003994155480", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_EAN8, 0, 0, 100,
                                     50, "95012346", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_UPCE, 0, 0, 100,
                                     50, "012345000058", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_QR, 0, 0, 100,
                                     100, "hello", PDF_BLACK));

    page = pdf_append_page(doc);
    if (!page) {
        fprintf(stderr, "Unable to append page\n");
        pdf_destroy(doc);
        return -1;
    }
    EXPECT_NULL(doc, pdf_get_page(doc, 2));

    /* Links & bookmarks */
    EXPECT_FAIL(doc, pdf_add_link(doc, NULL, 0, 0, 10, 10, NULL, 0, 0));
    link = pdf_add_link(doc, NULL, 0, 0, 10, 10, page, 0, 0);
    EXPECT_OK(doc, link);
    EXPECT_FAIL(doc, pdf_add_bookmark(doc, NULL, link, "Not a bookmark"));
    EXPECT_FAIL(doc, pdf_add_bookmark(doc, NULL, 999999, "Bad parent"));

    /* Fonts */
    EXPECT_FAIL(doc, pdf_set_font(doc, "NotAFont"));
    EXPECT_FAIL(doc,
                pdf_get_font_text_width(doc, "NotAFont", "x", 12, &width));
    EXPECT_FAIL(doc, pdf_get_font_text_width(doc, NULL, "\xff", 12, &width));
    EXPECT_OK(doc,
              pdf_get_font_text_width(doc, "Times-Bold", "x", 12, &width));
    EXPECT_OK(doc,
              pdf_get_font_text_width(doc, "Times-Italic", "x", 12, &width));
    EXPECT_OK(doc, pdf_get_font_text_width(doc, "Symbol", "x", 12, &width));
    EXPECT_OK(doc,
              pdf_get_font_text_width(doc, "ZapfDingbats", "x", 12, &width));
    EXPECT_NULL(doc, pdf_set_font_ttf(doc, "data/does-not-exist.ttf"));
    EXPECT_NULL(doc, pdf_set_font_ttf(doc, "data/penguin.jpg"));

    /* Text: invalid & unsupported UTF-8 with a standard font */
    EXPECT_OK(doc, pdf_set_font(doc, "Helvetica"));
    EXPECT_FAIL(
        doc, pdf_add_text(doc, NULL, "bad \xff utf8", 12, 10, 10, PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_text(doc, NULL, "emoji \xf0\x9f\x98\x80", 12, 10,
                                  10, PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_text(doc, NULL, "cjk \xe4\xb8\xad", 12, 10, 10,
                                  PDF_BLACK));

    /* Text wrapping: alignments, oversized lines & impossible wraps */
    EXPECT_OK(doc,
              pdf_add_text_wrap(doc, NULL, "Right aligned text", 12, 10, 700,
                                0, PDF_BLACK, 200, PDF_ALIGN_RIGHT, &height));
    EXPECT_OK(doc, pdf_add_text_wrap(doc, NULL, "Justify all lines\nshort",
                                     12, 10, 650, 0, PDF_BLACK, 200,
                                     PDF_ALIGN_JUSTIFY_ALL, &height));
    {
        char long_line[600];
        memset(long_line, 'a', sizeof(long_line) - 1);
        long_line[sizeof(long_line) - 1] = '\0';
        EXPECT_OK(doc, pdf_add_text_wrap(doc, NULL, long_line, 4, 10, 600, 0,
                                         PDF_BLACK, 100000, PDF_ALIGN_LEFT,
                                         &height));
    }
    EXPECT_FAIL(doc,
                pdf_add_text_wrap(doc, NULL, "Hello", 12, 10, 500, 0,
                                  PDF_BLACK, 0.5f, PDF_ALIGN_LEFT, &height));

    /* bbcode with malformed tags should fall back to literal text */
    EXPECT_OK(doc,
              pdf_add_bbcode(doc, NULL, "[color=#12]bad[/color] [b=1]x[/b]",
                             12, 10, 450, PDF_BLACK));

    /* Text with a TrueType font */
    if (!pdf_set_font_ttf(doc, "data/Ithaca.ttf")) {
        fprintf(stderr, "Failed to load TTF font: %s\n",
                pdf_get_err(doc, NULL));
        pdf_destroy(doc);
        return -1;
    }
    EXPECT_FAIL(
        doc, pdf_add_text(doc, NULL, "bad \xff utf8", 12, 10, 10, PDF_BLACK));
    EXPECT_FAIL(doc, pdf_get_font_text_width(doc, NULL, "\xff", 12, &width));
    EXPECT_OK(doc, pdf_set_font(doc, "Helvetica"));

    /* Custom paths & polygons */
    {
        struct pdf_path_operation ops[] = {
            {.op = 'm', .x1 = 10, .y1 = 10},
            {.op = 'v', .x1 = 50, .y1 = 50, .x2 = 100, .y2 = 10},
            {.op = 'y', .x1 = 150, .y1 = 50, .x2 = 200, .y2 = 10},
            {.op = 'h'},
        };
        struct pdf_path_operation bad_ops[] = {
            {.op = 'm', .x1 = 10, .y1 = 10},
            {.op = 'z', .x1 = 50, .y1 = 50},
        };
        EXPECT_OK(doc, pdf_add_custom_path(doc, NULL, ops, 4, 1, PDF_BLACK,
                                           PDF_TRANSPARENT));
        EXPECT_FAIL(doc, pdf_add_custom_path(doc, NULL, bad_ops, 2, 1,
                                             PDF_BLACK, PDF_TRANSPARENT));
    }
    EXPECT_FAIL(doc, pdf_add_polygon(doc, NULL, NULL, NULL, 0, 1, PDF_BLACK));

    /* Barcodes */
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, 999, 0, 0, 100, 20, "ABC",
                                     PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_128A, 0, 0, 0, 20,
                                     "ABC", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_128A, 0, 0, 100,
                                     20, "tab\there", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_39, 0, 0, 100, 20,
                                     "lowercase", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_EAN13, 0, 0, 100,
                                     50, "12345", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_EAN13, 0, 0, 100,
                                     50, "X003994155486", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_UPCA, 0, 0, 100,
                                     50, "123", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_EAN8, 0, 0, 100,
                                     50, "123", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_UPCE, 0, 0, 100,
                                     50, "123", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_UPCE, 0, 0, 100,
                                     50, "112345000058", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_UPCE, 0, 0, 100,
                                     50, "01234500005a", PDF_BLACK));
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_UPCE, 0, 0, 100,
                                     50, "012345678901", PDF_BLACK));
    /* The remaining UPC-E compression patterns */
    EXPECT_OK(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_UPCE, 0, 0, 100, 50,
                                   "012340000005", PDF_BLACK));
    EXPECT_OK(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_UPCE, 0, 0, 100, 50,
                                   "012000004567", PDF_BLACK));
    EXPECT_OK(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_UPCE, 0, 0, 100, 50,
                                   "012300000678", PDF_BLACK));
    {
        char long_qr[200];
        memset(long_qr, 'A', sizeof(long_qr) - 1);
        long_qr[sizeof(long_qr) - 1] = '\0';
        EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_QR, 0, 0, 100,
                                         100, long_qr, PDF_BLACK));
    }
    EXPECT_FAIL(doc, pdf_add_barcode(doc, NULL, PDF_BARCODE_QR, 0, 0, 0, 0,
                                     "hello", PDF_BLACK));

    /* Raw images: dimension validation & aspect ratio handling */
    EXPECT_FAIL(doc, pdf_add_rgb24(doc, NULL, 0, 0, 10, 10, data_rgb, 0, 8));
    EXPECT_FAIL(doc,
                pdf_add_grayscale8(doc, NULL, 0, 0, 10, 10, data_rgb, 16, 0));
    EXPECT_FAIL(doc,
                pdf_add_grayscale8(doc, NULL, 0, 0, -1, -1, data_rgb, 4, 4));
    EXPECT_OK(doc,
              pdf_add_grayscale8(doc, NULL, 0, 0, -1, 20, data_rgb, 4, 4));

    /* Image files */
    EXPECT_FAIL(doc, pdf_add_image_file(doc, NULL, 0, 0, 10, 10,
                                        "data/does-not-exist.png"));
    EXPECT_FAIL(doc, pdf_add_image_file(doc, NULL, 0, 0, 10, 10, "data"));
    EXPECT_OK(doc,
              pdf_add_image_file(doc, NULL, 0, 0, -1, 40, "data/bee.pgm"));

    /* Unrecognised image data */
    EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10,
                                        (const uint8_t *)"not an image", 12));
    if (pdf_parse_image_header(&img_info, (const uint8_t *)"nope", 4, err_msg,
                               sizeof(err_msg)) >= 0) {
        fprintf(stderr, "Parsed invalid image header\n");
        pdf_destroy(doc);
        return -1;
    }

    /* JPEG data */
    {
        const uint8_t bad_jpeg[] = {0xff, 0xd8, 0xff, 0xc0, 0x00, 0x11};
        EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, bad_jpeg,
                                            sizeof(bad_jpeg)));
    }
    EXPECT_FAIL(doc,
                pdf_add_image_data(doc, NULL, 0, 0, -1, -1, data_penguin_jpg,
                                   data_penguin_jpg_len));

    /* PNG data */
#define PNG_TEST(expect, ...)                                                \
    do {                                                                     \
        len = png_build(buf, __VA_ARGS__);                                   \
        expect(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, buf, len));  \
    } while (0)
    EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, png_sig,
                                        sizeof(png_sig)));
    memcpy(buf, png_sig, sizeof(png_sig));
    memset(&buf[sizeof(png_sig)], 0, 8);
    EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, buf, 12));
    memcpy(&buf[12], "IHDR", 4);
    EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, buf, 20));
    memcpy(&buf[12], "tEXt", 4);
    EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, buf, 40));
    PNG_TEST(EXPECT_FAIL, PNG_COLOR_RGB, 8, 1);  /* Bad deflate */
    PNG_TEST(EXPECT_FAIL, PNG_COLOR_RGB, 0, 0);  /* Zero bit depth */
    PNG_TEST(EXPECT_FAIL, PNG_COLOR_RGBA, 8, 0); /* Unsupported colour */
    PNG_TEST(EXPECT_FAIL, PNG_COLOR_RGB, 8, 0);  /* Truncated after IHDR */
    len = png_build(buf, PNG_COLOR_RGB, 8, 0);
    len = png_chunk(buf, len, "IDAT", NULL, 0);
    put_be32(&buf[len - 12], 0x7fffffff); /* Chunk longer than the file */
    EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, buf, len));
    len = png_build(buf, PNG_COLOR_INDEXED, 8, 0);
    len = png_chunk(buf, len, "PLTE", buf, 4); /* Not a multiple of 3 */
    EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, buf, len));
    len = png_build(buf, PNG_COLOR_INDEXED, 8, 0);
    len = png_chunk(buf, len, "PLTE", NULL, 0); /* Empty palette */
    EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, buf, len));
    len = png_build(buf, PNG_COLOR_INDEXED, 8, 0);
    len = png_chunk(buf, len, "PLTE", buf, 3);
    len = png_chunk(buf, len, "PLTE", buf, 3); /* Duplicate palette */
    EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, buf, len));
    len = png_build(buf, PNG_COLOR_GREYSCALE, 8, 0);
    len = png_chunk(buf, len, "PLTE", buf, 3); /* Unexpected palette */
    EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, buf, len));
    len = png_build(buf, PNG_COLOR_RGB, 8, 0);
    len = png_chunk(buf, len, "IEND", NULL, 0); /* No image data */
    EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, buf, len));
    len = png_build(buf, PNG_COLOR_INDEXED, 8, 0);
    len = png_chunk(buf, len, "IDAT", buf, 4);
    len = png_chunk(buf, len, "IEND", NULL, 0); /* Indexed without palette */
    EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, buf, len));
    len = png_build(buf, PNG_COLOR_RGB, 8, 0);
    len = png_chunk(buf, len, "IDAT", buf, 4);
    len = png_chunk(buf, len, "IEND", NULL, 0);
    EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, -1, -1, buf, len));
    EXPECT_OK(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, buf, len));
#undef PNG_TEST

    /* BMP data */
    bmp_build(bmp);
    EXPECT_OK(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, bmp, BMP_LEN));
    EXPECT_FAIL(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, bmp, 20));
#define BMP_TEST(offset, value, set)                                         \
    do {                                                                     \
        bmp_build(bmp);                                                      \
        set(&bmp[offset], value);                                            \
        EXPECT_FAIL(                                                         \
            doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10, bmp, BMP_LEN)); \
    } while (0)
    BMP_TEST(18, 0xffffffff, put_le32); /* Negative width */
    BMP_TEST(22, 0x80000000, put_le32); /* Height overflow */
    BMP_TEST(2, BMP_LEN + 1, put_le32); /* Wrong file size */
    BMP_TEST(14, 12, put_le32);         /* Wrong header size */
    BMP_TEST(30, 1, put_le32);          /* Compressed */
    BMP_TEST(18, 0, put_le32);          /* Zero width */
    BMP_TEST(22, 0, put_le32);          /* Zero height */
    BMP_TEST(28, 8, put_le16);          /* Unsupported bit depth */
    BMP_TEST(10, 100, put_le32);        /* Data offset beyond file */
    BMP_TEST(10, 60, put_le32);         /* Insufficient pixel data */
#undef BMP_TEST

    /* PPM data */
#define PPM_TEST(expect, str)                                                \
    expect(doc, pdf_add_image_data(doc, NULL, 0, 0, 10, 10,                  \
                                   (const uint8_t *)str, sizeof(str) - 1))
    PPM_TEST(EXPECT_FAIL, "P6\n");              /* No header */
    PPM_TEST(EXPECT_FAIL, "P6\nfoo\n");         /* No size */
    PPM_TEST(EXPECT_FAIL, "P6\n0 0\n");         /* Zero size */
    PPM_TEST(EXPECT_FAIL, "P6\n2 2\n");         /* No maxval */
    PPM_TEST(EXPECT_FAIL, "P6\n2 2\n65535\n");  /* Bad maxval */
    PPM_TEST(EXPECT_FAIL, "P6\n2 2\n255\nabc"); /* Short data */
    PPM_TEST(EXPECT_FAIL, "P2\n2 1\n255\n# comment\n300 1\n"); /* > maxval */
    PPM_TEST(EXPECT_FAIL, "P3\n1 1\n255\nx y z\n"); /* Non-numeric */
    PPM_TEST(EXPECT_OK, "P2\n1 1\n255\n7");         /* No trailing ws */
    PPM_TEST(EXPECT_OK, "P1\n2 1\n10");             /* Packed bits */
#undef PPM_TEST

    /* Saving to somewhere impossible */
    EXPECT_FAIL(doc, pdf_save(doc, "/nonexistent-directory/output.pdf"));
    EXPECT_FAIL(doc, pdf_save_encrypted(doc, "/nonexistent-directory/o.pdf",
                                        "secret"));

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
