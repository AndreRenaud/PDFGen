/* vim: set tabstop=8 shiftwidth=4 softtabstop=4 smarttab expandtab autoindent: */
/**
 * Simple engine for creating PDF files.
 * It supports text, shapes, images etc...
 * Capable of handling millions of objects without too much performance penalty        *
 * Licensed under a 3-Clause BSD license. See LICENSE file for full details
 */

/**
 * PDF HINTS & TIPS
 * The following sites have various bits & pieces about PDF document
 * generation
 * http://www.mactech.com/articles/mactech/Vol.15/15.09/PDFIntro/index.html
 * http://gnupdf.org/Introduction_to_PDF
 * http://www.planetpdf.com/mainpage.asp?WebPageID=63
 * http://archive.vector.org.uk/art10008970
 * http://www.adobe.com/devnet/acrobat/pdfs/pdf_reference_1-7.pdf
 *
 * To validate the PDF output, there are several online validators:
 * http://www.validatepdfa.com/online.htm
 * http://www.datalogics.com/products/callas/callaspdfA-onlinedemo.asp
 * http://www.pdf-tools.com/pdf/validate-pdfa-online.aspx
 *
 * PDF page markup operators:
 * b    closepath, fill,and stroke path.
 * B    fill and stroke path.
 * b*   closepath, eofill,and stroke path.
 * B*   eofill and stroke path.
 * BI   begin image.
 * BMC  begin marked content.
 * BT   begin text object.
 * BX   begin section allowing undefined operators.
 * c    curveto.
 * cm   concat. Concatenates the matrix to the current transform.
 * cs   setcolorspace for fill.
 * CS   setcolorspace for stroke.
 * d    setdash.
 * Do   execute the named XObject.
 * DP   mark a place in the content stream, with a dictionary.
 * EI   end image.
 * EMC  end marked content.
 * ET   end text object.
 * EX   end section that allows undefined operators.
 * f    fill path.
 * f*   eofill Even/odd fill path.
 * g    setgray (fill).
 * G    setgray (stroke).
 * gs   set parameters in the extended graphics state.
 * h    closepath.
 * i    setflat.
 * ID   begin image data.
 * j    setlinejoin.
 * J    setlinecap.
 * k    setcmykcolor (fill).
 * K    setcmykcolor (stroke).
 * l    lineto.
 * m    moveto.
 * M    setmiterlimit.
 * n    end path without fill or stroke.
 * q    save graphics state.
 * Q    restore graphics state.
 * re   rectangle.
 * rg   setrgbcolor (fill).
 * RG   setrgbcolor (stroke).
 * s    closepath and stroke path.
 * S    stroke path.
 * sc   setcolor (fill).
 * SC   setcolor (stroke).
 * sh   shfill (shaded fill).
 * Tc   set character spacing.
 * Td   move text current point.
 * TD   move text current point and set leading.
 * Tf   set font name and size.
 * Tj   show text.
 * TJ   show text, allowing individual character positioning.
 * TL   set leading.
 * Tm   set text matrix.
 * Tr   set text rendering mode.
 * Ts   set super/subscripting text rise.
 * Tw   set word spacing.
 * Tz   set horizontal scaling.
 * T*   move to start of next line.
 * v    curveto.
 * w    setlinewidth.
 * W    clip.
 * y    curveto.
 */

#define _POSIX_SOURCE /* For localtime_r */
#include <sys/types.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "pdfgen.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define PDF_RGB_R(c) ((((c) >> 16) & 0xff) / 255.0)
#define PDF_RGB_G(c) ((((c) >>  8) & 0xff) / 255.0)
#define PDF_RGB_B(c) ((((c) >>  0) & 0xff) / 255.0)

typedef struct pdf_object pdf_object;

enum {
    OBJ_none, /* skipped */
    OBJ_info,
    OBJ_stream,
    OBJ_font,
    OBJ_page,
    OBJ_bookmark,
    OBJ_outline,
    OBJ_catalog,
    OBJ_pages,
    OBJ_image,

    OBJ_count,
};

struct flexarray {
    void ***bins;
    int item_count;
    int bin_count;
};

struct pdf_object {
    int type; /* See OBJ_xxxx */
    int index; /* PDF output index */
    int offset; /* Byte position within the output file */
    struct pdf_object *prev; /* Previous of this type */
    struct pdf_object *next; /* Next of this type */
    union {
        struct {
            struct pdf_object *page;
            char name[64];
        } bookmark;
        struct {
            char *text;
            int len;
        } stream;
        struct {
	    struct flexarray children;
        } page;
        struct pdf_info info;
        struct {
            char name[64];
            int index;
        } font;
    };
};

struct pdf_doc {
    char errstr[128];
    int errval;
    struct flexarray objects;

    int width;
    int height;

    struct pdf_object *current_font;

    struct pdf_object *last_objects[OBJ_count];
    struct pdf_object *first_objects[OBJ_count];
    int counts[OBJ_count];
};

/**
 * Simple flexible resizing array implementation
 * The bins get larger in powers of two
 * bin 0 = 1024 items
 *     1 = 2048 items
 *     2 = 4096 items
 *     etc...
 */
/* What is the first index that will be in the given bin? */
#define MIN_SHIFT 10
#define MIN_OFFSET ((1 << MIN_SHIFT) - 1)
static int bin_offset[] = {
    (1 << (MIN_SHIFT + 0)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 1)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 2)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 3)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 4)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 5)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 6)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 7)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 8)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 9)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 10)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 11)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 12)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 13)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 14)) - 1 - MIN_OFFSET,
    (1 << (MIN_SHIFT + 15)) - 1 - MIN_OFFSET,
};

static inline int flexarray_get_bin(struct flexarray *flex, int index)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(bin_offset); i++)
	if (index < bin_offset[i])
	    return i - 1;
    return -1;
    //return index / 4096;
}

static inline int flexarray_get_bin_size(struct flexarray *flex, int bin)
{
    if (bin >= ARRAY_SIZE(bin_offset))
	return -1;
    int next = bin_offset[bin + 1];
    return next - bin_offset[bin];
}

static inline int flexarray_get_bin_offset(struct flexarray *flex, int bin, int index)
{
    return index - bin_offset[bin];
}

static void flexarray_clear(struct flexarray *flex)
{
    int i;
    for (i = 0; i < flex->bin_count; i++)
	free(flex->bins[i]);
    free(flex->bins);
    flex->bin_count = 0;
    flex->item_count = 0;
}

static inline int flexarray_size(struct flexarray *flex)
{
    return flex->item_count;
}

static int flexarray_set(struct flexarray *flex, int index, void *data)
{
    int bin = flexarray_get_bin(flex, index);
    if (bin >= flex->bin_count) {
	void *bins = realloc(flex->bins, (flex->bin_count + 1) *
		sizeof(flex->bins));
	if (!bins)
	    return -ENOMEM;
	flex->bin_count++;
	flex->bins = bins;
	flex->bins[flex->bin_count - 1] =
	    calloc(flexarray_get_bin_size(flex, flex->bin_count - 1),
		    sizeof(void *));
	if (!flex->bins[flex->bin_count - 1]) {
	    flex->bin_count--;
	    return -ENOMEM;
	}
    }
    flex->item_count++;
    flex->bins[bin][flexarray_get_bin_offset(flex, bin, index)] = data;
    return flex->item_count - 1;
}

static inline int flexarray_append(struct flexarray *flex, void *data)
{
    return flexarray_set(flex, flexarray_size(flex), data);
}

static inline void *flexarray_get(struct flexarray *flex, int index)
{
    int bin;

    if (index >= flex->item_count)
	return NULL;
    bin = flexarray_get_bin(flex, index);
    if (bin >= flex->bin_count)
	return NULL;
    return flex->bins[bin][flexarray_get_bin_offset(flex, bin, index)];
}

/**
 * PDF Implementation
 */

