/**
 * Simple engine for creating PDF files.
 * It supports text, shapes, images etc...
 * Capable of handling millions of objects without too much performance
 * penalty.
 * Public domain license - no warrenty implied; use at your own risk.
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
 * https://blog.idrsolutions.com/2013/01/understanding-the-pdf-file-format-overview/
 *
 * To validate the PDF output, there are several online validators:
 * http://www.validatepdfa.com/online.htm
 * http://www.datalogics.com/products/callas/callaspdfA-onlinedemo.asp
 * http://www.pdf-tools.com/pdf/validate-pdfa-online.aspx
 *
 * In addition the 'pdftk' server can be used to analyse the output:
 * https://www.pdflabs.com/docs/pdftk-cli-examples/
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

#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS 1 // Drop the MSVC complaints about snprintf
#define _USE_MATH_DEFINES
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#else
#define _POSIX_SOURCE     /* For localtime_r */
#define _XOPEN_SOURCE 600 /* for M_SQRT2 */
#include <sys/types.h>    /* for ssize_t */
#endif

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "pdfgen.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define PDF_RGB_R(c) (float)((((c) >> 16) & 0xff) / 255.0)
#define PDF_RGB_G(c) (float)((((c) >> 8) & 0xff) / 255.0)
#define PDF_RGB_B(c) (float)((((c) >> 0) & 0xff) / 255.0)
#define PDF_IS_TRANSPARENT(c) (((c) >> 24) == 0xff)

#if defined(_MSC_VER)
#define inline __inline
#define snprintf _snprintf
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#ifdef stat
#undef stat
#endif
#define stat _stat
#define SKIP_ATTRIBUTE
#else
#include <strings.h> // strcasecmp
#endif

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

/**
 * Simple dynamic string object. Tries to store a reasonable amount on the
 * stack before falling back to malloc once things get large
 */
struct dstr {
    char static_data[128];
    char *data;
    size_t alloc_len;
    size_t used_len;
};

struct pdf_object {
    int type;                /* See OBJ_xxxx */
    int index;               /* PDF output index */
    int offset;              /* Byte position within the output file */
    struct pdf_object *prev; /* Previous of this type */
    struct pdf_object *next; /* Next of this type */
    union {
        struct {
            struct pdf_object *page;
            char name[64];
            struct pdf_object *parent;
            struct flexarray children;
        } bookmark;
        struct dstr stream;
        struct {
            int width;
            int height;
            struct flexarray children;
        } page;
        struct pdf_info *info;
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

static inline int flexarray_get_bin(const struct flexarray *flex, int index)
{
    (void)flex;
    for (int i = 0; i < ARRAY_SIZE(bin_offset); i++)
        if (index < bin_offset[i])
            return i - 1;
    return -1;
}

static inline int flexarray_get_bin_size(const struct flexarray *flex,
                                         int bin)
{
    (void)flex;
    if (bin >= ARRAY_SIZE(bin_offset))
        return -1;
    int next = bin_offset[bin + 1];
    return next - bin_offset[bin];
}

static inline int flexarray_get_bin_offset(const struct flexarray *flex,
                                           int bin, int index)
{
    (void)flex;
    return index - bin_offset[bin];
}

static void flexarray_clear(struct flexarray *flex)
{
    for (int i = 0; i < flex->bin_count; i++)
        free(flex->bins[i]);
    free(flex->bins);
    flex->bin_count = 0;
    flex->item_count = 0;
}

static inline int flexarray_size(const struct flexarray *flex)
{
    return flex->item_count;
}

static int flexarray_set(struct flexarray *flex, int index, void *data)
{
    int bin = flexarray_get_bin(flex, index);
    if (bin < 0)
        return -EINVAL;
    if (bin >= flex->bin_count) {
        void *bins =
            realloc(flex->bins, (flex->bin_count + 1) * sizeof(flex->bins));
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

static inline void *flexarray_get(const struct flexarray *flex, int index)
{
    int bin;

    if (index >= flex->item_count)
        return NULL;
    bin = flexarray_get_bin(flex, index);
    if (bin < 0 || bin >= flex->bin_count)
        return NULL;
    return flex->bins[bin][flexarray_get_bin_offset(flex, bin, index)];
}

/**
 * Simple dynamic string object. Tries to store a reasonable amount on the
 * stack before falling back to malloc once things get large
 */

#define INIT_DSTR                                                            \
    (struct dstr)                                                            \
    {                                                                        \
        .static_data = {0}, .data = NULL, .alloc_len = 0, .used_len = 0      \
    }

static char *dstr_data(const struct dstr *str)
{
    return str->data ? str->data : (char *)str->static_data;
}

static size_t dstr_len(const struct dstr *str)
{
    return str->used_len;
}

static ssize_t dstr_ensure(struct dstr *str, size_t len)
{
    if (len <= str->alloc_len)
        return 0;
    if (!str->data && len <= sizeof(str->static_data))
        str->alloc_len = len;
    else if (str->alloc_len < len) {
        char *new_data;
        size_t new_len;

        new_len = len + 4096;
        new_data = realloc(str->data, new_len);
        if (!new_data)
            return -ENOMEM;
        // If we move beyond the on-stack buffer, copy the old data out
        if (!str->data && str->used_len > 0)
            memcpy(new_data, str->static_data, str->used_len + 1);
        str->data = new_data;
        str->alloc_len = new_len;
    }
    return 0;
}

#ifndef SKIP_ATTRIBUTE
static int dstr_printf(struct dstr *str, const char *fmt, ...)
    __attribute__((format(gnu_printf, 2, 3)));
#endif
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
    vsprintf(dstr_data(str) + str->used_len, fmt, aq);
    str->used_len += len;
    va_end(ap);
    va_end(aq);

    return len;
}

static ssize_t dstr_append_data(struct dstr *str, const void *extend,
                                size_t len)
{
    if (dstr_ensure(str, str->used_len + len + 1) < 0)
        return -ENOMEM;
    memcpy(dstr_data(str) + str->used_len, extend, len);
    str->used_len += len;
    dstr_data(str)[str->used_len] = '\0';
    return len;
}

static ssize_t dstr_append(struct dstr *str, const char *extend)
{
    return dstr_append_data(str, extend, strlen(extend));
}

static void dstr_free(struct dstr *str)
{
    if (str->data)
        free(str->data);
    *str = INIT_DSTR;
}

/**
 * PDF Implementation
 */

#ifndef SKIP_ATTRIBUTE
static int pdf_set_err(struct pdf_doc *doc, int errval, const char *buffer,
                       ...) __attribute__((format(gnu_printf, 3, 4)));
#endif
static int pdf_set_err(struct pdf_doc *doc, int errval, const char *buffer,
                       ...)
{
    va_list ap;
    int len;

    va_start(ap, buffer);
    len = vsnprintf(doc->errstr, sizeof(doc->errstr) - 1, buffer, ap);
    va_end(ap);

    if (len < 0) {
        doc->errstr[0] = '\0';
        return errval;
    }

    if (len >= sizeof(doc->errstr) - 1)
        len = sizeof(doc->errstr) - 1;

    /* Make sure we're properly terminated */
    if (len > 0 && doc->errstr[len - 1] != '\n')
        doc->errstr[len - 1] = '\n';
    doc->errstr[len] = '\0';
    doc->errval = errval;

    return errval;
}

const char *pdf_get_err(const struct pdf_doc *pdf, int *errval)
{
    if (!pdf)
        return NULL;
    if (pdf->errstr[0] == '\0')
        return NULL;
    if (errval)
        *errval = pdf->errval;
    return pdf->errstr;
}

void pdf_clear_err(struct pdf_doc *pdf)
{
    if (!pdf)
        return;
    pdf->errstr[0] = '\0';
    pdf->errval = 0;
}

static struct pdf_object *pdf_get_object(const struct pdf_doc *pdf, int index)
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

    return 0;
}

static void pdf_object_destroy(struct pdf_object *object)
{
    switch (object->type) {
    case OBJ_stream:
    case OBJ_image:
        dstr_free(&object->stream);
        break;
    case OBJ_page:
        flexarray_clear(&object->page.children);
        break;
    case OBJ_info:
        free(object->info);
        break;
    case OBJ_bookmark:
        flexarray_clear(&object->bookmark.children);
        break;
    }
    free(object);
}

static struct pdf_object *pdf_add_object(struct pdf_doc *pdf, int type)
{
    struct pdf_object *obj;

    obj = calloc(1, sizeof(*obj));
    if (!obj) {
        pdf_set_err(pdf, -errno,
                    "Unable to allocate object %d of type %d: %s",
                    flexarray_size(&pdf->objects) + 1, type, strerror(errno));
        return NULL;
    }

    obj->type = type;

    if (pdf_append_object(pdf, obj) < 0) {
        free(obj);
        return NULL;
    }

    return obj;
}

static void pdf_del_object(struct pdf_doc *pdf, struct pdf_object *obj)
{
    int type = obj->type;
    flexarray_set(&pdf->objects, obj->index, NULL);
    if (pdf->last_objects[type] == obj) {
        pdf->last_objects[type] = NULL;
        for (int i = 0; i < flexarray_size(&pdf->objects); i++) {
            struct pdf_object *o = pdf_get_object(pdf, i);
            if (o && o->type == type)
                pdf->last_objects[type] = o;
        }
    }

    if (pdf->first_objects[type] == obj) {
        pdf->first_objects[type] = NULL;
        for (int i = 0; i < flexarray_size(&pdf->objects); i++) {
            struct pdf_object *o = pdf_get_object(pdf, i);
            if (o && o->type == type) {
                pdf->first_objects[type] = o;
                break;
            }
        }
    }

    pdf_object_destroy(obj);
}

struct pdf_doc *pdf_create(int width, int height, const struct pdf_info *info)
{
    struct pdf_doc *pdf;
    struct pdf_object *obj;

