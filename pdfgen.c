/**
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <errno.h>

#include "pdfgen.h"

typedef struct pdf_object pdf_object;

enum {
    OBJ_none, // skipped
    OBJ_info,
    OBJ_stream,
    OBJ_font,
    OBJ_page,
    OBJ_bookmark,
    OBJ_outline,
    OBJ_catalog,
    OBJ_pages,
};

struct pdf_object {
    int type; /* See OBJ_xxxx */
    int index; /* PDF output index */
    int offset; /* Byte position within the output file */
    union {
        struct {
            struct pdf_object *page;
            char name[64];
        } bookmark;
        struct {
            char *text;
        } stream;
        struct {
            int nchildren;
            struct pdf_object **children;
        } page;
        struct pdf_info info;
        struct {
            char name[64];
        } font;
    };
};

struct pdf_doc {
    char errstr[128];
    int errval;
    struct pdf_object **objects;
    int nobjects;

    int width;
    int height;

    struct pdf_object *last_page;

};

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

static struct pdf_object *pdf_add_object(struct pdf_doc *pdf, int type)
{
    void *new_objs;
    struct pdf_object *obj;

    new_objs = realloc(pdf->objects,
            (pdf->nobjects + 1) * sizeof(struct pdf_object *));
    if (!new_objs) {
        pdf_set_err(pdf, -errno, "Unable to allocate object array %d: %s\n",
                pdf->nobjects + 1, strerror(errno));
        return NULL;
    }
    pdf->objects = new_objs;
    pdf->nobjects++;

    obj = calloc(sizeof(struct pdf_object), 1);
    if (!obj) {
        pdf->nobjects--;
        pdf_set_err(pdf, -errno, "Unable to allocate object %d: %s\n",
                pdf->nobjects + 1, strerror(errno));
        return NULL;
    }

    memset(obj, 0, sizeof(*obj));
    obj->type = type;
    obj->index = pdf->nobjects - 1;
    pdf->objects[pdf->nobjects - 1] = obj;

    //printf("Adding object %d [%d] %p\n", type,
    //pdf->nobjects, pdf->objects[pdf->nobjects - 1]);


    return obj;
}

struct pdf_doc *pdf_create(int width, int height, struct pdf_info *info)
{
    struct pdf_doc *pdf;
    struct pdf_object *obj;

    pdf = calloc(sizeof(struct pdf_doc), 1);
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
    obj = pdf_add_object(pdf, OBJ_font);
    sprintf(obj->font.name, "Times-Roman");

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

static inline void pdf_object_destroy(struct pdf_object *object)
{
    /* FIXME: Do something */
    free(object);
}

void pdf_destroy(struct pdf_doc *pdf)
{
    int i;
    if (pdf) {
        for (i = 0; i < pdf->nobjects; i++)
            pdf_object_destroy(pdf->objects[i]);
        free(pdf->objects);
        free(pdf);
    }
}

static struct pdf_object *pdf_find_object_before(struct pdf_doc *pdf,
        int index, int type)
{
    int i;
    for (i = index - 1; i >= 0; i--) {
        struct pdf_object *obj = pdf->objects[i];
        if (obj->type == type)
            return obj;
    }
    return NULL;
}

static struct pdf_object *pdf_find_object_after(struct pdf_doc *pdf,
        int index, int type)
{
    int i;
    for (i = index + 1; i < pdf->nobjects; i++) {
        struct pdf_object *obj = pdf->objects[i];
        if (obj->type == type)
            return obj;
    }
    return NULL;
}

static inline struct pdf_object *pdf_find_first_object(struct pdf_doc *pdf,
        int type)
{
    return pdf_find_object_after(pdf, -1, type);
}

static inline struct pdf_object *pdf_find_last_object(struct pdf_doc *pdf,
        int type)
{
    return pdf_find_object_before(pdf, pdf->nobjects, type);
}

static int pdf_object_type_count(struct pdf_doc *pdf, int type)
{
    int i, count = 0;
    for (i = 0; i < pdf->nobjects; i++) {
        if (pdf->objects[i]->type == type)
            count++;
    }
    return count;
}

int pdf_set_font(struct pdf_doc *pdf, const char *font)
{
    struct pdf_object *obj;
    /* FIXME: We're only allowing one font in the entire document at this stage */

    obj = pdf_find_first_object(pdf, OBJ_font);
    if (!obj)
        obj = pdf_add_object(pdf, OBJ_font);
    if (!obj)
        return pdf->errval;
    strncpy(obj->font.name, font, sizeof(obj->font.name));
    obj->font.name[sizeof(obj->font.name) - 1] = '\0';

    return 0;
}

struct pdf_object *pdf_append_page(struct pdf_doc *pdf)
{
    struct pdf_object *page;