static int pdf_set_err(struct pdf_doc *doc, int errval,
                       const char *buffer, ...)
{
    va_list ap;
    int len;

    va_start(ap, buffer);
    len = vsnprintf(doc->errstr, sizeof(doc->errstr) - 2, buffer, ap);
    va_end(ap);

    /* Make sure we're properly terminated */
    if (doc->errstr[len] != '\n')
        doc->errstr[len] = '\n';
    doc->errstr[sizeof(doc->errstr) - 1] = '\0';
    doc->errval = errval;

    fprintf(stderr, "PDFERR: %s", doc->errstr);

    return errval;
}

const char *pdf_get_err(struct pdf_doc *pdf, int *errval)
{
    if (!pdf)
        return NULL;
    if (pdf->errstr[0] == '\0')
        return NULL;
    if (errval) *errval = pdf->errval;
    return pdf->errstr;
}

static struct pdf_object *pdf_get_object(struct pdf_doc *pdf, int index)
{
    return flexarray_get(&pdf->objects, index);
}

static int pdf_append_object(struct pdf_doc *pdf, struct pdf_object *obj)
{
    int index = flexarray_append(&pdf->objects, obj);

    if (index < 0)
	return index;
    obj->index = index;

    if (pdf->last_objects[obj->type]) {
        obj->prev = pdf->last_objects[obj->type];
        pdf->last_objects[obj->type]->next = obj;
    }
    pdf->last_objects[obj->type] = obj;

    if (!pdf->first_objects[obj->type])
        pdf->first_objects[obj->type] = obj;

    pdf->counts[obj->type]++;

    return 0;
}

static struct pdf_object *pdf_add_object(struct pdf_doc *pdf, int type)
{
    struct pdf_object *obj;

    obj = calloc(1, sizeof(struct pdf_object));
    if (!obj) {
        pdf_set_err(pdf, -errno, "Unable to allocate object %d: %s",
                    flexarray_size(&pdf->objects) + 1, strerror(errno));
        return NULL;
    }

    obj->type = type;

    if (pdf_append_object(pdf, obj) < 0) {
	free(obj);
	return NULL;
    }

    return obj;
}

struct pdf_doc *pdf_create(int width, int height, struct pdf_info *info)
{
    struct pdf_doc *pdf;
    struct pdf_object *obj;

    pdf = calloc(1, sizeof(struct pdf_doc));
    pdf->width = width;
    pdf->height = height;

    /* We don't want to use ID 0 */
    pdf_add_object(pdf, OBJ_none);

    /* Create the 'info' object */
    obj = pdf_add_object(pdf, OBJ_info);
    if (info)
        obj->info = *info;
    /* FIXME: Should be quoting PDF strings? */
    if (!obj->info.date[0]) {
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        strftime(obj->info.date, sizeof(obj->info.date),
                 "%Y%m%d%H%M%SZ", &tm);
    }
    if (!obj->info.creator[0])
        strcpy(obj->info.creator, "pdfgen");
    if (!obj->info.producer[0])
        strcpy(obj->info.producer, "pdfgen");
    if (!obj->info.title[0])
        strcpy(obj->info.title, "pdfgen");
    if (!obj->info.author[0])
        strcpy(obj->info.author, "pdfgen");
    if (!obj->info.subject[0])
        strcpy(obj->info.subject, "pdfgen");

    pdf_add_object(pdf, OBJ_pages);
    pdf_add_object(pdf, OBJ_catalog);

    pdf_set_font(pdf, "Times-Roman");

    return pdf;
}

int pdf_width(struct pdf_doc *pdf)
{
    return pdf->width;
}

int pdf_height(struct pdf_doc *pdf)
{
    return pdf->height;
}

static void pdf_object_destroy(struct pdf_object *object)
{
    switch (object->type) {
    case OBJ_stream:
    case OBJ_image:
        free(object->stream.text);
        break;

    case OBJ_page:
	flexarray_clear(&object->page.children);
        break;
    }
    free(object);
}

void pdf_destroy(struct pdf_doc *pdf)
{
    if (pdf) {
        int i;
        for (i = 0; i < flexarray_size(&pdf->objects); i++)
            pdf_object_destroy(pdf_get_object(pdf, i));
	flexarray_clear(&pdf->objects);
        free(pdf);
    }
}

static struct pdf_object *pdf_find_first_object(struct pdf_doc *pdf,
        int type)
{
    return pdf->first_objects[type];
}

static struct pdf_object *pdf_find_last_object(struct pdf_doc *pdf,
        int type)
{
    return pdf->last_objects[type];
}

int pdf_set_font(struct pdf_doc *pdf, const char *font)
{
    struct pdf_object *obj;
    int last_index = 0;

    /* See if we've used this font before */
    for (obj = pdf_find_first_object(pdf, OBJ_font); obj; obj = obj->next) {
        if (strcmp(obj->font.name, font) == 0)
            break;
        last_index = obj->font.index;
    }

    /* Create a new font object if we need it */
    if (!obj) {
        obj = pdf_add_object(pdf, OBJ_font);
        if (!obj)
            return pdf->errval;
        strncpy(obj->font.name, font, sizeof(obj->font.name));
        obj->font.name[sizeof(obj->font.name) - 1] = '\0';
        obj->font.index = last_index + 1;
    }

    pdf->current_font = obj;

    return 0;
}

struct pdf_object *pdf_append_page(struct pdf_doc *pdf)
{
    struct pdf_object *page;

    page = pdf_add_object(pdf, OBJ_page);

    if (!page)
        return NULL;

    return page;
}