    pdf = calloc(1, sizeof(*pdf));
    if (!pdf)
        return NULL;
    pdf->width = width;
    pdf->height = height;

    /* We don't want to use ID 0 */
    pdf_add_object(pdf, OBJ_none);

    /* Create the 'info' object */
    obj = pdf_add_object(pdf, OBJ_info);
    if (!obj) {
        pdf_destroy(pdf);
        return NULL;
    }
    obj->info = calloc(sizeof(*obj->info), 1);
    if (!obj->info) {
        pdf_destroy(pdf);
        return NULL;
    }
    if (info) {
        *obj->info = *info;
        obj->info->creator[sizeof(obj->info->creator) - 1] = '\0';
        obj->info->producer[sizeof(obj->info->producer) - 1] = '\0';
        obj->info->title[sizeof(obj->info->title) - 1] = '\0';
        obj->info->author[sizeof(obj->info->author) - 1] = '\0';
        obj->info->subject[sizeof(obj->info->subject) - 1] = '\0';
        obj->info->date[sizeof(obj->info->date) - 1] = '\0';
    }
    /* FIXME: Should be quoting PDF strings? */
    if (!obj->info->date[0]) {
        time_t now = time(NULL);
        struct tm tm;
#ifdef _WIN32
        struct tm *tmp;
        tmp = localtime(&now);
        tm = *tmp;
#else
        localtime_r(&now, &tm);
#endif
        strftime(obj->info->date, sizeof(obj->info->date), "%Y%m%d%H%M%SZ",
                 &tm);
    }
    if (!obj->info->creator[0])
        snprintf(obj->info->creator, sizeof(obj->info->creator), "pdfgen");
    if (!obj->info->producer[0])
        snprintf(obj->info->producer, sizeof(obj->info->producer), "pdfgen");
    if (!obj->info->title[0])
        snprintf(obj->info->title, sizeof(obj->info->title), "pdfgen");
    if (!obj->info->author[0])
        snprintf(obj->info->author, sizeof(obj->info->author), "pdfgen");
    if (!obj->info->subject[0])
        snprintf(obj->info->subject, sizeof(obj->info->subject), "pdfgen");

    if (!pdf_add_object(pdf, OBJ_pages)) {
        pdf_destroy(pdf);
        return NULL;
    }
    if (!pdf_add_object(pdf, OBJ_catalog)) {
        pdf_destroy(pdf);
        return NULL;
    }

    if (pdf_set_font(pdf, "Times-Roman") < 0) {
        pdf_destroy(pdf);
        return NULL;
    }