    page = pdf_add_object(pdf, OBJ_page);

    if (!page)
        return NULL;

    pdf->last_page = page;

    return page;
}

static int pdf_save_object(struct pdf_doc *pdf, FILE *fp, int index)
{
    struct pdf_object *object = pdf->objects[index];

    if (object->type == OBJ_none)
        return -ENOENT;

    object->offset = ftell(fp);

    fprintf(fp, "%d 0 obj\r\n", index);

    switch (object->type) {
        case OBJ_stream:
            fwrite(object->stream.text, strlen(object->stream.text), 1, fp);
            break;
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
            struct pdf_object *font = pdf_find_first_object(pdf, OBJ_font);
            struct pdf_object *pages = pdf_find_first_object(pdf, OBJ_pages);

            fprintf(fp, "<<\r\n"
                    "/Type /Page\r\n"
                    "/Parent %d 0 R\r\n", pages->index);
            fprintf(fp, "/Resources <<\r\n"
                    "  /Font <<\r\n"
                    "    /F1 %d 0 R\r\n"
                    "  >>\r\n"
                    ">>\r\n", font->index);
            fprintf(fp, "/Contents [ ");
            for (i = 0; i < object->page.nchildren; i++)
                fprintf(fp, "%d 0 R ", object->page.children[i]->index);
            fprintf(fp, "]\r\n");
            fprintf(fp, ">>\r\n");
            break;
        }

        case OBJ_bookmark: {
            struct pdf_object *other;

            other = pdf_find_first_object(pdf, OBJ_catalog);
            fprintf(fp, "<<\r\n"
                    "/A << /Type /Action\r\n"
                    "      /S /GoTo\r\n"
                    "      /D [%d 0 R /XYZ 0 %d null]\r\n"
                    "   >>\r\n"
                    "/Parent %d 0 R\r\n"
                    "/Title (%s)\r\n",
                    object->bookmark.page->index,
                    pdf->height,
                    other->index, // they're all top level
                    object->bookmark.name);
            other = pdf_find_object_before(pdf, index, OBJ_bookmark);
            if (other)
                fprintf(fp, "/Prev %d 0 R\r\n", other->index);
            other = pdf_find_object_after(pdf, index, OBJ_bookmark);
            if (other)
                fprintf(fp, "/Next %d 0 R\r\n", other->index);
            fprintf(fp, ">>\r\n");
            break;
        }

        case OBJ_outline: {
            int count = pdf_object_type_count(pdf, OBJ_bookmark);

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
            page = pdf_find_first_object(pdf, OBJ_page);
            while (page) {
                npages++;
                fprintf(fp, "%d 0 R ", page->index);
                page = pdf_find_object_after(pdf, page->index, OBJ_page);
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
            return pdf_set_err(pdf, -EINVAL, "Invalid PDF object type %d\n",
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

    if (pdf_find_first_object(pdf, OBJ_bookmark) &&
        !pdf_find_first_object(pdf, OBJ_outline))
        pdf_add_object(pdf, OBJ_outline);

    if ((fp = fopen(filename, "wb")) == NULL)
        return pdf_set_err(pdf, -errno, "Unable to open '%s': %s\n",
                filename, strerror(errno));

    fprintf(fp, "%%PDF-1.2\r\n");
    /* Hibit bytes */
    fprintf(fp, "%c%c%c%c%c\r\n", 0x25, 0xc7, 0xec, 0x8f, 0xa2);

    /* Dump all the objects & get their file offsets */
    for (i = 0; i < pdf->nobjects; i++)
        if (pdf_save_object(pdf, fp, i) >= 0)
            xref_count++;

    /* xref */
    xref_offset = ftell(fp);
    fprintf(fp, "xref\r\n");
    fprintf(fp, "0 %d\r\n", xref_count + 1);
    fprintf(fp, "0000000000 65535 f\r\n");
    for (i = 0; i < pdf->nobjects; i++) {
        obj = pdf->objects[i];
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
        page = pdf->last_page;

    if (!page)
        return pdf_set_err(pdf, -EINVAL, "Invalid pdf page\n");

    len = strlen(buffer);
    /* We don't want any trailing whitespace in the stream */
    while (len >= 1 && (buffer[len - 1] == '\r' ||
                        buffer[len - 1] == '\n')) {
        buffer[len - 1] = '\0';
        len--;
    }

    sprintf(prefix, "<< /Length %d >>\r\nstream\r\n", len);
    sprintf(suffix, "\r\nendstream\r\n");
    len += strlen(prefix) + strlen(suffix);

    obj = pdf_add_object(pdf, OBJ_stream);
    if (!obj)
        return pdf->errval;
    obj->stream.text = malloc(len + 1);
    obj->stream.text[0] = '\0';
    strcat(obj->stream.text, prefix);
    strcat(obj->stream.text, buffer);
    strcat(obj->stream.text, suffix);

    page->page.nchildren++;
    page->page.children = realloc(page->page.children,
            page->page.nchildren * sizeof (struct pdf_object *));
    page->page.children[page->page.nchildren - 1] = obj;

    return 0;
}

int pdf_add_bookmark(struct pdf_doc *pdf, struct pdf_object *page,
        const char *name)
{
    struct pdf_object *obj = pdf_add_object(pdf, OBJ_bookmark);
    if (!obj)
        return -ENOMEM;

    if (!page)
        page = pdf->last_page;

    if (!page)
        return pdf_set_err(pdf, -EINVAL,
                "Unable to add bookmark, no pages available\n");

    strncpy(obj->bookmark.name, name, sizeof(obj->bookmark.name));
    obj->bookmark.name[sizeof(obj->bookmark.name)] = '\0';
    obj->bookmark.page = page;

    return 0;
}

#define ADDTEXT(a,b...) written += snprintf(&buffer[written],  \
        sizeof(buffer) - written, a , ##b)
int pdf_add_text(struct pdf_doc *pdf, struct pdf_object *page,
        const char *text, int size, int xoff, int yoff, uint32_t colour)
{
    char buffer[1024];
    int written = 0;
    int i;
    int len = text ? strlen(text) : 0;

    /* Don't bother adding empty/null strings */
    if (!len)
        return 0;

    ADDTEXT("BT ");
    ADDTEXT("%d %d TD ", xoff, yoff);
    ADDTEXT("/F1 %d Tf ", size);
    ADDTEXT("%f %f %f rg ", ((colour >> 16) & 0xff) / 255.0,
            ((colour >> 8) & 0xff) / 255.0, (colour & 0xff) / 255.0);
    ADDTEXT("(");

    /* Escape magic characters properly */
    for (i = 0; i < len; i++) {
        if (strchr("()\\", text[i])) {
            /* Escape some characters */
            buffer[written++] = '\\';
            buffer[written++] = text[i];
        } else if (strrchr("\n\r\t", text[i])) {
            /* Skip over these characters */
            ;
        } else
            buffer[written++] = text[i];
    }
    ADDTEXT(") Tj ");
    ADDTEXT("ET");

    return pdf_add_stream(pdf, page, buffer);
}

int pdf_add_line(struct pdf_doc *pdf, struct pdf_object *page,
    int x1, int y1, int x2, int y2, int width, uint32_t colour)
{
    char buffer[1024];
    int written = 0;

    ADDTEXT("BT ");
    ADDTEXT("%d w ", width);
    ADDTEXT("%d %d m ", x1, y1);
    ADDTEXT("%f %f %f RG ", ((colour >> 16) & 0xff) / 255.0,
            ((colour >> 8) & 0xff) / 255.0,
            (colour & 0xff) / 255.0);
    ADDTEXT("%d %d l S ", x2, y2);
    ADDTEXT("ET");

    return pdf_add_stream(pdf, page, buffer);
}

int pdf_add_rectangle(struct pdf_doc *pdf, struct pdf_object *page,
    int x, int y, int width, int height, int border_width,
    uint32_t colour)
{
    char buffer[1024];
    int written = 0;
    ADDTEXT("BT ");
    ADDTEXT("%f %f %f RG ", ((colour >> 16) & 0xff) / 255.0,
            ((colour >> 8) & 0xff) / 255.0,
            (colour & 0xff) / 255.0);
    ADDTEXT("%d w ", border_width);
    ADDTEXT("%d %d %d %d re S ", x, y, width, height);
    ADDTEXT("ET");

    return pdf_add_stream(pdf, page, buffer);
}

int pdf_add_filled_rectangle(struct pdf_doc *pdf, struct pdf_object *page,
    int x, int y, int width, int height, int border_width,
    uint32_t colour)
{
    char buffer[1024];
    int written = 0;
    ADDTEXT("BT ");
    ADDTEXT("%f %f %f rg ", ((colour >> 16) & 0xff) / 255.0,
            ((colour >> 8) & 0xff) / 255.0,
            (colour & 0xff) / 255.0);
    ADDTEXT("%d w ", border_width);
    ADDTEXT("%d %d %d %d re f ", x, y, width, height);
    ADDTEXT("ET");

    return pdf_add_stream(pdf, page, buffer);
}