static int pdf_save_object(struct pdf_doc *pdf, FILE *fp, int index)
{
    struct pdf_object *object = pdf_get_object(pdf, index);

    if (object->type == OBJ_none)
        return -ENOENT;

    object->offset = ftell(fp);

    fprintf(fp, "%d 0 obj\r\n", index);

    switch (object->type) {
    case OBJ_stream:
    case OBJ_image: {
        int len = object->stream.len ? object->stream.len :
                  strlen(object->stream.text);
        fwrite(object->stream.text, len, 1, fp);
        break;
    }
    case OBJ_info: {
        struct pdf_info *info = &object->info;

        fprintf(fp, "<<\r\n"
                "  /Creator (%s)\r\n"
                "  /Producer (%s)\r\n"
                "  /Title (%s)\r\n"
                "  /Author (%s)\r\n"
                "  /Subject (%s)\r\n"
                "  /CreationDate (D:%s)\r\n"
                ">>\r\n",
                info->creator, info->producer, info->title,
                info->author, info->subject, info->date);
        break;
    }

    case OBJ_page: {
        int i;
        struct pdf_object *font;
        struct pdf_object *pages = pdf_find_first_object(pdf, OBJ_pages);
        struct pdf_object *image = pdf_find_first_object(pdf, OBJ_image);

        fprintf(fp, "<<\r\n"
                "/Type /Page\r\n"
                "/Parent %d 0 R\r\n", pages->index);
        fprintf(fp, "/Resources <<\r\n");
        fprintf(fp, "  /Font <<\r\n");
        for (font = pdf_find_first_object(pdf, OBJ_font); font; font = font->next)
            fprintf(fp, "    /F%d %d 0 R\r\n",
                    font->font.index, font->index);
        fprintf(fp, "  >>\r\n");

        if (image) {
            fprintf(fp, "  /XObject <<");
            for (; image; image = image->next)
                fprintf(fp, "/Image%d %d 0 R ", image->index, image->index);
            fprintf(fp, ">>\r\n");
        }

        fprintf(fp, ">>\r\n");
        fprintf(fp, "/Contents [\r\n");
        for (i = 0; i < flexarray_size(&object->page.children); i++) {
	    struct pdf_object *child = flexarray_get(&object->page.children, i);
            fprintf(fp, "%d 0 R\r\n", child->index);
	}
        fprintf(fp, "]\r\n");
        fprintf(fp, ">>\r\n");
        break;
    }

    case OBJ_bookmark: {
        struct pdf_object *other;

        other = pdf_find_first_object(pdf, OBJ_outline);
        fprintf(fp, "<<\r\n"
                "/A << /Type /Action\r\n"
                "      /S /GoTo\r\n"
                "      /D [%d 0 R /XYZ 0 %d null]\r\n"
                "   >>\r\n"
                "/Parent %d 0 R\r\n"
                "/Title (%s)\r\n",
                object->bookmark.page->index,
                pdf->height,
                other->index, /* FIXME: they're all top level */
                object->bookmark.name);
        other = object->prev;
        if (other)
            fprintf(fp, "/Prev %d 0 R\r\n", other->index);
        other = object->next;
        if (other)
            fprintf(fp, "/Next %d 0 R\r\n", other->index);
        fprintf(fp, ">>\r\n");
        break;
    }

    case OBJ_outline: {
        int count = pdf->counts[OBJ_bookmark];

        if (count) {
            struct pdf_object *first, *last;
            first = pdf_find_first_object(pdf, OBJ_bookmark);
            last = pdf_find_last_object(pdf, OBJ_bookmark);

            /* Bookmark outline */
            fprintf(fp, "<<\r\n"
                    "/Count %d\r\n"
                    "/Type /Outlines\r\n"
                    "/First %d 0 R\r\n"
                    "/Last %d 0 R\r\n"
                    ">>\r\n",
                    count, first->index, last->index);
        }
        break;
    }

    case OBJ_font:
        fprintf(fp, "<<\r\n"
                "  /Type /Font\r\n"
                "  /Subtype /Type1\r\n"
                "  /BaseFont /%s\r\n"
                ">>\r\n", object->font.name);
        break;

    case OBJ_pages: {
        struct pdf_object *page;
        int npages = 0;

        fprintf(fp, "<<\r\n"
                "/Type /Pages\r\n"
                "/Kids [ ");
        for (page = pdf_find_first_object(pdf, OBJ_page);
             page;
             page = page->next) {
            npages++;
            fprintf(fp, "%d 0 R ", page->index);
        }
        fprintf(fp, "]\r\n");
        fprintf(fp, "/Count %d\r\n", npages);
        fprintf(fp, "/MediaBox [0 0 %d %d]\r\n"
                ">>\r\n",
                pdf->width, pdf->height);
        break;
    }

    case OBJ_catalog: {
        struct pdf_object *outline = pdf_find_first_object(pdf, OBJ_outline);
        struct pdf_object *pages = pdf_find_first_object(pdf, OBJ_pages);

        fprintf(fp, "<<\r\n"
                "/Type /Catalog\r\n");
        if (outline)
            fprintf(fp, "/Outlines %d 0 R\r\n", outline->index);
        fprintf(fp, "/Pages %d 0 R\r\n"
                ">>\r\n",
                pages->index);
        break;
    }

    default:
        return pdf_set_err(pdf, -EINVAL, "Invalid PDF object type %d",
                           object->type);
    }

    fprintf(fp, "endobj\r\n");

    return 0;
}

int pdf_save(struct pdf_doc *pdf, const char *filename)
{
    FILE *fp;
    int i;
    struct pdf_object *obj;
    int xref_offset;
    int xref_count = 0;

    if ((fp = fopen(filename, "wb")) == NULL)
        return pdf_set_err(pdf, -errno, "Unable to open '%s': %s",
                           filename, strerror(errno));

    fprintf(fp, "%%PDF-1.2\r\n");
    /* Hibit bytes */
    fprintf(fp, "%c%c%c%c%c\r\n", 0x25, 0xc7, 0xec, 0x8f, 0xa2);

    /* Dump all the objects & get their file offsets */
    for (i = 0; i < flexarray_size(&pdf->objects); i++)
        if (pdf_save_object(pdf, fp, i) >= 0)
            xref_count++;

    /* xref */
    xref_offset = ftell(fp);
    fprintf(fp, "xref\r\n");
    fprintf(fp, "0 %d\r\n", xref_count + 1);
    fprintf(fp, "0000000000 65535 f\r\n");
    for (i = 0; i < flexarray_size(&pdf->objects); i++) {
        obj = pdf_get_object(pdf, i);
        if (obj->type != OBJ_none)
            fprintf(fp, "%10.10d 00000 n\r\n",
                    obj->offset);
    }

    fprintf(fp, "trailer\r\n"
            "<<\r\n"
            "/Size %d\r\n", xref_count + 1);
    obj = pdf_find_first_object(pdf, OBJ_catalog);
    fprintf(fp, "/Root %d 0 R\r\n", obj->index);
    obj = pdf_find_first_object(pdf, OBJ_info);
    fprintf(fp, "/Info %d 0 R\r\n", obj->index);
    /* FIXME: Not actually generating a unique ID */
    fprintf(fp, "/ID [<%16.16x> <%16.16x>]\r\n", 0x123, 0x123);
    fprintf(fp, ">>\r\n"
            "startxref\r\n");
    fprintf(fp, "%d\r\n", xref_offset);
    fprintf(fp, "%%%%EOF\r\n");
    fclose(fp);

    return 0;
}

static int pdf_add_stream(struct pdf_doc *pdf, struct pdf_object *page,
                          char *buffer)
{
    struct pdf_object *obj;
    int len;
    char prefix[128];
    char suffix[128];

    if (!page)
        page = pdf_find_last_object(pdf, OBJ_page);

    if (!page)
        return pdf_set_err(pdf, -EINVAL, "Invalid pdf page");

    len = strlen(buffer);
    /* We don't want any trailing whitespace in the stream */
    while (len >= 1 && (buffer[len - 1] == '\r' ||
                        buffer[len - 1] == '\n')) {
        buffer[len - 1] = '\0';
        len--;
    }

    sprintf(prefix, "<< /Length %d >>stream\r\n", len);
    sprintf(suffix, "\r\nendstream\r\n");
    len += strlen(prefix) + strlen(suffix);

    obj = pdf_add_object(pdf, OBJ_stream);
    if (!obj)
        return pdf->errval;
    obj->stream.text = malloc(len + 1);
    if (!obj->stream.text) {
        obj->type = OBJ_none;
        return pdf_set_err(pdf, -ENOMEM, "Insufficient memory for text (%d bytes)",
                           len + 1);
    }
    obj->stream.text[0] = '\0';
    strcat(obj->stream.text, prefix);
    strcat(obj->stream.text, buffer);
    strcat(obj->stream.text, suffix);
    obj->stream.len = 0;

    return flexarray_append(&page->page.children, obj);
}

int pdf_add_bookmark(struct pdf_doc *pdf, struct pdf_object *page,
                     const char *name)
{
    struct pdf_object *obj = pdf_add_object(pdf, OBJ_bookmark);
    if (!obj)
        return pdf_set_err(pdf, -ENOMEM, "Insufficient memory");

    if (!page)
        page = pdf_find_last_object(pdf, OBJ_page);

    if (!page)
        return pdf_set_err(pdf, -EINVAL,
                           "Unable to add bookmark, no pages available");

    strncpy(obj->bookmark.name, name, sizeof(obj->bookmark.name));
    obj->bookmark.name[sizeof(obj->bookmark.name) - 1] = '\0';
    obj->bookmark.page = page;

    if (!pdf_find_first_object(pdf, OBJ_outline))
        pdf_add_object(pdf, OBJ_outline);

    return 0;
}

struct dstr {
    char *data;
    int alloc_len;
    int used_len;
};

static int dstr_ensure(struct dstr *str, int len)
{
    if (str->alloc_len <= len) {
        int new_len = len + 4096;
        char *new_data = realloc(str->data, new_len);
        if (!new_data) {
            return -ENOMEM;
        }
        str->data = new_data;
        str->alloc_len = new_len;
    }
    return 0;
}

static int dstr_printf(struct dstr *str, const char *fmt, ...)
{
    va_list ap, aq;
    int len;

    va_start(ap, fmt);
    va_copy(aq, ap);
    len = vsnprintf(NULL, 0, fmt, ap);
    if (dstr_ensure(str, str->used_len + len + 1) < 0) {
        va_end(ap);
        va_end(aq);
        return -ENOMEM;
    }
    vsprintf(&str->data[str->used_len], fmt, aq);
    str->used_len += len;
    va_end(ap);
    va_end(aq);

    return len;
}