    return pdf;
}

int pdf_width(const struct pdf_doc *pdf)
{
    return pdf->width;
}

int pdf_height(const struct pdf_doc *pdf)
{
    return pdf->height;
}

void pdf_destroy(struct pdf_doc *pdf)
{
    if (pdf) {
        for (int i = 0; i < flexarray_size(&pdf->objects); i++)
            pdf_object_destroy(pdf_get_object(pdf, i));
        flexarray_clear(&pdf->objects);
        free(pdf);
    }
}

static struct pdf_object *pdf_find_first_object(const struct pdf_doc *pdf,
                                                int type)
{
    return pdf->first_objects[type];
}

static struct pdf_object *pdf_find_last_object(const struct pdf_doc *pdf,
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

    page->page.width = pdf->width;
    page->page.height = pdf->height;

    return page;
}

int pdf_page_set_size(struct pdf_doc *pdf, struct pdf_object *page, int width,
                      int height)
{
    if (!page)
        page = pdf_find_last_object(pdf, OBJ_page);

    if (!page || page->type != OBJ_page)
        return pdf_set_err(pdf, -EINVAL, "Invalid PDF page");
    page->page.width = width;
    page->page.height = height;
    return 0;
}

static int pdf_save_object(struct pdf_doc *pdf, FILE *fp, int index)
{
    struct pdf_object *object = pdf_get_object(pdf, index);
    if (!object)
        return -ENOENT;

    if (object->type == OBJ_none)
        return -ENOENT;

    object->offset = ftell(fp);

    fprintf(fp, "%d 0 obj\r\n", index);

    switch (object->type) {
    case OBJ_stream:
    case OBJ_image: {
        fwrite(dstr_data(&object->stream), dstr_len(&object->stream), 1, fp);
        break;
    }
    case OBJ_info: {
        struct pdf_info *info = object->info;

        fprintf(fp,
                "<<\r\n"
                "  /Creator (%s)\r\n"
                "  /Producer (%s)\r\n"
                "  /Title (%s)\r\n"
                "  /Author (%s)\r\n"
                "  /Subject (%s)\r\n"
                "  /CreationDate (D:%s)\r\n"
                ">>\r\n",
                info->creator, info->producer, info->title, info->author,
                info->subject, info->date);
        break;
    }

    case OBJ_page: {
        struct pdf_object *pages = pdf_find_first_object(pdf, OBJ_pages);
        struct pdf_object *image = pdf_find_first_object(pdf, OBJ_image);

        fprintf(fp,
                "<<\r\n"
                "/Type /Page\r\n"
                "/Parent %d 0 R\r\n",
                pages->index);
        fprintf(fp, "/MediaBox [0 0 %d %d]\r\n", object->page.width,
                object->page.height);
        fprintf(fp, "/Resources <<\r\n");
        fprintf(fp, "  /Font <<\r\n");
        for (struct pdf_object *font = pdf_find_first_object(pdf, OBJ_font);
             font; font = font->next)
            fprintf(fp, "    /F%d %d 0 R\r\n", font->font.index, font->index);
        fprintf(fp, "  >>\r\n");
        // We trim transparency to just 4-bits
        fprintf(fp, "  /ExtGState <<\r\n");
        for (int i = 0; i < 16; i++) {
            fprintf(fp, "  /GS%d <</ca %f>>\r\n", i, (float)(15 - i) / 15);
        }
        fprintf(fp, "  >>\r\n");

        if (image) {
            fprintf(fp, "  /XObject <<");
            for (; image; image = image->next)
                fprintf(fp, "/Image%d %d 0 R ", image->index, image->index);
            fprintf(fp, ">>\r\n");
        }

        fprintf(fp, ">>\r\n");
        fprintf(fp, "/Contents [\r\n");
        for (int i = 0; i < flexarray_size(&object->page.children); i++) {
            struct pdf_object *child =
                flexarray_get(&object->page.children, i);
            fprintf(fp, "%d 0 R\r\n", child->index);
        }
        fprintf(fp, "]\r\n");
        fprintf(fp, ">>\r\n");
        break;
    }

    case OBJ_bookmark: {
        struct pdf_object *parent, *other;

        parent = object->bookmark.parent;
        if (!parent)
            parent = pdf_find_first_object(pdf, OBJ_outline);
        if (!object->bookmark.page)
            break;
        fprintf(fp,
                "<<\r\n"
                "/A << /Type /Action\r\n"
                "      /S /GoTo\r\n"
                "      /D [%d 0 R /XYZ 0 %d null]\r\n"
                "   >>\r\n"
                "/Parent %d 0 R\r\n"
                "/Title (%s)\r\n",
                object->bookmark.page->index, pdf->height, parent->index,
                object->bookmark.name);
        int nchildren = flexarray_size(&object->bookmark.children);
        if (nchildren > 0) {
            struct pdf_object *f, *l;
            f = flexarray_get(&object->bookmark.children, 0);
            l = flexarray_get(&object->bookmark.children, nchildren - 1);
            fprintf(fp, "/First %d 0 R\r\n", f->index);
            fprintf(fp, "/Last %d 0 R\r\n", l->index);
        }
        // Find the previous bookmark with the same parent
        for (other = object->prev;
             other && other->bookmark.parent != object->bookmark.parent;
             other = other->prev)
            ;
        if (other)
            fprintf(fp, "/Prev %d 0 R\r\n", other->index);
        // Find the next bookmark with the same parent
        for (other = object->next;
             other && other->bookmark.parent != object->bookmark.parent;
             other = other->next)
            ;
        if (other)
            fprintf(fp, "/Next %d 0 R\r\n", other->index);
        fprintf(fp, ">>\r\n");
        break;
    }

    case OBJ_outline: {
        struct pdf_object *first, *last, *cur;
        first = pdf_find_first_object(pdf, OBJ_bookmark);
        last = pdf_find_last_object(pdf, OBJ_bookmark);

        if (first && last) {
            int count = 0;
            cur = first;
            while (cur) {
                if (!cur->bookmark.parent)
                    count++;
                cur = cur->next;
            }

            /* Bookmark outline */
            fprintf(fp,
                    "<<\r\n"
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
        fprintf(fp,
                "<<\r\n"
                "  /Type /Font\r\n"
                "  /Subtype /Type1\r\n"
                "  /BaseFont /%s\r\n"
                "  /Encoding /WinAnsiEncoding\r\n"
                ">>\r\n",
                object->font.name);
        break;

    case OBJ_pages: {
        int npages = 0;

        fprintf(fp, "<<\r\n"
                    "/Type /Pages\r\n"
                    "/Kids [ ");
        for (struct pdf_object *page = pdf_find_first_object(pdf, OBJ_page);
             page; page = page->next) {
            npages++;
            fprintf(fp, "%d 0 R ", page->index);
        }
        fprintf(fp, "]\r\n");
        fprintf(fp, "/Count %d\r\n", npages);
        fprintf(fp, ">>\r\n");
        break;
    }

    case OBJ_catalog: {
        struct pdf_object *outline = pdf_find_first_object(pdf, OBJ_outline);
        struct pdf_object *pages = pdf_find_first_object(pdf, OBJ_pages);

        fprintf(fp, "<<\r\n"
                    "/Type /Catalog\r\n");
        if (outline)
            fprintf(fp,
                    "/Outlines %d 0 R\r\n"
                    "/PageMode /UseOutlines\r\n",
                    outline->index);
        fprintf(fp,
                "/Pages %d 0 R\r\n"
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

// Slightly modified djb2 hash algorithm to get pseudo-random ID
static uint64_t hash(uint64_t hash, const void *data, size_t len)
{
    const uint8_t *d8 = (const uint8_t *)data;
    while (len--)
        hash = ((hash << 5) + hash) + *d8++;
    return hash;
}

int pdf_save_file(struct pdf_doc *pdf, FILE *fp)
{
    struct pdf_object *obj;
    int xref_offset;
    int xref_count = 0;
    uint64_t id1, id2;
    time_t now = time(NULL);

    fprintf(fp, "%%PDF-1.2\r\n");
    /* Hibit bytes */
    fprintf(fp, "%c%c%c%c%c\r\n", 0x25, 0xc7, 0xec, 0x8f, 0xa2);

    /* Dump all the objects & get their file offsets */
    for (int i = 0; i < flexarray_size(&pdf->objects); i++)
        if (pdf_save_object(pdf, fp, i) >= 0)
            xref_count++;

    /* xref */
    xref_offset = ftell(fp);
    fprintf(fp, "xref\r\n");
    fprintf(fp, "0 %d\r\n", xref_count + 1);
    fprintf(fp, "0000000000 65535 f\r\n");
    for (int i = 0; i < flexarray_size(&pdf->objects); i++) {
        obj = pdf_get_object(pdf, i);
        if (obj->type != OBJ_none)
            fprintf(fp, "%10.10d 00000 n\r\n", obj->offset);
    }

    fprintf(fp,
            "trailer\r\n"
            "<<\r\n"
            "/Size %d\r\n",
            xref_count + 1);
    obj = pdf_find_first_object(pdf, OBJ_catalog);
    fprintf(fp, "/Root %d 0 R\r\n", obj->index);
    obj = pdf_find_first_object(pdf, OBJ_info);
    fprintf(fp, "/Info %d 0 R\r\n", obj->index);
    /* Generate document unique IDs */
    id1 = hash(5381, obj->info, sizeof(struct pdf_info));
    id1 = hash(id1, &xref_count, sizeof(xref_count));
    id2 = hash(5381, &now, sizeof(now));
    fprintf(fp, "/ID [<%16.16" PRIx64 "> <%16.16" PRIx64 ">]\r\n", id1, id2);
    fprintf(fp, ">>\r\n"
                "startxref\r\n");
    fprintf(fp, "%d\r\n", xref_offset);
    fprintf(fp, "%%%%EOF\r\n");

    return 0;
}

int pdf_save(struct pdf_doc *pdf, const char *filename)
{
    FILE *fp;
    int e;

    if (filename == NULL)
        fp = stdout;
    else if ((fp = fopen(filename, "wb")) == NULL)
        return pdf_set_err(pdf, -errno, "Unable to open '%s': %s", filename,
                           strerror(errno));

    e = pdf_save_file(pdf, fp);

    if (fp != stdout)
        if (fclose(fp) != 0 && e >= 0)
            return pdf_set_err(pdf, -errno, "Unable to close '%s': %s",
                               filename, strerror(errno));

    return e;
}

static int pdf_add_stream(struct pdf_doc *pdf, struct pdf_object *page,
                          const char *buffer)
{
    struct pdf_object *obj;
    size_t len;

    if (!page)
        page = pdf_find_last_object(pdf, OBJ_page);

    if (!page)
        return pdf_set_err(pdf, -EINVAL, "Invalid pdf page");

    len = strlen(buffer);
    /* We don't want any trailing whitespace in the stream */
    while (len >= 1 && (buffer[len - 1] == '\r' || buffer[len - 1] == '\n'))
        len--;

    obj = pdf_add_object(pdf, OBJ_stream);
    if (!obj)
        return pdf->errval;

    dstr_printf(&obj->stream, "<< /Length %zd >>stream\r\n", len);
    dstr_append_data(&obj->stream, buffer, len);
    dstr_append(&obj->stream, "\r\nendstream\r\n");

    return flexarray_append(&page->page.children, obj);
}

int pdf_add_bookmark(struct pdf_doc *pdf, struct pdf_object *page, int parent,
                     const char *name)
{
    struct pdf_object *obj, *outline = NULL;

    if (!page)
        page = pdf_find_last_object(pdf, OBJ_page);

    if (!page)
        return pdf_set_err(pdf, -EINVAL,
                           "Unable to add bookmark, no pages available");

    if (!pdf_find_first_object(pdf, OBJ_outline)) {
        outline = pdf_add_object(pdf, OBJ_outline);
        if (!outline)
            return pdf->errval;
    }

    obj = pdf_add_object(pdf, OBJ_bookmark);
    if (!obj) {
        if (outline)
            pdf_del_object(pdf, outline);
        return pdf->errval;
    }

    strncpy(obj->bookmark.name, name, sizeof(obj->bookmark.name));
    obj->bookmark.name[sizeof(obj->bookmark.name) - 1] = '\0';
    obj->bookmark.page = page;
    if (parent >= 0) {
        struct pdf_object *parent_obj = pdf_get_object(pdf, parent);
        if (!parent_obj)
            return pdf_set_err(pdf, -EINVAL, "Invalid parent ID %d supplied",
                               parent);
        obj->bookmark.parent = parent_obj;
        flexarray_append(&parent_obj->bookmark.children, obj);
    }

    return obj->index;
}

static int utf8_to_utf32(const char *utf8, int len, uint32_t *utf32)
{
    uint32_t ch = *utf8;
    uint8_t mask;

    if ((ch & 0x80) == 0) {
        len = 1;
        mask = 0x7f;
    } else if ((ch & 0xe0) == 0xc0 && len >= 2) {
        len = 2;
        mask = 0x1f;
    } else if ((ch & 0xf0) == 0xe0 && len >= 3) {
        len = 3;
        mask = 0xf;
    } else if ((ch & 0xf8) == 0xf0 && len >= 4) {
        len = 4;
        mask = 0x7;
    } else
        return -EINVAL;

    ch = 0;
    for (int i = 0; i < len; i++) {
        int shift = (len - i - 1) * 6;
        if (i == 0)
            ch |= ((uint32_t)(*utf8++) & mask) << shift;
        else
            ch |= ((uint32_t)(*utf8++) & 0x3f) << shift;
    }

    *utf32 = ch;

    return len;
}

static int pdf_add_text_spacing(struct pdf_doc *pdf, struct pdf_object *page,
                                const char *text, int size, int xoff,
                                int yoff, uint32_t colour, double spacing)
{
    int ret;
    size_t len = text ? strlen(text) : 0;
    struct dstr str = INIT_DSTR;
    int alpha = (colour >> 24) >> 4;

    /* Don't bother adding empty/null strings */
    if (!len)
        return 0;

    dstr_append(&str, "BT ");
    dstr_printf(&str, "/GS%d gs ", alpha);
    dstr_printf(&str, "%d %d TD ", xoff, yoff);
    dstr_printf(&str, "/F%d %d Tf ", pdf->current_font->font.index, size);
    dstr_printf(&str, "%f %f %f rg ", PDF_RGB_R(colour), PDF_RGB_G(colour),
                PDF_RGB_B(colour));
    dstr_printf(&str, "%f Tc ", spacing);
    dstr_append(&str, "(");

    /* Escape magic characters properly */
    for (size_t i = 0; i < len;) {
        uint32_t code;
        int code_len;
        code_len = utf8_to_utf32(&text[i], len - i, &code);
        if (code_len < 0) {
            dstr_free(&str);
            return pdf_set_err(pdf, -EINVAL, "Invalid UTF-8 encoding");
        }

        if (code > 255) {
            /* We support *some* minimal UTF-8 characters */
            char buf[5] = {0};
            switch (code) {
            case 0x160:
                buf[0] = '\x8a';
                break;
            case 0x161:
                buf[0] = '\x9a';
                break;
            case 0x17d:
                buf[0] = '\x8e';
                break;
            case 0x17e:
                buf[0] = '\x9e';
                break;
            case 0x20ac:
                strcpy(buf, "\\200");
                break;
            default:
                dstr_free(&str);
                return pdf_set_err(pdf, -EINVAL,
                                   "Unsupported UTF-8 character: 0x%x 0o%o",
                                   code, code);
            }
            dstr_append(&str, buf);
        } else if (strchr("()\\", code)) {
            char buf[3];
            /* Escape some characters */
            buf[0] = '\\';
            buf[1] = code;
            buf[2] = '\0';
            dstr_append(&str, buf);
        } else if (strrchr("\n\r\t\b\f", code)) {
            /* Skip over these characters */
            ;
        } else {
            char buf[2];
            buf[0] = code;
            buf[1] = '\0';
            dstr_append(&str, buf);
        }

        i += code_len;
    }
    dstr_append(&str, ") Tj ");
    dstr_append(&str, "ET");

    ret = pdf_add_stream(pdf, page, dstr_data(&str));
    dstr_free(&str);
    return ret;
}

int pdf_add_text(struct pdf_doc *pdf, struct pdf_object *page,
                 const char *text, int size, int xoff, int yoff,
                 uint32_t colour)
{
    return pdf_add_text_spacing(pdf, page, text, size, xoff, yoff, colour, 0);
}

/* How wide is each character, in points, at size 14 */
static const uint16_t helvetica_widths[256] = {
    280, 280, 280, 280,  280, 280, 280, 280,  280,  280, 280,  280, 280,
    280, 280, 280, 280,  280, 280, 280, 280,  280,  280, 280,  280, 280,
    280, 280, 280, 280,  280, 280, 280, 280,  357,  560, 560,  896, 672,
    192, 335, 335, 392,  588, 280, 335, 280,  280,  560, 560,  560, 560,
    560, 560, 560, 560,  560, 560, 280, 280,  588,  588, 588,  560, 1023,
    672, 672, 727, 727,  672, 615, 784, 727,  280,  504, 672,  560, 839,
    727, 784, 672, 784,  727, 672, 615, 727,  672,  951, 672,  672, 615,
    280, 280, 280, 472,  560, 335, 560, 560,  504,  560, 560,  280, 560,
    560, 223, 223, 504,  223, 839, 560, 560,  560,  560, 335,  504, 280,
    560, 504, 727, 504,  504, 504, 336, 262,  336,  588, 352,  560, 352,
    223, 560, 335, 1008, 560, 560, 335, 1008, 672,  335, 1008, 352, 615,
    352, 352, 223, 223,  335, 335, 352, 560,  1008, 335, 1008, 504, 335,
    951, 352, 504, 672,  280, 335, 560, 560,  560,  560, 262,  560, 335,
    742, 372, 560, 588,  335, 742, 335, 403,  588,  335, 335,  335, 560,
    541, 280, 335, 335,  367, 560, 840, 840,  840,  615, 672,  672, 672,
    672, 672, 672, 1008, 727, 672, 672, 672,  672,  280, 280,  280, 280,
    727, 727, 784, 784,  784, 784, 784, 588,  784,  727, 727,  727, 727,
    672, 672, 615, 560,  560, 560, 560, 560,  560,  896, 504,  560, 560,
    560, 560, 280, 280,  280, 280, 560, 560,  560,  560, 560,  560, 560,
    588, 615, 560, 560,  560, 560, 504, 560,  504,
};

static const uint16_t helvetica_bold_widths[256] = {
    280,  280, 280,  280, 280, 280, 280, 280,  280, 280, 280, 280,  280, 280,
    280,  280, 280,  280, 280, 280, 280, 280,  280, 280, 280, 280,  280, 280,
    280,  280, 280,  280, 280, 335, 477, 560,  560, 896, 727, 239,  335, 335,
    392,  588, 280,  335, 280, 280, 560, 560,  560, 560, 560, 560,  560, 560,
    560,  560, 335,  335, 588, 588, 588, 615,  982, 727, 727, 727,  727, 672,
    615,  784, 727,  280, 560, 727, 615, 839,  727, 784, 672, 784,  727, 672,
    615,  727, 672,  951, 672, 672, 615, 335,  280, 335, 588, 560,  335, 560,
    615,  560, 615,  560, 335, 615, 615, 280,  280, 560, 280, 896,  615, 615,
    615,  615, 392,  560, 335, 615, 560, 784,  560, 560, 504, 392,  282, 392,
    588,  352, 560,  352, 280, 560, 504, 1008, 560, 560, 335, 1008, 672, 335,
    1008, 352, 615,  352, 352, 280, 280, 504,  504, 352, 560, 1008, 335, 1008,
    560,  335, 951,  352, 504, 672, 280, 335,  560, 560, 560, 560,  282, 560,
    335,  742, 372,  560, 588, 335, 742, 335,  403, 588, 335, 335,  335, 615,
    560,  280, 335,  335, 367, 560, 840, 840,  840, 615, 727, 727,  727, 727,
    727,  727, 1008, 727, 672, 672, 672, 672,  280, 280, 280, 280,  727, 727,
    784,  784, 784,  784, 784, 588, 784, 727,  727, 727, 727, 672,  672, 615,
    560,  560, 560,  560, 560, 560, 896, 560,  560, 560, 560, 560,  280, 280,
    280,  280, 615,  615, 615, 615, 615, 615,  615, 588, 615, 615,  615, 615,
    615,  560, 615,  560,
};

static const uint16_t helvetica_bold_oblique_widths[256] = {
    280,  280, 280,  280, 280, 280, 280, 280,  280, 280, 280, 280,  280, 280,
    280,  280, 280,  280, 280, 280, 280, 280,  280, 280, 280, 280,  280, 280,
    280,  280, 280,  280, 280, 335, 477, 560,  560, 896, 727, 239,  335, 335,
    392,  588, 280,  335, 280, 280, 560, 560,  560, 560, 560, 560,  560, 560,
    560,  560, 335,  335, 588, 588, 588, 615,  982, 727, 727, 727,  727, 672,
    615,  784, 727,  280, 560, 727, 615, 839,  727, 784, 672, 784,  727, 672,
    615,  727, 672,  951, 672, 672, 615, 335,  280, 335, 588, 560,  335, 560,
    615,  560, 615,  560, 335, 615, 615, 280,  280, 560, 280, 896,  615, 615,
    615,  615, 392,  560, 335, 615, 560, 784,  560, 560, 504, 392,  282, 392,
    588,  352, 560,  352, 280, 560, 504, 1008, 560, 560, 335, 1008, 672, 335,
    1008, 352, 615,  352, 352, 280, 280, 504,  504, 352, 560, 1008, 335, 1008,
    560,  335, 951,  352, 504, 672, 280, 335,  560, 560, 560, 560,  282, 560,
    335,  742, 372,  560, 588, 335, 742, 335,  403, 588, 335, 335,  335, 615,
    560,  280, 335,  335, 367, 560, 840, 840,  840, 615, 727, 727,  727, 727,
    727,  727, 1008, 727, 672, 672, 672, 672,  280, 280, 280, 280,  727, 727,
    784,  784, 784,  784, 784, 588, 784, 727,  727, 727, 727, 672,  672, 615,
    560,  560, 560,  560, 560, 560, 896, 560,  560, 560, 560, 560,  280, 280,
    280,  280, 615,  615, 615, 615, 615, 615,  615, 588, 615, 615,  615, 615,
    615,  560, 615,  560,
};

static const uint16_t helvetica_oblique_widths[256] = {
    280, 280, 280, 280,  280, 280, 280, 280,  280,  280, 280,  280, 280,
    280, 280, 280, 280,  280, 280, 280, 280,  280,  280, 280,  280, 280,
    280, 280, 280, 280,  280, 280, 280, 280,  357,  560, 560,  896, 672,
    192, 335, 335, 392,  588, 280, 335, 280,  280,  560, 560,  560, 560,
    560, 560, 560, 560,  560, 560, 280, 280,  588,  588, 588,  560, 1023,
    672, 672, 727, 727,  672, 615, 784, 727,  280,  504, 672,  560, 839,
    727, 784, 672, 784,  727, 672, 615, 727,  672,  951, 672,  672, 615,
    280, 280, 280, 472,  560, 335, 560, 560,  504,  560, 560,  280, 560,
    560, 223, 223, 504,  223, 839, 560, 560,  560,  560, 335,  504, 280,
    560, 504, 727, 504,  504, 504, 336, 262,  336,  588, 352,  560, 352,
    223, 560, 335, 1008, 560, 560, 335, 1008, 672,  335, 1008, 352, 615,
    352, 352, 223, 223,  335, 335, 352, 560,  1008, 335, 1008, 504, 335,
    951, 352, 504, 672,  280, 335, 560, 560,  560,  560, 262,  560, 335,
    742, 372, 560, 588,  335, 742, 335, 403,  588,  335, 335,  335, 560,
    541, 280, 335, 335,  367, 560, 840, 840,  840,  615, 672,  672, 672,
    672, 672, 672, 1008, 727, 672, 672, 672,  672,  280, 280,  280, 280,
    727, 727, 784, 784,  784, 784, 784, 588,  784,  727, 727,  727, 727,
    672, 672, 615, 560,  560, 560, 560, 560,  560,  896, 504,  560, 560,
    560, 560, 280, 280,  280, 280, 560, 560,  560,  560, 560,  560, 560,
    588, 615, 560, 560,  560, 560, 504, 560,  504,
};

static const uint16_t symbol_widths[256] = {
    252, 252, 252, 252,  252, 252, 252,  252, 252,  252,  252, 252, 252, 252,
    252, 252, 252, 252,  252, 252, 252,  252, 252,  252,  252, 252, 252, 252,
    252, 252, 252, 252,  252, 335, 718,  504, 553,  839,  784, 442, 335, 335,
    504, 553, 252, 553,  252, 280, 504,  504, 504,  504,  504, 504, 504, 504,
    504, 504, 280, 280,  553, 553, 553,  447, 553,  727,  672, 727, 616, 615,
    769, 607, 727, 335,  636, 727, 691,  896, 727,  727,  774, 746, 560, 596,
    615, 695, 442, 774,  650, 801, 615,  335, 869,  335,  663, 504, 504, 636,
    553, 553, 497, 442,  525, 414, 607,  331, 607,  553,  553, 580, 525, 553,
    553, 525, 553, 607,  442, 580, 718,  691, 496,  691,  497, 483, 201, 483,
    553, 0,   0,   0,    0,   0,   0,    0,   0,    0,    0,   0,   0,   0,
    0,   0,   0,   0,    0,   0,   0,    0,   0,    0,    0,   0,   0,   0,
    0,   0,   0,   0,    0,   0,   756,  624, 248,  553,  168, 718, 504, 759,
    759, 759, 759, 1050, 994, 607, 994,  607, 403,  553,  414, 553, 553, 718,
    497, 463, 553, 553,  553, 553, 1008, 607, 1008, 663,  829, 691, 801, 994,
    774, 774, 829, 774,  774, 718, 718,  718, 718,  718,  718, 718, 774, 718,
    796, 796, 897, 829,  553, 252, 718,  607, 607,  1050, 994, 607, 994, 607,
    497, 331, 796, 796,  792, 718, 387,  387, 387,  387,  387, 387, 497, 497,
    497, 497, 0,   331,  276, 691, 691,  691, 387,  387,  387, 387, 387, 387,
    497, 497, 497, 0,
};

static const uint16_t times_widths[256] = {
    252, 252, 252, 252, 252, 252, 252, 252,  252, 252, 252, 252,  252, 252,
    252, 252, 252, 252, 252, 252, 252, 252,  252, 252, 252, 252,  252, 252,
    252, 252, 252, 252, 252, 335, 411, 504,  504, 839, 784, 181,  335, 335,
    504, 568, 252, 335, 252, 280, 504, 504,  504, 504, 504, 504,  504, 504,
    504, 504, 280, 280, 568, 568, 568, 447,  928, 727, 672, 672,  727, 615,
    560, 727, 727, 335, 392, 727, 615, 896,  727, 727, 560, 727,  672, 560,
    615, 727, 727, 951, 727, 727, 615, 335,  280, 335, 472, 504,  335, 447,
    504, 447, 504, 447, 335, 504, 504, 280,  280, 504, 280, 784,  504, 504,
    504, 504, 335, 392, 280, 504, 504, 727,  504, 504, 447, 483,  201, 483,
    545, 352, 504, 352, 335, 504, 447, 1008, 504, 504, 335, 1008, 560, 335,
    896, 352, 615, 352, 352, 335, 335, 447,  447, 352, 504, 1008, 335, 987,
    392, 335, 727, 352, 447, 727, 252, 335,  504, 504, 504, 504,  201, 504,
    335, 766, 278, 504, 568, 335, 766, 335,  403, 568, 302, 302,  335, 504,
    456, 252, 335, 302, 312, 504, 756, 756,  756, 447, 727, 727,  727, 727,
    727, 727, 896, 672, 615, 615, 615, 615,  335, 335, 335, 335,  727, 727,
    727, 727, 727, 727, 727, 568, 727, 727,  727, 727, 727, 727,  560, 504,
    447, 447, 447, 447, 447, 447, 672, 447,  447, 447, 447, 447,  280, 280,
    280, 280, 504, 504, 504, 504, 504, 504,  504, 568, 504, 504,  504, 504,
    504, 504, 504, 504,
};

static const uint16_t times_bold_widths[256] = {
    252, 252, 252, 252,  252, 252, 252, 252,  252,  252,  252,  252,  252,
    252, 252, 252, 252,  252, 252, 252, 252,  252,  252,  252,  252,  252,
    252, 252, 252, 252,  252, 252, 252, 335,  559,  504,  504,  1008, 839,
    280, 335, 335, 504,  574, 252, 335, 252,  280,  504,  504,  504,  504,
    504, 504, 504, 504,  504, 504, 335, 335,  574,  574,  574,  504,  937,
    727, 672, 727, 727,  672, 615, 784, 784,  392,  504,  784,  672,  951,
    727, 784, 615, 784,  727, 560, 672, 727,  727,  1008, 727,  727,  672,
    335, 280, 335, 585,  504, 335, 504, 560,  447,  560,  447,  335,  504,
    560, 280, 335, 560,  280, 839, 560, 504,  560,  560,  447,  392,  335,
    560, 504, 727, 504,  504, 447, 397, 221,  397,  524,  352,  504,  352,
    335, 504, 504, 1008, 504, 504, 335, 1008, 560,  335,  1008, 352,  672,
    352, 352, 335, 335,  504, 504, 352, 504,  1008, 335,  1008, 392,  335,
    727, 352, 447, 727,  252, 335, 504, 504,  504,  504,  221,  504,  335,
    752, 302, 504, 574,  335, 752, 335, 403,  574,  302,  302,  335,  560,
    544, 252, 335, 302,  332, 504, 756, 756,  756,  504,  727,  727,  727,
    727, 727, 727, 1008, 727, 672, 672, 672,  672,  392,  392,  392,  392,
    727, 727, 784, 784,  784, 784, 784, 574,  784,  727,  727,  727,  727,
    727, 615, 560, 504,  504, 504, 504, 504,  504,  727,  447,  447,  447,
    447, 447, 280, 280,  280, 280, 504, 560,  504,  504,  504,  504,  504,
    574, 504, 560, 560,  560, 560, 504, 560,  504,
};

static const uint16_t times_bold_italic_widths[256] = {
    252, 252, 252, 252, 252, 252, 252, 252,  252, 252, 252, 252,  252, 252,
    252, 252, 252, 252, 252, 252, 252, 252,  252, 252, 252, 252,  252, 252,
    252, 252, 252, 252, 252, 392, 559, 504,  504, 839, 784, 280,  335, 335,
    504, 574, 252, 335, 252, 280, 504, 504,  504, 504, 504, 504,  504, 504,
    504, 504, 335, 335, 574, 574, 574, 504,  838, 672, 672, 672,  727, 672,
    672, 727, 784, 392, 504, 672, 615, 896,  727, 727, 615, 727,  672, 560,
    615, 727, 672, 896, 672, 615, 615, 335,  280, 335, 574, 504,  335, 504,
    504, 447, 504, 447, 335, 504, 560, 280,  280, 504, 280, 784,  560, 504,
    504, 504, 392, 392, 280, 560, 447, 672,  504, 447, 392, 350,  221, 350,
    574, 352, 504, 352, 335, 504, 504, 1008, 504, 504, 335, 1008, 560, 335,
    951, 352, 615, 352, 352, 335, 335, 504,  504, 352, 504, 1008, 335, 1008,
    392, 335, 727, 352, 392, 615, 252, 392,  504, 504, 504, 504,  221, 504,
    335, 752, 268, 504, 610, 335, 752, 335,  403, 574, 302, 302,  335, 580,
    504, 252, 335, 302, 302, 504, 756, 756,  756, 504, 672, 672,  672, 672,
    672, 672, 951, 672, 672, 672, 672, 672,  392, 392, 392, 392,  727, 727,
    727, 727, 727, 727, 727, 574, 727, 727,  727, 727, 727, 615,  615, 504,
    504, 504, 504, 504, 504, 504, 727, 447,  447, 447, 447, 447,  280, 280,
    280, 280, 504, 560, 504, 504, 504, 504,  504, 574, 504, 560,  560, 560,
    560, 447, 504, 447,
};

static const uint16_t times_italic_widths[256] = {
    252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252,  252, 252,
    252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252, 252,  252, 252,
    252, 252, 252, 252, 252, 335, 423, 504, 504, 839, 784, 215,  335, 335,
    504, 680, 252, 335, 252, 280, 504, 504, 504, 504, 504, 504,  504, 504,
    504, 504, 335, 335, 680, 680, 680, 504, 927, 615, 615, 672,  727, 615,
    615, 727, 727, 335, 447, 672, 560, 839, 672, 727, 615, 727,  615, 504,
    560, 727, 615, 839, 615, 560, 560, 392, 280, 392, 425, 504,  335, 504,
    504, 447, 504, 447, 280, 504, 504, 280, 280, 447, 280, 727,  504, 504,
    504, 504, 392, 392, 280, 504, 447, 672, 447, 447, 392, 403,  277, 403,
    545, 352, 504, 352, 335, 504, 560, 896, 504, 504, 335, 1008, 504, 335,
    951, 352, 560, 352, 352, 335, 335, 560, 560, 352, 504, 896,  335, 987,
    392, 335, 672, 352, 392, 560, 252, 392, 504, 504, 504, 504,  277, 504,
    335, 766, 278, 504, 680, 335, 766, 335, 403, 680, 302, 302,  335, 504,
    527, 252, 335, 302, 312, 504, 756, 756, 756, 504, 615, 615,  615, 615,
    615, 615, 896, 672, 615, 615, 615, 615, 335, 335, 335, 335,  727, 672,
    727, 727, 727, 727, 727, 680, 727, 727, 727, 727, 727, 560,  615, 504,
    504, 504, 504, 504, 504, 504, 672, 447, 447, 447, 447, 447,  280, 280,
    280, 280, 504, 504, 504, 504, 504, 504, 504, 680, 504, 504,  504, 504,
    504, 447, 504, 447,
};

static const uint16_t zapfdingbats_widths[256] = {
    0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   280,  981, 968, 981, 987, 724, 795, 796, 797, 695,
    967, 946, 553, 861, 918,  940, 918, 952, 981, 761, 852, 768, 767, 575,
    682, 769, 766, 765, 760,  497, 556, 541, 581, 697, 792, 794, 794, 796,
    799, 800, 822, 829, 795,  847, 829, 839, 822, 837, 930, 749, 728, 754,
    796, 798, 700, 782, 774,  798, 765, 712, 713, 687, 706, 832, 821, 795,
    795, 712, 692, 701, 694,  792, 793, 718, 797, 791, 797, 879, 767, 768,
    768, 765, 765, 899, 899,  794, 790, 441, 139, 279, 418, 395, 395, 673,
    673, 0,   393, 393, 319,  319, 278, 278, 513, 513, 413, 413, 235, 235,
    336, 336, 0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,    0,   0,   737, 548, 548, 917, 672, 766, 766,
    782, 599, 699, 631, 794,  794, 794, 794, 794, 794, 794, 794, 794, 794,
    794, 794, 794, 794, 794,  794, 794, 794, 794, 794, 794, 794, 794, 794,
    794, 794, 794, 794, 794,  794, 794, 794, 794, 794, 794, 794, 794, 794,
    794, 794, 901, 844, 1024, 461, 753, 931, 753, 925, 934, 935, 935, 840,
    879, 834, 931, 931, 924,  937, 938, 466, 890, 842, 842, 873, 873, 701,
    701, 880, 0,   880, 766,  953, 777, 871, 777, 895, 974, 895, 837, 879,
    934, 977, 925, 0,
};

static const uint16_t courier_widths[256] = {
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604, 604,
    604,
};

static int pdf_text_pixel_width(struct pdf_doc *pdf, const char *text,
                                ptrdiff_t text_len, int size,
                                const uint16_t *widths)
{
    unsigned int len = 0;
    if (text_len < 0)
        text_len = strlen(text);

    for (int i = 0; i < (int)text_len;) {
        uint32_t code;
        int code_len;
        code_len = utf8_to_utf32(&text[i], text_len - i, &code);
        if (code_len < 0)
            return pdf_set_err(pdf, code_len,
                               "Invalid unicode string at position %d in %s",
                               i, text);
        if (code >= 255)
            return pdf_set_err(
                pdf, code_len,
                "Unable to determine width of character code %d", code);
        i += code_len;

        if (code != '\n' && code != '\r')
            len += widths[(uint8_t)code];
    }

    /* Our widths arrays are for 14pt fonts */
    return len * size / (14 * 72);
}

static const uint16_t *find_font_widths(const char *font_name)
{
    if (strcasecmp(font_name, "Helvetica") == 0)
        return helvetica_widths;
    if (strcasecmp(font_name, "Helvetica-Bold") == 0)
        return helvetica_bold_widths;
    if (strcasecmp(font_name, "Helvetica-BoldOblique") == 0)
        return helvetica_bold_oblique_widths;
    if (strcasecmp(font_name, "Helvetica-Oblique") == 0)
        return helvetica_oblique_widths;
    if (strcasecmp(font_name, "Courier") == 0 ||
        strcasecmp(font_name, "Courier-Bold") == 0 ||
        strcasecmp(font_name, "Courier-BoldOblique") == 0 ||
        strcasecmp(font_name, "Courier-Oblique") == 0)
        return courier_widths;
    if (strcasecmp(font_name, "Times-Roman") == 0)
        return times_widths;
    if (strcasecmp(font_name, "Times-Bold") == 0)
        return times_bold_widths;
    if (strcasecmp(font_name, "Times-Italic") == 0)
        return times_italic_widths;
    if (strcasecmp(font_name, "Times-BoldItalic") == 0)
        return times_bold_italic_widths;
    if (strcasecmp(font_name, "Symbol") == 0)
        return symbol_widths;
    if (strcasecmp(font_name, "ZapfDingbats") == 0)
        return zapfdingbats_widths;

    return NULL;
}

int pdf_get_font_text_width(struct pdf_doc *pdf, const char *font_name,
                            const char *text, int size)
{
    const uint16_t *widths = find_font_widths(font_name);

    if (!widths)
        return pdf_set_err(pdf, -EINVAL,
                           "Unable to determine width for font '%s'",
                           pdf->current_font->font.name);
    return pdf_text_pixel_width(pdf, text, -1, size, widths);
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
                      uint32_t colour, int wrap_width, int align)
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
        return pdf_set_err(pdf, -EINVAL,
                           "Unable to determine width for font '%s'",
                           pdf->current_font->font.name);

    while (start && *start) {
        const char *new_end = find_word_break(end + 1);
        int line_width;
        int output = 0;
        int xoff_align = xoff;

        end = new_end;

        line_width =
            pdf_text_pixel_width(pdf, start, end - start, size, widths);
        if (line_width < 0)
            return line_width;

        if (line_width >= wrap_width) {
            if (last_best == start) {
                /* There is a single word that is too long for the line */
                ptrdiff_t i;
                /* Find the best character to chop it at */
                for (i = end - start - 1; i > 0; i--) {
                    int e = pdf_text_pixel_width(pdf, start, i, size, widths);
                    if (e < 0)
                        return e;
                    if (e < wrap_width)
                        break;
                }

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
            double char_spacing = 0;
            if (len >= sizeof(line))
                len = sizeof(line) - 1;
            strncpy(line, start, len);
            line[len] = '\0';

            line_width = pdf_text_pixel_width(pdf, start, len, size, widths);
            if (line_width < 0)
                return line_width;

            switch (align) {
            case PDF_ALIGN_RIGHT:
                xoff_align += wrap_width - line_width;
                break;
            case PDF_ALIGN_CENTER:
                xoff_align += (wrap_width - line_width) / 2;
                break;
            case PDF_ALIGN_JUSTIFY:
                if ((len - 1) > 0 && *end != '\r' && *end != '\n' &&
                    *end != '\0')
                    char_spacing =
                        ((double)(wrap_width - line_width)) / (len - 2);
                break;
            case PDF_ALIGN_JUSTIFY_ALL:
                if ((len - 1) > 0)
                    char_spacing =
                        ((double)(wrap_width - line_width)) / (len - 2);
                break;
            }

            if (align != PDF_ALIGN_NO_WRITE) {
                pdf_add_text_spacing(pdf, page, line, size, xoff_align, yoff,
                                     colour, char_spacing);
            }

            if (*end == ' ')
                end++;

            start = last_best = end;
            yoff -= size;
        } else
            last_best = end;
    }

    return orig_yoff - yoff;
}

int pdf_add_line(struct pdf_doc *pdf, struct pdf_object *page, int x1, int y1,
                 int x2, int y2, int width, uint32_t colour)
{
    int ret;
    struct dstr str = INIT_DSTR;

    dstr_printf(&str, "%d w\r\n", width);
    dstr_printf(&str, "%d %d m\r\n", x1, y1);
    dstr_printf(&str, "/DeviceRGB CS\r\n");
    dstr_printf(&str, "%f %f %f RG\r\n", PDF_RGB_R(colour), PDF_RGB_G(colour),
                PDF_RGB_B(colour));
    dstr_printf(&str, "%d %d l S\r\n", x2, y2);

    ret = pdf_add_stream(pdf, page, dstr_data(&str));
    dstr_free(&str);

    return ret;
}

int pdf_add_ellipse(struct pdf_doc *pdf, struct pdf_object *page, int xr,
                    int yr, int xradius, int yradius, int width,
                    uint32_t colour, uint32_t fill_colour)
{
    int ret;
    struct dstr str = INIT_DSTR;

    float rx = xradius * 1.0f;
    float ry = yradius * 1.0f;
    float lx, ly;
    float x = xr * 1.0f;
    float y = yr * 1.0f;

    lx = (4.0f / 3.0f) * (M_SQRT2 - 1) * rx;
    ly = (4.0f / 3.0f) * (M_SQRT2 - 1) * ry;

    if (!PDF_IS_TRANSPARENT(fill_colour)) {
        dstr_printf(&str, "/DeviceRGB CS\r\n");
        dstr_printf(&str, "%f %f %f rg\r\n", PDF_RGB_R(fill_colour),
                    PDF_RGB_G(fill_colour), PDF_RGB_B(fill_colour));
    }

    /* stroke color */
    dstr_printf(&str, "/DeviceRGB CS\r\n");
    dstr_printf(&str, "%f %f %f RG\r\n", PDF_RGB_R(colour), PDF_RGB_G(colour),
                PDF_RGB_B(colour));

    dstr_printf(&str, "%d w ", width);

    dstr_printf(&str, "%.2f %.2f m ", (x + rx), (y));

    dstr_printf(&str, "%.2f %.2f %.2f %.2f %.2f %.2f c ", (x + rx), (y - ly),
                (x + lx), (y - ry), x, (y - ry));

    dstr_printf(&str, "%.2f %.2f %.2f %.2f %.2f %.2f c ", (x - lx), (y - ry),
                (x - rx), (y - ly), (x - rx), y);

    dstr_printf(&str, "%.2f %.2f %.2f %.2f %.2f %.2f c ", (x - rx), (y + ly),
                (x - lx), (y + ry), x, (y + ry));

    dstr_printf(&str, "%.2f %.2f %.2f %.2f %.2f %.2f c ", (x + lx), (y + ry),
                (x + rx), (y + ly), (x + rx), y);

    if (PDF_IS_TRANSPARENT(fill_colour))
        dstr_printf(&str, "%s", "S ");
    else
        dstr_printf(&str, "%s", "B ");

    ret = pdf_add_stream(pdf, page, dstr_data(&str));
    dstr_free(&str);

    return ret;
}

int pdf_add_circle(struct pdf_doc *pdf, struct pdf_object *page, int xr,
                   int yr, int radius, int width, uint32_t colour,
                   uint32_t fill_colour)
{
    return pdf_add_ellipse(pdf, page, xr, yr, radius, radius, width, colour,
                           fill_colour);
}

int pdf_add_rectangle(struct pdf_doc *pdf, struct pdf_object *page, int x,
                      int y, int width, int height, int border_width,
                      uint32_t colour)
{
    int ret;
    struct dstr str = INIT_DSTR;

    dstr_printf(&str, "%f %f %f RG ", PDF_RGB_R(colour), PDF_RGB_G(colour),
                PDF_RGB_B(colour));
    dstr_printf(&str, "%d w ", border_width);
    dstr_printf(&str, "%d %d %d %d re S ", x, y, width, height);

    ret = pdf_add_stream(pdf, page, dstr_data(&str));
    dstr_free(&str);

    return ret;
}

int pdf_add_filled_rectangle(struct pdf_doc *pdf, struct pdf_object *page,
                             int x, int y, int width, int height,
                             int border_width, uint32_t colour)
{
    int ret;
    struct dstr str = INIT_DSTR;

    dstr_printf(&str, "%f %f %f rg ", PDF_RGB_R(colour), PDF_RGB_G(colour),
                PDF_RGB_B(colour));
    dstr_printf(&str, "%d w ", border_width);
    dstr_printf(&str, "%d %d %d %d re f ", x, y, width, height);

    ret = pdf_add_stream(pdf, page, dstr_data(&str));
    dstr_free(&str);

    return ret;
}

int pdf_add_polygon(struct pdf_doc *pdf, struct pdf_object *page, int x[],
                    int y[], int count, int border_width, uint32_t colour)
{
    int ret;
    struct dstr str = INIT_DSTR;

    dstr_printf(&str, "%f %f %f RG ", PDF_RGB_R(colour), PDF_RGB_G(colour),
                PDF_RGB_B(colour));
    dstr_printf(&str, "%d w ", border_width);
    dstr_printf(&str, "%d %d m ", x[0], y[0]);
    for (int i = 1; i < count; i++) {
        dstr_printf(&str, "%d %d l ", x[i], y[i]);
    }
    dstr_printf(&str, "h S ");

    ret = pdf_add_stream(pdf, page, dstr_data(&str));
    dstr_free(&str);

    return ret;
}

int pdf_add_filled_polygon(struct pdf_doc *pdf, struct pdf_object *page,
                           int x[], int y[], int count, int border_width,
                           uint32_t colour)
{
    int ret;
    struct dstr str = INIT_DSTR;

    dstr_printf(&str, "%f %f %f RG ", PDF_RGB_R(colour), PDF_RGB_G(colour),
                PDF_RGB_B(colour));
    dstr_printf(&str, "%f %f %f rg ", PDF_RGB_R(colour), PDF_RGB_G(colour),
                PDF_RGB_B(colour));
    dstr_printf(&str, "%d w ", border_width);
    dstr_printf(&str, "%d %d m ", x[0], y[0]);
    for (int i = 1; i < count; i++) {
        dstr_printf(&str, "%d %d l ", x[i], y[i]);
    }
    dstr_printf(&str, "h f ");

    ret = pdf_add_stream(pdf, page, dstr_data(&str));
    dstr_free(&str);

    return ret;
}

static const struct {
    uint32_t code;
    char ch;
} code_128a_encoding[] = {
    {0x212222, ' '},  {0x222122, '!'},  {0x222221, '"'},   {0x121223, '#'},
    {0x121322, '$'},  {0x131222, '%'},  {0x122213, '&'},   {0x122312, '\''},
    {0x132212, '('},  {0x221213, ')'},  {0x221312, '*'},   {0x231212, '+'},
    {0x112232, ','},  {0x122132, '-'},  {0x122231, '.'},   {0x113222, '/'},
    {0x123122, '0'},  {0x123221, '1'},  {0x223211, '2'},   {0x221132, '3'},
    {0x221231, '4'},  {0x213212, '5'},  {0x223112, '6'},   {0x312131, '7'},
    {0x311222, '8'},  {0x321122, '9'},  {0x321221, ':'},   {0x312212, ';'},
    {0x322112, '<'},  {0x322211, '='},  {0x212123, '>'},   {0x212321, '?'},
    {0x232121, '@'},  {0x111323, 'A'},  {0x131123, 'B'},   {0x131321, 'C'},
    {0x112313, 'D'},  {0x132113, 'E'},  {0x132311, 'F'},   {0x211313, 'G'},
    {0x231113, 'H'},  {0x231311, 'I'},  {0x112133, 'J'},   {0x112331, 'K'},
    {0x132131, 'L'},  {0x113123, 'M'},  {0x113321, 'N'},   {0x133121, 'O'},
    {0x313121, 'P'},  {0x211331, 'Q'},  {0x231131, 'R'},   {0x213113, 'S'},
    {0x213311, 'T'},  {0x213131, 'U'},  {0x311123, 'V'},   {0x311321, 'W'},
    {0x331121, 'X'},  {0x312113, 'Y'},  {0x312311, 'Z'},   {0x332111, '['},
    {0x314111, '\\'}, {0x221411, ']'},  {0x431111, '^'},   {0x111224, '_'},
    {0x111422, '`'},  {0x121124, 'a'},  {0x121421, 'b'},   {0x141122, 'c'},
    {0x141221, 'd'},  {0x112214, 'e'},  {0x112412, 'f'},   {0x122114, 'g'},
    {0x122411, 'h'},  {0x142112, 'i'},  {0x142211, 'j'},   {0x241211, 'k'},
    {0x221114, 'l'},  {0x413111, 'm'},  {0x241112, 'n'},   {0x134111, 'o'},
    {0x111242, 'p'},  {0x121142, 'q'},  {0x121241, 'r'},   {0x114212, 's'},
    {0x124112, 't'},  {0x124211, 'u'},  {0x411212, 'v'},   {0x421112, 'w'},
    {0x421211, 'x'},  {0x212141, 'y'},  {0x214121, 'z'},   {0x412121, '{'},
    {0x111143, '|'},  {0x111341, '}'},  {0x131141, '~'},   {0x114113, '\0'},
    {0x114311, '\0'}, {0x411113, '\0'}, {0x411311, '\0'},  {0x113141, '\0'},
    {0x114131, '\0'}, {0x311141, '\0'}, {0x411131, '\0'},  {0x211412, '\0'},
    {0x211214, '\0'}, {0x211232, '\0'}, {0x2331112, '\0'},
};

static int find_128_encoding(char ch)
{
    for (int i = 0; i < ARRAY_SIZE(code_128a_encoding); i++) {
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
    int line_width = width / 11;

    for (int i = 0; i < code_len; i++) {
        uint8_t shift = (code_len - 1 - i) * 4;
        uint8_t mask = (code >> shift) & 0xf;

        if (!(i % 2))
            pdf_add_filled_rectangle(pdf, page, x, y, line_width * mask,
                                     height, 0, colour);
        x += line_width * mask;
    }
    return x;
}

static int pdf_add_barcode_128a(struct pdf_doc *pdf, struct pdf_object *page,
                                int x, int y, int width, int height,
                                const char *string, uint32_t colour)
{
    const char *s;
    size_t len = strlen(string) + 3;
    int char_width = width / len;
    int checksum, i;

    if (char_width / 11 <= 0)
        return pdf_set_err(pdf, -EINVAL,
                           "Insufficient width to draw barcode");

    for (s = string; *s; s++)
        if (find_128_encoding(*s) < 0)
            return pdf_set_err(pdf, -EINVAL, "Invalid barcode character 0x%x",
                               *s);

    x = pdf_barcode_128a_ch(pdf, page, x, y, char_width, height, colour, 104,
                            6);
    checksum = 104;

    for (i = 1, s = string; *s; s++, i++) {
        int index = find_128_encoding(*s);
        x = pdf_barcode_128a_ch(pdf, page, x, y, char_width, height, colour,
                                index, 6);
        checksum += index * i;
    }
    x = pdf_barcode_128a_ch(pdf, page, x, y, char_width, height, colour,
                            checksum % 103, 6);
    pdf_barcode_128a_ch(pdf, page, x, y, char_width, height, colour, 106, 7);
    return 0;
}

/* Code 39 character encoding. Each 4-bit value indicates:
 * 0 => wide bar
 * 1 => narrow bar
 * 2 => wide space
 */
static const struct {
    uint32_t code;
    char ch;
} code_39_encoding[] = {
    {0x012110, '1'}, {0x102110, '2'}, {0x002111, '3'},
    {0x112010, '4'}, {0x012011, '5'}, {0x102011, '6'},
    {0x112100, '7'}, {0x012101, '8'}, {0x102101, '9'},
    {0x112001, '0'}, {0x011210, 'A'}, {0x101210, 'B'},
    {0x001211, 'C'}, {0x110210, 'D'}, {0x010211, 'E'},
    {0x100211, 'F'}, {0x111200, 'G'}, {0x011201, 'H'},
    {0x101201, 'I'}, {0x110201, 'J'}, {0x011120, 'K'},
    {0x101120, 'L'}, {0x001121, 'M'}, {0x110120, 'N'},
    {0x010121, 'O'}, {0x100121, 'P'}, {0x111020, 'Q'},
    {0x011021, 'R'}, {0x101021, 'S'}, {0x110021, 'T'},
    {0x021110, 'U'}, {0x120110, 'V'}, {0x020111, 'W'},
    {0x121010, 'X'}, {0x021011, 'Y'}, {0x120011, 'Z'},
    {0x121100, '-'}, {0x021101, '.'}, {0x120101, ' '},
    {0x121001, '*'}, // 'stop' character
};

static int pdf_barcode_39_ch(struct pdf_doc *pdf, struct pdf_object *page,
                             int x, int y, int char_width, int height,
                             uint32_t colour, char ch)
{
    int nw = char_width / 12;
    int ww = char_width / 4;
    int i;
    uint32_t code;

    if (nw <= 1 || ww <= 1)
        return pdf_set_err(pdf, -EINVAL,
                           "Insufficient width for each character");

    for (i = 0; i < ARRAY_SIZE(code_39_encoding); i++) {
        if (code_39_encoding[i].ch == ch) {
            code = code_39_encoding[i].code;
            break;
        }
    }
    if (i == ARRAY_SIZE(code_39_encoding))
        return pdf_set_err(pdf, -EINVAL, "Invalid Code 39 character %c 0x%x",
                           ch, ch);

    for (i = 5; i >= 0; i--) {
        int pattern = (code >> i * 4) & 0xf;
        if (pattern == 0) { // wide
            if (pdf_add_filled_rectangle(pdf, page, x, y, ww - 1, height, 0,
                                         colour) < 0)
                return pdf->errval;
            x += ww;
        }
        if (pattern == 1) { // narrow
            if (pdf_add_filled_rectangle(pdf, page, x, y, nw - 1, height, 0,
                                         colour) < 0)
                return pdf->errval;
            x += nw;
        }
        if (pattern == 2) { // space
            x += nw;
        }
    }
    return x;
}

static int pdf_add_barcode_39(struct pdf_doc *pdf, struct pdf_object *page,
                              int x, int y, int width, int height,
                              const char *string, uint32_t colour)
{
    size_t len = strlen(string);
    int char_width = width / (len + 2);

    x = pdf_barcode_39_ch(pdf, page, x, y, char_width, height, colour, '*');
    if (x < 0)
        return x;

    while (string && *string) {
        x = pdf_barcode_39_ch(pdf, page, x, y, char_width, height, colour,
                              *string);
        if (x < 0)
            return x;
        string++;
    }

    x = pdf_barcode_39_ch(pdf, page, x, y, char_width, height, colour, '*');
    if (x < 0)
        return x;

    return 0;
}

int pdf_add_barcode(struct pdf_doc *pdf, struct pdf_object *page, int code,
                    int x, int y, int width, int height, const char *string,
                    uint32_t colour)
{
    if (!string || !*string)
        return 0;
    switch (code) {
    case PDF_BARCODE_128A:
        return pdf_add_barcode_128a(pdf, page, x, y, width, height, string,
                                    colour);
    case PDF_BARCODE_39:
        return pdf_add_barcode_39(pdf, page, x, y, width, height, string,
                                  colour);
    default:
        return pdf_set_err(pdf, -EINVAL, "Invalid barcode code %d", code);
    }
}

static pdf_object *pdf_add_raw_rgb24(struct pdf_doc *pdf, const uint8_t *data,
                                     unsigned width, unsigned height)
{
    struct pdf_object *obj;
    size_t len;
    const char *endstream = ">\r\nendstream\r\n";
    struct dstr str = INIT_DSTR;

    dstr_printf(
        &str,
        "<<\r\n/Type /XObject\r\n/Name /Image%d\r\n/Subtype /Image\r\n"
        "/ColorSpace /DeviceRGB\r\n/Height %d\r\n/Width %d\r\n"
        "/BitsPerComponent 8\r\n/Filter /ASCIIHexDecode\r\n"
        "/Length %d\r\n>>stream\r\n",
        flexarray_size(&pdf->objects), height, width,
        width * height * 3 * 2 + 1);

    len = dstr_len(&str) + (size_t)width * (size_t)height * 3 * 2 +
          strlen(endstream) + 1;
    if (dstr_ensure(&str, len) < 0) {
        dstr_free(&str);
        pdf_set_err(pdf, -ENOMEM,
                    "Unable to allocate %zd bytes memory for image", len);
        return NULL;
    }
    for (int i = 0; i < width * height * 3; i++) {
        char buf[3] = {"0123456789ABCDEF"[(data[i] >> 4) & 0xf],
                       "0123456789ABCDEF"[data[i] & 0xf], 0};
        dstr_append(&str, buf);
    }
    dstr_append(&str, endstream);

    obj = pdf_add_object(pdf, OBJ_image);
    if (!obj) {
        dstr_free(&str);
        return NULL;
    }
    obj->stream = str;

    return obj;
}

/* See http://www.64lines.com/jpeg-width-height for details */
static int jpeg_size(const unsigned char *data, size_t data_size, int *width,
                     int *height)
{
    size_t i = 0;
    if (i + 3 < data_size && data[i] == 0xFF && data[i + 1] == 0xD8 &&
        data[i + 2] == 0xFF && data[i + 3] == 0xE0) {
        i += 4;
        if (i + 6 < data_size && data[i + 2] == 'J' && data[i + 3] == 'F' &&
            data[i + 4] == 'I' && data[i + 5] == 'F' && data[i + 6] == 0x00) {
            unsigned short block_length = data[i] * 256 + data[i + 1];
            while (i < data_size) {
                i += block_length;
                if ((i + 8) >= data_size)
                    return -1;
                if (data[i] != 0xFF)
                    return -1;
                if (data[i + 1] == 0xC0) {
                    *height = data[i + 5] * 256 + data[i + 6];
                    *width = data[i + 7] * 256 + data[i + 8];
                    return 0;
                }
                i += 2;
                block_length = data[i] * 256 + data[i + 1];
            }
        }
    }

    return -1;
}

static pdf_object *pdf_add_raw_jpeg_data(struct pdf_doc *pdf,
                                         const unsigned char *jpeg_data,
                                         size_t len)
{
    struct pdf_object *obj;
    int width, height;

    if (jpeg_size(jpeg_data, len, &width, &height) < 0) {
        pdf_set_err(pdf, -EINVAL, "Unable to determine jpeg width/height");
        return NULL;
    }

    obj = pdf_add_object(pdf, OBJ_image);
    if (!obj)
        return NULL;
    dstr_printf(&obj->stream,
                "<<\r\n/Type /XObject\r\n/Name /Image%d\r\n"
                "/Subtype /Image\r\n/ColorSpace /DeviceRGB\r\n"
                "/Width %d\r\n/Height %d\r\n"
                "/BitsPerComponent 8\r\n/Filter /DCTDecode\r\n"
                "/Length %d\r\n>>stream\r\n",
                flexarray_size(&pdf->objects), width, height, (int)len);
    dstr_append_data(&obj->stream, jpeg_data, len);

    dstr_printf(&obj->stream, "\r\nendstream\r\n");

    return obj;
}

static pdf_object *pdf_add_raw_jpeg(struct pdf_doc *pdf,
                                    const char *jpeg_file)
{
    struct stat buf;
    size_t len;
    uint8_t *jpeg_data;
    FILE *fp;
    struct pdf_object *obj;

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
        pdf_set_err(pdf, -errno, "Unable to allocate: %zd", len);
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

    obj = pdf_add_raw_jpeg_data(pdf, jpeg_data, len);
    if (obj == NULL) {
        free(jpeg_data);
        pdf_set_err(pdf, -EINVAL, "Unable to add jpeg data from %s",
                    jpeg_file);
        return NULL;
    }

    free(jpeg_data);

    return obj;
}

static int pdf_add_image(struct pdf_doc *pdf, struct pdf_object *page,
                         struct pdf_object *image, int x, int y, int width,
                         int height)
{
    int ret;
    struct dstr str = INIT_DSTR;

    dstr_append(&str, "q ");
    dstr_printf(&str, "%d 0 0 %d %d %d cm ", width, height, x, y);
    dstr_printf(&str, "/Image%d Do ", image->index);
    dstr_append(&str, "Q");

    ret = pdf_add_stream(pdf, page, dstr_data(&str));
    dstr_free(&str);
    return ret;
}

int pdf_add_ppm(struct pdf_doc *pdf, struct pdf_object *page, int x, int y,
                int display_width, int display_height, const char *ppm_file)
{
    struct pdf_object *obj;
    uint8_t *data;
    FILE *fp;
    char line[1024];
    unsigned width, height, size;

    /* Load the PPM file */
    fp = fopen(ppm_file, "rb");
    if (!fp)
        return pdf_set_err(pdf, -errno, "Unable to open '%s'", ppm_file);
    if (!fgets(line, sizeof(line) - 1, fp)) {
        fclose(fp);
        return pdf_set_err(pdf, -EINVAL, "Invalid PPM file");
    }

    /* We only support binary ppms */
    if (strncmp(line, "P6", 2) != 0) {
        fclose(fp);
        return pdf_set_err(pdf, -EINVAL, "Only binary PPM files supported");
    }

    /* Skip the header comments until we get to the dimensions info */
    do {
        if (!fgets(line, sizeof(line) - 1, fp)) {
            fclose(fp);
            return pdf_set_err(pdf, -EINVAL, "Unable to find PPM size");
        }
        if (line[0] != '#')
            break;
    } while (1);

    if (sscanf(line, "%u %u\n", &width, &height) != 2) {
        fclose(fp);
        return pdf_set_err(pdf, -EINVAL, "Unable to find PPM size");
    }

    /* Skip over the byte-size line */
    if (!fgets(line, sizeof(line) - 1, fp)) {
        fclose(fp);
        return pdf_set_err(pdf, -EINVAL, "No byte-size line in PPM file");
    }

    /* Try and limit the memory usage to sane images */
    if (width > 4096 || height > 4096) {
        fclose(fp);
        return pdf_set_err(pdf, -EINVAL,
                           "Invalid width/height in PPM file: %ux%u", width,
                           height);
    }

    size = width * height * 3;
    data = malloc(size);
    if (!data) {
        fclose(fp);
        return pdf_set_err(pdf, -ENOMEM,
                           "Unable to allocate memory for RGB data");
    }
    if (fread(data, 1, size, fp) != size) {
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

int pdf_add_jpeg(struct pdf_doc *pdf, struct pdf_object *page, int x, int y,
                 int display_width, int display_height, const char *jpeg_file)
{
    struct pdf_object *obj;

    obj = pdf_add_raw_jpeg(pdf, jpeg_file);
    if (!obj)
        return pdf->errval;

    return pdf_add_image(pdf, page, obj, x, y, display_width, display_height);
}

int pdf_add_jpeg_data(struct pdf_doc *pdf, struct pdf_object *page, int x,
                      int y, int display_width, int display_height,
                      const unsigned char *jpeg_data, size_t len)
{
    struct pdf_object *obj;

    obj = pdf_add_raw_jpeg_data(pdf, jpeg_data, len);
    if (!obj)
        return pdf->errval;

    return pdf_add_image(pdf, page, obj, x, y, display_width, display_height);
}