static int dstr_append(struct dstr *str, const char *extend)
{
    int len = strlen(extend);
    if (dstr_ensure(str, str->used_len + len + 1) < 0)
        return -ENOMEM;
    strcpy(&str->data[str->used_len], extend);
    str->used_len += len;
    return len;
}

static void dstr_free(struct dstr *str)
{
    free(str->data);
}

int pdf_add_text(struct pdf_doc *pdf, struct pdf_object *page,
                 const char *text, int size, int xoff, int yoff,
		 uint32_t colour)
{
    int i, ret;
    int len = text ? strlen(text) : 0;
    struct dstr str = {0, 0, 0};

    /* Don't bother adding empty/null strings */
    if (!len)
        return 0;

    dstr_append(&str, "BT ");
    dstr_printf(&str, "%d %d TD ", xoff, yoff);
    dstr_printf(&str, "/F%d %d Tf ",
                pdf->current_font->font.index, size);
    dstr_printf(&str, "%f %f %f rg ",
            PDF_RGB_R(colour), PDF_RGB_G(colour), PDF_RGB_B(colour));
    dstr_append(&str, "(");

    /* Escape magic characters properly */
    for (i = 0; i < len; i++) {
        if (strchr("()\\", text[i])) {
            char buf[3];
            /* Escape some characters */
            buf[0] = '\\';
            buf[1] = text[i];
            buf[2] = '\0';
            dstr_append(&str, buf);
        } else if (strrchr("\n\r\t\b\f", text[i])) {
            /* Skip over these characters */
            ;
        } else {
            char buf[2];
            buf[0] = text[i];
            buf[1] = '\0';
            dstr_append(&str, buf);
        }
    }
    dstr_append(&str, ") Tj ");
    dstr_append(&str, "ET");

    ret = pdf_add_stream(pdf, page, str.data);
    dstr_free(&str);
    return ret;
}

/* How wide is each character, in points, at size 14 */
static const uint16_t helvetica_widths[256] = {
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 355, 556, 556, 889, 667, 191,
    333, 333, 389, 584, 278, 333, 278, 278,
    556, 556, 556, 556, 556, 556, 556, 556,
    556, 556, 278, 278, 584, 584, 584, 556,
    1015, 667, 667, 722, 722, 667, 611, 778,
    722, 278, 500, 667, 556, 833, 722, 778,
    667, 778, 722, 667, 611, 722, 667, 944,
    667, 667, 611, 278, 278, 278, 469, 556,
    333, 556, 556, 500, 556, 556, 278, 556,
    556, 222, 222, 500, 222, 833, 556, 556,
    556, 556, 333, 500, 278, 556, 500, 722,
    500, 500, 500, 334, 260, 334, 584, 350,
    556, 350, 222, 556, 333, 1000, 556, 556,
    333, 1000, 667, 333, 1000, 350, 611, 350,
    350, 222, 222, 333, 333, 350, 556, 1000,
    333, 1000, 500, 333, 944, 350, 500, 667,
    278, 333, 556, 556, 556, 556, 260, 556,
    333, 737, 370, 556, 584, 333, 737, 333,
    400, 584, 333, 333, 333, 556, 537, 278,
    333, 333, 365, 556, 834, 834, 834, 611,
    667, 667, 667, 667, 667, 667, 1000, 722,
    667, 667, 667, 667, 278, 278, 278, 278,
    722, 722, 778, 778, 778, 778, 778, 584,
    778, 722, 722, 722, 722, 667, 667, 611,
    556, 556, 556, 556, 556, 556, 889, 500,
    556, 556, 556, 556, 278, 278, 278, 278,
    556, 556, 556, 556, 556, 556, 556, 584,
    611, 556, 556, 556, 556, 500, 556, 500
};

static const uint16_t helvetica_bold_widths[256] = {
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 333, 474, 556, 556, 889, 722, 238,
    333, 333, 389, 584, 278, 333, 278, 278,
    556, 556, 556, 556, 556, 556, 556, 556,
    556, 556, 333, 333, 584, 584, 584, 611,
    975, 722, 722, 722, 722, 667, 611, 778,
    722, 278, 556, 722, 611, 833, 722, 778,
    667, 778, 722, 667, 611, 722, 667, 944,
    667, 667, 611, 333, 278, 333, 584, 556,
    333, 556, 611, 556, 611, 556, 333, 611,
    611, 278, 278, 556, 278, 889, 611, 611,
    611, 611, 389, 556, 333, 611, 556, 778,
    556, 556, 500, 389, 280, 389, 584, 350,
    556, 350, 278, 556, 500, 1000, 556, 556,
    333, 1000, 667, 333, 1000, 350, 611, 350,
    350, 278, 278, 500, 500, 350, 556, 1000,
    333, 1000, 556, 333, 944, 350, 500, 667,
    278, 333, 556, 556, 556, 556, 280, 556,
    333, 737, 370, 556, 584, 333, 737, 333,
    400, 584, 333, 333, 333, 611, 556, 278,
    333, 333, 365, 556, 834, 834, 834, 611,
    722, 722, 722, 722, 722, 722, 1000, 722,
    667, 667, 667, 667, 278, 278, 278, 278,
    722, 722, 778, 778, 778, 778, 778, 584,
    778, 722, 722, 722, 722, 667, 667, 611,
    556, 556, 556, 556, 556, 556, 889, 556,
    556, 556, 556, 556, 278, 278, 278, 278,
    611, 611, 611, 611, 611, 611, 611, 584,
    611, 611, 611, 611, 611, 556, 611, 556
};

static uint16_t helvetica_bold_oblique_widths[256] = {
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 333, 474, 556, 556, 889, 722, 238,
    333, 333, 389, 584, 278, 333, 278, 278,
    556, 556, 556, 556, 556, 556, 556, 556,
    556, 556, 333, 333, 584, 584, 584, 611,
    975, 722, 722, 722, 722, 667, 611, 778,
    722, 278, 556, 722, 611, 833, 722, 778,
    667, 778, 722, 667, 611, 722, 667, 944,
    667, 667, 611, 333, 278, 333, 584, 556,
    333, 556, 611, 556, 611, 556, 333, 611,
    611, 278, 278, 556, 278, 889, 611, 611,
    611, 611, 389, 556, 333, 611, 556, 778,
    556, 556, 500, 389, 280, 389, 584, 350,
    556, 350, 278, 556, 500, 1000, 556, 556,
    333, 1000, 667, 333, 1000, 350, 611, 350,
    350, 278, 278, 500, 500, 350, 556, 1000,
    333, 1000, 556, 333, 944, 350, 500, 667,
    278, 333, 556, 556, 556, 556, 280, 556,
    333, 737, 370, 556, 584, 333, 737, 333,
    400, 584, 333, 333, 333, 611, 556, 278,
    333, 333, 365, 556, 834, 834, 834, 611,
    722, 722, 722, 722, 722, 722, 1000, 722,
    667, 667, 667, 667, 278, 278, 278, 278,
    722, 722, 778, 778, 778, 778, 778, 584,
    778, 722, 722, 722, 722, 667, 667, 611,
    556, 556, 556, 556, 556, 556, 889, 556,
    556, 556, 556, 556, 278, 278, 278, 278,
    611, 611, 611, 611, 611, 611, 611, 584,
    611, 611, 611, 611, 611, 556, 611, 556
};

static uint16_t helvetica_oblique_widths[256] = {
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 355, 556, 556, 889, 667, 191,
    333, 333, 389, 584, 278, 333, 278, 278,
    556, 556, 556, 556, 556, 556, 556, 556,
    556, 556, 278, 278, 584, 584, 584, 556,
    1015, 667, 667, 722, 722, 667, 611, 778,
    722, 278, 500, 667, 556, 833, 722, 778,
    667, 778, 722, 667, 611, 722, 667, 944,
    667, 667, 611, 278, 278, 278, 469, 556,
    333, 556, 556, 500, 556, 556, 278, 556,
    556, 222, 222, 500, 222, 833, 556, 556,
    556, 556, 333, 500, 278, 556, 500, 722,
    500, 500, 500, 334, 260, 334, 584, 350,
    556, 350, 222, 556, 333, 1000, 556, 556,
    333, 1000, 667, 333, 1000, 350, 611, 350,
    350, 222, 222, 333, 333, 350, 556, 1000,
    333, 1000, 500, 333, 944, 350, 500, 667,
    278, 333, 556, 556, 556, 556, 260, 556,
    333, 737, 370, 556, 584, 333, 737, 333,
    400, 584, 333, 333, 333, 556, 537, 278,
    333, 333, 365, 556, 834, 834, 834, 611,
    667, 667, 667, 667, 667, 667, 1000, 722,
    667, 667, 667, 667, 278, 278, 278, 278,
    722, 722, 778, 778, 778, 778, 778, 584,
    778, 722, 722, 722, 722, 667, 667, 611,
    556, 556, 556, 556, 556, 556, 889, 500,
    556, 556, 556, 556, 278, 278, 278, 278,
    556, 556, 556, 556, 556, 556, 556, 584,
    611, 556, 556, 556, 556, 500, 556, 500};

static uint16_t symbol_widths[256] = {
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 333, 713, 500, 549, 833, 778, 439,
    333, 333, 500, 549, 250, 549, 250, 278,
    500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 278, 278, 549, 549, 549, 444,
    549, 722, 667, 722, 612, 611, 763, 603,
    722, 333, 631, 722, 686, 889, 722, 722,
    768, 741, 556, 592, 611, 690, 439, 768,
    645, 795, 611, 333, 863, 333, 658, 500,
    500, 631, 549, 549, 494, 439, 521, 411,
    603, 329, 603, 549, 549, 576, 521, 549,
    549, 521, 549, 603, 439, 576, 713, 686,
    493, 686, 494, 480, 200, 480, 549, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    750, 620, 247, 549, 167, 713, 500, 753,
    753, 753, 753, 1042, 987, 603, 987, 603,
    400, 549, 411, 549, 549, 713, 494, 460,
    549, 549, 549, 549, 1000, 603, 1000, 658,
    823, 686, 795, 987, 768, 768, 823, 768,
    768, 713, 713, 713, 713, 713, 713, 713,
    768, 713, 790, 790, 890, 823, 549, 250,
    713, 603, 603, 1042, 987, 603, 987, 603,
    494, 329, 790, 790, 786, 713, 384, 384,
    384, 384, 384, 384, 494, 494, 494, 494,
    0, 329, 274, 686, 686, 686, 384, 384,
    384, 384, 384, 384, 494, 494, 494, 0
};

static uint16_t times_widths[256] = {
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 333, 408, 500, 500, 833, 778, 180,
    333, 333, 500, 564, 250, 333, 250, 278,
    500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 278, 278, 564, 564, 564, 444,
    921, 722, 667, 667, 722, 611, 556, 722,
    722, 333, 389, 722, 611, 889, 722, 722,
    556, 722, 667, 556, 611, 722, 722, 944,
    722, 722, 611, 333, 278, 333, 469, 500,
    333, 444, 500, 444, 500, 444, 333, 500,
    500, 278, 278, 500, 278, 778, 500, 500,
    500, 500, 333, 389, 278, 500, 500, 722,
    500, 500, 444, 480, 200, 480, 541, 350,
    500, 350, 333, 500, 444, 1000, 500, 500,
    333, 1000, 556, 333, 889, 350, 611, 350,
    350, 333, 333, 444, 444, 350, 500, 1000,
    333, 980, 389, 333, 722, 350, 444, 722,
    250, 333, 500, 500, 500, 500, 200, 500,
    333, 760, 276, 500, 564, 333, 760, 333,
    400, 564, 300, 300, 333, 500, 453, 250,
    333, 300, 310, 500, 750, 750, 750, 444,
    722, 722, 722, 722, 722, 722, 889, 667,
    611, 611, 611, 611, 333, 333, 333, 333,
    722, 722, 722, 722, 722, 722, 722, 564,
    722, 722, 722, 722, 722, 722, 556, 500,
    444, 444, 444, 444, 444, 444, 667, 444,
    444, 444, 444, 444, 278, 278, 278, 278,
    500, 500, 500, 500, 500, 500, 500, 564,
    500, 500, 500, 500, 500, 500, 500, 500
};

static uint16_t times_bold_widths[256] = {
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 333, 555, 500, 500, 1000, 833, 278,
    333, 333, 500, 570, 250, 333, 250, 278,
    500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 333, 333, 570, 570, 570, 500,
    930, 722, 667, 722, 722, 667, 611, 778,
    778, 389, 500, 778, 667, 944, 722, 778,
    611, 778, 722, 556, 667, 722, 722, 1000,
    722, 722, 667, 333, 278, 333, 581, 500,
    333, 500, 556, 444, 556, 444, 333, 500,
    556, 278, 333, 556, 278, 833, 556, 500,
    556, 556, 444, 389, 333, 556, 500, 722,
    500, 500, 444, 394, 220, 394, 520, 350,
    500, 350, 333, 500, 500, 1000, 500, 500,
    333, 1000, 556, 333, 1000, 350, 667, 350,
    350, 333, 333, 500, 500, 350, 500, 1000,
    333, 1000, 389, 333, 722, 350, 444, 722,
    250, 333, 500, 500, 500, 500, 220, 500,
    333, 747, 300, 500, 570, 333, 747, 333,
    400, 570, 300, 300, 333, 556, 540, 250,
    333, 300, 330, 500, 750, 750, 750, 500,
    722, 722, 722, 722, 722, 722, 1000, 722,
    667, 667, 667, 667, 389, 389, 389, 389,
    722, 722, 778, 778, 778, 778, 778, 570,
    778, 722, 722, 722, 722, 722, 611, 556,
    500, 500, 500, 500, 500, 500, 722, 444,
    444, 444, 444, 444, 278, 278, 278, 278,
    500, 556, 500, 500, 500, 500, 500, 570,
    500, 556, 556, 556, 556, 500, 556, 500} ;

static uint16_t times_bold_italic_widths[256] = {
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 389, 555, 500, 500, 833, 778, 278,
    333, 333, 500, 570, 250, 333, 250, 278,
    500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 333, 333, 570, 570, 570, 500,
    832, 667, 667, 667, 722, 667, 667, 722,
    778, 389, 500, 667, 611, 889, 722, 722,
    611, 722, 667, 556, 611, 722, 667, 889,
    667, 611, 611, 333, 278, 333, 570, 500,
    333, 500, 500, 444, 500, 444, 333, 500,
    556, 278, 278, 500, 278, 778, 556, 500,
    500, 500, 389, 389, 278, 556, 444, 667,
    500, 444, 389, 348, 220, 348, 570, 350,
    500, 350, 333, 500, 500, 1000, 500, 500,
    333, 1000, 556, 333, 944, 350, 611, 350,
    350, 333, 333, 500, 500, 350, 500, 1000,
    333, 1000, 389, 333, 722, 350, 389, 611,
    250, 389, 500, 500, 500, 500, 220, 500,
    333, 747, 266, 500, 606, 333, 747, 333,
    400, 570, 300, 300, 333, 576, 500, 250,
    333, 300, 300, 500, 750, 750, 750, 500,
    667, 667, 667, 667, 667, 667, 944, 667,
    667, 667, 667, 667, 389, 389, 389, 389,
    722, 722, 722, 722, 722, 722, 722, 570,
    722, 722, 722, 722, 722, 611, 611, 500,
    500, 500, 500, 500, 500, 500, 722, 444,
    444, 444, 444, 444, 278, 278, 278, 278,
    500, 556, 500, 500, 500, 500, 500, 570,
    500, 556, 556, 556, 556, 444, 500, 444
};

static uint16_t times_italic_widths[256] = {
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250,
    250, 333, 420, 500, 500, 833, 778, 214,
    333, 333, 500, 675, 250, 333, 250, 278,
    500, 500, 500, 500, 500, 500, 500, 500,
    500, 500, 333, 333, 675, 675, 675, 500,
    920, 611, 611, 667, 722, 611, 611, 722,
    722, 333, 444, 667, 556, 833, 667, 722,
    611, 722, 611, 500, 556, 722, 611, 833,
    611, 556, 556, 389, 278, 389, 422, 500,
    333, 500, 500, 444, 500, 444, 278, 500,
    500, 278, 278, 444, 278, 722, 500, 500,
    500, 500, 389, 389, 278, 500, 444, 667,
    444, 444, 389, 400, 275, 400, 541, 350,
    500, 350, 333, 500, 556, 889, 500, 500,
    333, 1000, 500, 333, 944, 350, 556, 350,
    350, 333, 333, 556, 556, 350, 500, 889,
    333, 980, 389, 333, 667, 350, 389, 556,
    250, 389, 500, 500, 500, 500, 275, 500,
    333, 760, 276, 500, 675, 333, 760, 333,
    400, 675, 300, 300, 333, 500, 523, 250,
    333, 300, 310, 500, 750, 750, 750, 500,
    611, 611, 611, 611, 611, 611, 889, 667,
    611, 611, 611, 611, 333, 333, 333, 333,
    722, 667, 722, 722, 722, 722, 722, 675,
    722, 722, 722, 722, 722, 556, 611, 500,
    500, 500, 500, 500, 500, 500, 667, 444,
    444, 444, 444, 444, 278, 278, 278, 278,
    500, 500, 500, 500, 500, 500, 500, 675,
    500, 500, 500, 500, 500, 444, 500, 444
};

static uint16_t zapfdingbats_widths[256] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    278, 974, 961, 974, 980, 719, 789, 790,
    791, 690, 960, 939, 549, 855, 911, 933,
    911, 945, 974, 755, 846, 762, 761, 571,
    677, 763, 760, 759, 754, 494, 552, 537,
    577, 692, 786, 788, 788, 790, 793, 794,
    816, 823, 789, 841, 823, 833, 816, 831,
    923, 744, 723, 749, 790, 792, 695, 776,
    768, 792, 759, 707, 708, 682, 701, 826,
    815, 789, 789, 707, 687, 696, 689, 786,
    787, 713, 791, 785, 791, 873, 761, 762,
    762, 759, 759, 892, 892, 788, 784, 438,
    138, 277, 415, 392, 392, 668, 668, 0,
    390, 390, 317, 317, 276, 276, 509, 509,
    410, 410, 234, 234, 334, 334, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 732, 544, 544, 910, 667, 760, 760,
    776, 595, 694, 626, 788, 788, 788, 788,
    788, 788, 788, 788, 788, 788, 788, 788,
    788, 788, 788, 788, 788, 788, 788, 788,
    788, 788, 788, 788, 788, 788, 788, 788,
    788, 788, 788, 788, 788, 788, 788, 788,
    788, 788, 788, 788, 894, 838, 1016, 458,
    748, 924, 748, 918, 927, 928, 928, 834,
    873, 828, 924, 924, 917, 930, 931, 463,
    883, 836, 836, 867, 867, 696, 696, 874,
    0, 874, 760, 946, 771, 865, 771, 888,
    967, 888, 831, 873, 927, 970, 918, 0
};

static uint16_t courier_widths[256] = {
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
    600, 600, 600, 600, 600, 600, 600, 600,
};

static int pdf_text_pixel_width(const char *text, int text_len, int size,
        const uint16_t *widths)
{
    int i;
    int len = 0;
    if (text_len < 0)
        text_len = strlen(text);

    for (i = 0; i < text_len; i++)
        len += widths[(uint8_t)text[i]];

    /* Our widths arrays are for 14pt fonts */
    return len * size / (14 * 72);
}

static const uint16_t *find_font_widths(const char *font_name)
{
    if (strcmp(font_name, "Helvetica") == 0)
        return helvetica_widths;
    else if (strcmp(font_name, "Helvetica-Bold") == 0)
        return helvetica_bold_widths;
    else if (strcmp(font_name, "Helvetica-BoldOblique") == 0)
        return helvetica_bold_oblique_widths;
    else if (strcmp(font_name, "Helvetica-Oblique") == 0)
        return helvetica_oblique_widths;
    else if (strcmp(font_name, "Courier") == 0 ||
             strcmp(font_name, "Courier-Bold") == 0 ||
             strcmp(font_name, "Courier-BoldOblique") == 0 ||
             strcmp(font_name, "Courier-Oblique") == 0)
        return courier_widths;
    else if (strcmp(font_name, "Times-Roman") == 0)
        return times_widths;
    else if (strcmp(font_name, "Times-Bold") == 0)
        return times_bold_widths;
    else if (strcmp(font_name, "Times-Italic") == 0)
        return times_italic_widths;
    else if (strcmp(font_name, "Times-BoldItalic") == 0)
        return times_bold_italic_widths;
    else if (strcmp(font_name, "Symbol") == 0)
        return symbol_widths;
    else if (strcmp(font_name, "ZapfDingbats") == 0)
        return zapfdingbats_widths;
    else
        return NULL;
}

int pdf_get_font_text_width(struct pdf_doc *pdf, const char *font_name,
        const char *text, int size)
{
    const uint16_t *widths = find_font_widths(pdf->current_font->font.name);

    if (!widths)
        return pdf_set_err(pdf, -EINVAL, "Unable to determine width for font '%s'",
                pdf->current_font->font.name);
    return pdf_text_pixel_width(text, -1, size, widths);
}

static const char *find_word_break(const char *string)
{
    /* Skip over the actual word */
    while (string && *string && !isspace(*string))
        string++;

    return string;
}

int pdf_add_text_wrap(struct pdf_doc *pdf, struct pdf_object *page,
                 const char *text, int size, int xoff, int yoff,
		 uint32_t colour, int wrap_width)
{
    /* Move through the text string, stopping at word boundaries,
     * trying to find the longest text string we can fit in the given width
     */
    const char *start = text;
    const char *last_best = text;
    const char *end = text;
    char line[512];
    const uint16_t *widths;
    int orig_yoff = yoff;

    widths = find_font_widths(pdf->current_font->font.name);
    if (!widths)
        return pdf_set_err(pdf, -EINVAL, "Unable to determine width for font '%s'",
                pdf->current_font->font.name);

    while (start && *start) {
        const char *new_end = find_word_break(end + 1);
        int line_width;
        int output = 0;

        end = new_end;

        line_width = pdf_text_pixel_width(start, end - start, size, widths);

        if (line_width >= wrap_width) {
            if (last_best == start) {
                /* There is a single word that is too long for the line */
                int i;
                /* Find the best character to chop it at */
                for (i = end - start - 1; i > 0; i--)
                    if (pdf_text_pixel_width(start, i, size, widths) < wrap_width)
                        break;

                end = start + i;
            } else
                end = last_best;
            output = 1;
        }
        if (*end == '\0')
            output = 1;

        if (*end == '\n' || *end == '\r')
            output = 1;

        if (output) {
            int len = end - start;
            strncpy(line, start, len);
            line[len] = '\0';
            pdf_add_text(pdf, page, line, size, xoff, yoff, colour);

            if (*end == ' ')
                end++;

            start = last_best = end;
            yoff -= size;
        } else
            last_best = end;
    }

    return orig_yoff - yoff;
}


int pdf_add_line(struct pdf_doc *pdf, struct pdf_object *page,
                 int x1, int y1, int x2, int y2, int width, uint32_t colour)
{
    int ret;
    struct dstr str = {0, 0, 0};

    dstr_append(&str, "BT\r\n");
    dstr_printf(&str, "%d w\r\n", width);
    dstr_printf(&str, "%d %d m\r\n", x1, y1);
    dstr_printf(&str, "/DeviceRGB CS\r\n");
    dstr_printf(&str, "%f %f %f RG\r\n",
            PDF_RGB_R(colour), PDF_RGB_G(colour), PDF_RGB_B(colour));
    dstr_printf(&str, "%d %d l S\r\n", x2, y2);
    dstr_append(&str, "ET");

    ret = pdf_add_stream(pdf, page, str.data);
    dstr_free(&str);

    return ret;
}

int pdf_add_rectangle(struct pdf_doc *pdf, struct pdf_object *page,
                      int x, int y, int width, int height, int border_width,
                      uint32_t colour)
{
    int ret;
    struct dstr str = {0, 0, 0};

    dstr_append(&str, "BT ");
    dstr_printf(&str, "%f %f %f RG ",
            PDF_RGB_R(colour), PDF_RGB_G(colour), PDF_RGB_B(colour));
    dstr_printf(&str, "%d w ", border_width);
    dstr_printf(&str, "%d %d %d %d re S ", x, y, width, height);
    dstr_append(&str, "ET");

    ret = pdf_add_stream(pdf, page, str.data);
    dstr_free(&str);

    return ret;
}

int pdf_add_filled_rectangle(struct pdf_doc *pdf, struct pdf_object *page,
                             int x, int y, int width, int height,
			     int border_width, uint32_t colour)
{
    int ret;
    struct dstr str = {0, 0, 0};

    dstr_append(&str, "BT ");
    dstr_printf(&str, "%f %f %f rg ",
            PDF_RGB_R(colour), PDF_RGB_G(colour), PDF_RGB_B(colour));
    dstr_printf(&str, "%d w ", border_width);
    dstr_printf(&str, "%d %d %d %d re f ", x, y, width, height);
    dstr_append(&str, "ET");

    ret = pdf_add_stream(pdf, page, str.data);
    dstr_free(&str);

    return ret;
}

static const struct {
    uint32_t code;
    char ch;
} code_128a_encoding[] = {
    {0x212222, ' '},
    {0x222122, '!'},
    {0x222221, '"'},
    {0x121223, '#'},
    {0x121322, '$'},
    {0x131222, '%'},
    {0x122213, '&'},
    {0x122312, '\''},
    {0x132212, '('},
    {0x221213, ')'},
    {0x221312, '*'},
    {0x231212, '+'},
    {0x112232, ','},
    {0x122132, '-'},
    {0x122231, '.'},
    {0x113222, '/'},
    {0x123122, '0'},
    {0x123221, '1'},
    {0x223211, '2'},
    {0x221132, '3'},
    {0x221231, '4'},
    {0x213212, '5'},
    {0x223112, '6'},
    {0x312131, '7'},
    {0x311222, '8'},
    {0x321122, '9'},
    {0x321221, ':'},
    {0x312212, ';'},
    {0x322112, '<'},
    {0x322211, '='},
    {0x212123, '>'},
    {0x212321, '?'},
    {0x232121, '@'},
    {0x111323, 'A'},
    {0x131123, 'B'},
    {0x131321, 'C'},
    {0x112313, 'D'},
    {0x132113, 'E'},
    {0x132311, 'F'},
    {0x211313, 'G'},
    {0x231113, 'H'},
    {0x231311, 'I'},
    {0x112133, 'J'},
    {0x112331, 'K'},
    {0x132131, 'L'},
    {0x113123, 'M'},
    {0x113321, 'N'},
    {0x133121, 'O'},
    {0x313121, 'P'},
    {0x211331, 'Q'},
    {0x231131, 'R'},
    {0x213113, 'S'},
    {0x213311, 'T'},
    {0x213131, 'U'},
    {0x311123, 'V'},
    {0x311321, 'W'},
    {0x331121, 'X'},
    {0x312113, 'Y'},
    {0x312311, 'Z'},
    {0x332111, '['},
    {0x314111, '\\'},
    {0x221411, ']'},
    {0x431111, '^'},
    {0x111224, '_'},
    {0x111422, '`'},
    {0x121124, 'a'},
    {0x121421, 'b'},
    {0x141122, 'c'},
    {0x141221, 'd'},
    {0x112214, 'e'},
    {0x112412, 'f'},
    {0x122114, 'g'},
    {0x122411, 'h'},
    {0x142112, 'i'},
    {0x142211, 'j'},
    {0x241211, 'k'},
    {0x221114, 'l'},
    {0x413111, 'm'},
    {0x241112, 'n'},
    {0x134111, 'o'},
    {0x111242, 'p'},
    {0x121142, 'q'},
    {0x121241, 'r'},
    {0x114212, 's'},
    {0x124112, 't'},
    {0x124211, 'u'},
    {0x411212, 'v'},
    {0x421112, 'w'},
    {0x421211, 'x'},
    {0x212141, 'y'},
    {0x214121, 'z'},
    {0x412121, '{'},
    {0x111143, '|'},
    {0x111341, '}'},
    {0x131141, '~'},
    {0x114113, '\0'},
    {0x114311, '\0'},
    {0x411113, '\0'},
    {0x411311, '\0'},
    {0x113141, '\0'},
    {0x114131, '\0'},
    {0x311141, '\0'},
    {0x411131, '\0'},
    {0x211412, '\0'},
    {0x211214, '\0'},
    {0x211232, '\0'},
    {0x2331112, '\0'},
};

static int find_128_encoding(char ch)
{
    int i;
    for (i = 0; i < ARRAY_SIZE(code_128a_encoding); i++) {
        if (code_128a_encoding[i].ch == ch)
            return i;
    }
    return -1;
}

static int pdf_barcode_128a_ch(struct pdf_doc *pdf, struct pdf_object *page,
                               int x, int y, int width, int height,
			       uint32_t colour, int index, int code_len)
{
    uint32_t code = code_128a_encoding[index].code;
    int i;
    int line_width = width / 11;

    for (i = 0; i < code_len; i++) {
        uint8_t shift = (code_len - 1 - i) * 4;
        uint8_t mask = (code >> shift) & 0xf;
        int j;

        if (!(i % 2))
            for (j = 0; j < mask; j++) {
                pdf_add_line(pdf, page, x, y, x, y + height, line_width, colour);
                x += line_width;
            }
        else
            x += line_width * mask;
    }
    return x;
}

static int pdf_add_barcode_128a(struct pdf_doc *pdf, struct pdf_object *page,
                                int x, int y, int width, int height,
				const char *string, uint32_t colour)
{
    const char *s;
    int len = strlen(string) + 3;
    int char_width = width / len;
    int checksum, i;

    for (s = string; *s; s++)
        if (find_128_encoding(*s) < 0)
            return pdf_set_err(pdf, -EINVAL, "Invalid barcode character 0x%x", *s);

    x = pdf_barcode_128a_ch(pdf, page, x, y, char_width, height, colour, 104,
                            6);
    checksum = 104;

    for (i = 1, s = string; *s; s++, i++) {
        int index = find_128_encoding(*s);
        x = pdf_barcode_128a_ch(pdf, page, x, y, char_width, height, colour, index,
                                6);
        checksum += index * i;
    }
    x = pdf_barcode_128a_ch(pdf, page, x, y, char_width, height, colour,
                            checksum % 103, 6);
    pdf_barcode_128a_ch(pdf, page, x, y, char_width, height, colour, 106,
                            7);
    return 0;
}

int pdf_add_barcode(struct pdf_doc *pdf, struct pdf_object *page,
                    int code, int x, int y, int width, int height,
		    const char *string, uint32_t colour)
{
    if (!string || !*string)
        return 0;
    switch (code) {
    case PDF_BARCODE_128A:
        return pdf_add_barcode_128a(pdf, page, x, y,
                                    width, height, string, colour);
    default:
        return pdf_set_err(pdf, -EINVAL, "Invalid barcode code %d", code);
    }
}

static pdf_object *pdf_add_raw_rgb24(struct pdf_doc *pdf,
                                     uint8_t *data, int width, int height)
{
    struct pdf_object *obj;
    char line[1024];
    int len;
    uint8_t *final_data;
    const char *endstream = ">\r\nendstream\r\n";
    int i;

    sprintf(line,
            "<<\r\n/Type /XObject\r\n/Name /Image%d\r\n/Subtype /Image\r\n"
            "/ColorSpace /DeviceRGB\r\n/Height %d\r\n/Width %d\r\n"
            "/BitsPerComponent 8\r\n/Filter /ASCIIHexDecode\r\n"
            "/Length %d\r\n>>stream\r\n",
            flexarray_size(&pdf->objects), height, width, width * height * 3 * 2 + 1);

    len = strlen(line) + width * height * 3 * 2 + strlen(endstream) + 1;
    final_data = malloc(len);
    if (!final_data) {
        pdf_set_err(pdf, -ENOMEM, "Unable to allocate %d bytes memory for image",
                    len);
        return NULL;
    }
    strcpy((char *)final_data, line);
    uint8_t *pos = &final_data[strlen(line)];
    for (i = 0; i < width * height * 3; i++) {
        *pos++ = "0123456789ABCDEF"[(data[i] >> 4 & 0xf)];
        *pos++ = "0123456789ABCDEF"[data[i] & 0xf];
    }
    strcpy((char *)pos, endstream);
    pos += strlen(endstream);

    obj = pdf_add_object(pdf, OBJ_image);
    if (!obj) {
        free(final_data);
        return NULL;
    }
    obj->stream.text = (char *)final_data;
    obj->stream.len = pos - final_data;

    return obj;
}

/* See http://www.64lines.com/jpeg-width-height for details */
static int jpeg_size(unsigned char* data, unsigned int data_size,
                     int *width, int *height)
{
    int i = 0;
    if (data[i] == 0xFF && data[i+1] == 0xD8 && data[i+2] == 0xFF
        && data[i+3] == 0xE0) {
        i += 4;
        if(data[i+2] == 'J' && data[i+3] == 'F' && data[i+4] == 'I'
           && data[i+5] == 'F' && data[i+6] == 0x00) {
            unsigned short block_length = data[i] * 256 + data[i+1];
            while(i<data_size) {
                i+=block_length;
                if(i >= data_size)
                    return -1;
                if(data[i] != 0xFF)
                    return -1;
                if(data[i+1] == 0xC0) {
                    *height = data[i+5]*256 + data[i+6];
                    *width = data[i+7]*256 + data[i+8];
                    return 0;
                } else {
                    i+=2;
                    block_length = data[i] * 256 + data[i+1];
                }
            }
        }
    }

    return -1;
}

static pdf_object *pdf_add_raw_jpeg(struct pdf_doc *pdf,
                                    const char *jpeg_file)
{
    struct stat buf;
    off_t len;
    char *final_data;
    uint8_t *jpeg_data;
    int written = 0;
    FILE *fp;
    struct pdf_object *obj;
    int width, height;

    if (stat(jpeg_file, &buf) < 0) {
        pdf_set_err(pdf, -errno, "Unable to access %s: %s", jpeg_file,
                    strerror(errno));
        return NULL;
    }

    len = buf.st_size;

    if ((fp = fopen(jpeg_file, "rb")) == NULL) {
        pdf_set_err(pdf, -errno, "Unable to open %s: %s", jpeg_file,
                    strerror(errno));
        return NULL;
    }

    jpeg_data = malloc(len);
    if (!jpeg_data) {
        pdf_set_err(pdf, -errno, "Unable to allocate: %s", len);
        fclose(fp);
        return NULL;
    }

    if (fread(jpeg_data, len, 1, fp) != 1) {
        pdf_set_err(pdf, -errno, "Unable to read full jpeg data");
        free(jpeg_data);
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    if (jpeg_size(jpeg_data, len, &width, &height) < 0) {
        free(jpeg_data);
        pdf_set_err(pdf, -EINVAL, "Unable to determine jpeg width/height from %s",
                    jpeg_file);
        return NULL;
    }

    final_data = malloc(len + 1024);
    if (!final_data) {
        pdf_set_err(pdf, -errno, "Unable to allocate jpeg data %d", len + 1024);
        free(jpeg_data);
        return NULL;
    }

    written = sprintf(final_data,
                      "<<\r\n/Type /XObject\r\n/Name /Image%d\r\n"
                      "/Subtype /Image\r\n/ColorSpace /DeviceRGB\r\n"
                      "/Width %d\r\n/Height %d\r\n"
                      "/BitsPerComponent 8\r\n/Filter /DCTDecode\r\n"
                      "/Length %d\r\n>>stream\r\n",
                      flexarray_size(&pdf->objects), width, height, (int)len);
    memcpy(&final_data[written], jpeg_data, len);
    written += len;
    written += sprintf(&final_data[written], "\r\nendstream\r\n");

    free(jpeg_data);

    obj = pdf_add_object(pdf, OBJ_image);
    if (!obj) {
	free(final_data);
        return NULL;
    }
    obj->stream.text = (char *)final_data;
    obj->stream.len = written;

    return obj;
}

static int pdf_add_image(struct pdf_doc *pdf, struct pdf_object *page,
                         struct pdf_object *image, int x, int y, int width,
			 int height)
{
    int ret;
    struct dstr str = {0, 0, 0};

    dstr_append(&str, "q ");
    dstr_printf(&str, "%d 0 0 %d %d %d cm ", width, height, x, y);
    dstr_printf(&str, "/Image%d Do ", image->index);
    dstr_append(&str, "Q");

    ret = pdf_add_stream(pdf, page, str.data);
    dstr_free(&str);
    return ret;
}

int pdf_add_ppm(struct pdf_doc *pdf, struct pdf_object *page,
                int x, int y, int display_width, int display_height,
		const char *ppm_file)
{
    struct pdf_object *obj;
    uint8_t *data;
    FILE *fp;
    char line[1024];
    int width, height;

    if (!page)
        page = pdf_find_last_object(pdf, OBJ_page);

    if (!page)
        return pdf_set_err(pdf, -EINVAL, "Invalid pdf page");

    /* Load the PPM file */
    fp = fopen(ppm_file, "rb");
    if (!fgets(line, sizeof(line) - 1, fp)) {
        fclose(fp);
        return pdf_set_err(pdf, -EINVAL, "Invalid PPM file");
    }

    /* We only support binary ppms */
    if (strncmp(line, "P6", 2) != 0) {
        fclose(fp);
        return pdf_set_err(pdf, -EINVAL, "Only binary PPM files supported");
    }

    /* Find the width line */
    do {
        if (!fgets(line, sizeof(line) - 1, fp)) {
            fclose(fp);
            return pdf_set_err(pdf, -EINVAL, "Unable to find PPM size");
        }
        if (line[0] == '#')
            continue;

        if (sscanf(line, "%d %d\n", &width, &height) != 2) {
            fclose(fp);
            return pdf_set_err(pdf, -EINVAL, "Unable to find PPM size");
        } else
            break;
    } while (1);

    /* Skip over the byte-size line */
    if (!fgets(line, sizeof(line) - 1, fp)) {
        fclose(fp);
        return pdf_set_err(pdf, -EINVAL, "No byte-size line in PPM file");
    }

    data = malloc(width * height * 3);
    if (!data) {
        fclose(fp);
        return pdf_set_err(pdf, -ENOMEM, "Unable to allocate memory for RGB data");
    }
    if (fread(data, 3, width * height, fp) != width * height) {
        free(data);
        fclose(fp);
        return pdf_set_err(pdf, -EINVAL, "Insufficient RGB data available");

    }
    fclose(fp);
    obj = pdf_add_raw_rgb24(pdf, data, width, height);
    free(data);
    if (!obj)
        return pdf->errval;

    return pdf_add_image(pdf, page, obj, x, y, display_width, display_height);
}

int pdf_add_jpeg(struct pdf_doc *pdf, struct pdf_object *page,
                 int x, int y, int display_width, int display_height,
		 const char *jpeg_file)
{
    struct pdf_object *obj;

    obj = pdf_add_raw_jpeg(pdf, jpeg_file);
    if (!obj)
        return pdf->errval;

    return pdf_add_image(pdf, page, obj, x, y, display_width, display_height);
}
