/**
 * Simple engine for creating PDF files.
 * It supports text, shapes, images etc...
 * Capable of handling millions of objects without too much performance
 * penalty.
 * Public domain license - no warrenty implied; use at your own risk.
 * @file pdfgen.h
 */
#ifndef PDFGEN_H
#define PDFGEN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

/**
 * @defgroup subsystem Simple PDF Generation
 * Allows for quick generation of simple PDF documents.
 * This is useful for producing easily printed output from C code, where
 * advanced formatting is not required
 *
 * Note: All coordinates/sizes are in points (1/72 of an inch).
 * All coordinates are based on 0,0 being the bottom left of the page.
 * All colours are specified as a packed 32-bit value - see @ref PDF_RGB.
 * Text strings are interpreted as UTF-8 encoded, but only a small subset of
 * characters beyond 7-bit ascii are supported (see @ref pdf_add_text for
 * details).
 *
 * @par PDF library example:
 * @code
#include "pdfgen.h"
 ...
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
pdf_add_line(pdf, NULL, 50, 24, 150, 24);
pdf_save(pdf, "output.pdf");
pdf_destroy(pdf);
 * @endcode
 */

struct pdf_doc;
struct pdf_object;

/**
 * pdf_info describes the metadata to be inserted into the
 * header of the output PDF
 */
struct pdf_info {
    char creator[64];  //!< Software used to create the PDF
    char producer[64]; //!< Software used to create the PDF
    char title[64];    //!< The title of the PDF (typically displayed in the
                       //!< window bar when viewing)
    char author[64];   //!< Who created the PDF
    char subject[64];  //!< What is the PDF about
    char date[64];     //!< The date the PDF was created
};

/**
 * pdf_path_operation holds information about a path
 * drawing operation.
 * See PDF reference for detailed usage.
 */
struct pdf_path_operation {
    char op;  /*!< Operation command. Possible operators are: m = move to, l =
                 line to, c = cubic bezier curve with two control points, v =
                 cubic bezier curve with one control point fixed at first
                 point, y = cubic bezier curve with one control point fixed
                 at second point, h = close path */
    float x1; /*!< X offset of the first point. Used with: m, l, c, v, y */
    float y1; /*!< Y offset of the first point. Used with: m, l, c, v, y */
    float x2; /*!< X offset of the second point. Used with: c, v, y */
    float y2; /*!< Y offset of the second point. Used with: c, v, y */
    float x3; /*!< X offset of the third point. Used with: c */
    float y3; /*!< Y offset of the third point. Used with: c */
};

/**
 * Convert a value in inches into a number of points.
 * Always returns an integer value
 * @param inch inches value to convert to points
 */
#define PDF_INCH_TO_POINT(inch) ((float)((inch)*72.0f))

/**
 * Convert a value in milli-meters into a number of points.
 * Always returns an integer value
 * @param mm millimeter value to convert to points
 */
#define PDF_MM_TO_POINT(mm) ((float)((mm)*72.0f / 25.4f))

/*! Point width of a standard US-Letter page */
#define PDF_LETTER_WIDTH PDF_INCH_TO_POINT(8.5f)

/*! Point height of a standard US-Letter page */
#define PDF_LETTER_HEIGHT PDF_INCH_TO_POINT(11.0f)

/*! Point width of a standard A4 page */
#define PDF_A4_WIDTH PDF_MM_TO_POINT(210.0f)

/*! Point height of a standard A4 page */
#define PDF_A4_HEIGHT PDF_MM_TO_POINT(297.0f)

/*! Point width of a standard A3 page */
#define PDF_A3_WIDTH PDF_MM_TO_POINT(297.0f)

/*! Point height of a standard A3 page */
#define PDF_A3_HEIGHT PDF_MM_TO_POINT(420.0f)

/**
 * Convert three 8-bit RGB values into a single packed 32-bit
 * colour. These 32-bit colours are used by various functions
 * in PDFGen
 */
#define PDF_RGB(r, g, b)                                                     \
    (uint32_t)((((r)&0xff) << 16) | (((g)&0xff) << 8) | (((b)&0xff)))

/**
 * Convert four 8-bit ARGB values into a single packed 32-bit
 * colour. These 32-bit colours are used by various functions
 * in PDFGen. Alpha values range from 0 (opaque) to 0xff
 * (transparent)
 */
#define PDF_ARGB(a, r, g, b)                                                 \
    (uint32_t)(((uint32_t)((a)&0xff) << 24) | (((r)&0xff) << 16) |           \
               (((g)&0xff) << 8) | (((b)&0xff)))

/*! Utility macro to provide bright red */
#define PDF_RED PDF_RGB(0xff, 0, 0)

/*! Utility macro to provide bright green */
#define PDF_GREEN PDF_RGB(0, 0xff, 0)

/*! Utility macro to provide bright blue */
#define PDF_BLUE PDF_RGB(0, 0, 0xff)

/*! Utility macro to provide black */
#define PDF_BLACK PDF_RGB(0, 0, 0)

/*! Utility macro to provide white */
#define PDF_WHITE PDF_RGB(0xff, 0xff, 0xff)

/*!
 * Utility macro to provide a transparent colour
 * This is used in some places for 'fill' colours, where no fill is required
 */
#define PDF_TRANSPARENT (uint32_t)(0xffu << 24)

/**
 * Different alignment options for rendering text
 */
enum {
    PDF_ALIGN_LEFT,    //!< Align text to the left
    PDF_ALIGN_RIGHT,   //!< Align text to the right
    PDF_ALIGN_CENTER,  //!< Align text in the center
    PDF_ALIGN_JUSTIFY, //!< Align text in the center, with padding to fill the
                       //!< available space
    PDF_ALIGN_JUSTIFY_ALL, //!< Like PDF_ALIGN_JUSTIFY, except even short
                           //!< lines will be fully justified
    PDF_ALIGN_NO_WRITE, //!< Fake alignment for only checking wrap height with
                        //!< no writes
};

/**
 * Create a new PDF object, with the given page
 * width/height
 * @param width Width of the page
 * @param height Height of the page
 * @param info Optional information to be put into the PDF header
 * @return PDF document object, or NULL on failure
 */
struct pdf_doc *pdf_create(float width, float height,
                           const struct pdf_info *info);

/**
 * Destroy the pdf object, and all of its associated memory
 * @param pdf PDF document to clean up
 */
void pdf_destroy(struct pdf_doc *pdf);

/**
 * Retrieve the error message if any operation fails
 * @param pdf pdf document to retrieve error message from
 * @param errval optional pointer to an integer to be set to the error code
 * @return NULL if no error message, string description of error otherwise
 */
const char *pdf_get_err(const struct pdf_doc *pdf, int *errval);

/**
 * Acknowledge an outstanding pdf error
 * @param pdf pdf document to clear the error message from
 */
void pdf_clear_err(struct pdf_doc *pdf);

/**
 * Sets the font to use for text objects. Default value is Times-Roman if
 * this function is not called.
 * Note: The font selection should be done before text is output,
 * and will remain until pdf_set_font is called again.
 * @param pdf PDF document to update font on
 * @param font New font to use. This must be one of the standard PDF fonts:
 *  Courier, Courier-Bold, Courier-BoldOblique, Courier-Oblique,
 *  Helvetica, Helvetica-Bold, Helvetica-BoldOblique, Helvetica-Oblique,
 *  Times-Roman, Times-Bold, Times-Italic, Times-BoldItalic,
 *  Symbol or ZapfDingbats
 * @return < 0 on failure, 0 on success
 */
int pdf_set_font(struct pdf_doc *pdf, const char *font);

/**
 * Calculate the width of a given string in the current font
 * @param pdf PDF document
 * @param font_name Name of the font to get the width of.
 *  This must be one of the standard PDF fonts:
 *  Courier, Courier-Bold, Courier-BoldOblique, Courier-Oblique,
 *  Helvetica, Helvetica-Bold, Helvetica-BoldOblique, Helvetica-Oblique,
 *  Times-Roman, Times-Bold, Times-Italic, Times-BoldItalic,
 *  Symbol or ZapfDingbats
 * @param text Text to determine width of
 * @param size Size of the text, in points
 * @param text_width area to store calculated width in
 * @return < 0 on failure, 0 on success
 */
int pdf_get_font_text_width(struct pdf_doc *pdf, const char *font_name,
                            const char *text, float size, float *text_width);

/**
 * Retrieves a PDF document height
 * @param pdf PDF document to get height of
 * @return height of PDF document (in points)
 */
float pdf_height(const struct pdf_doc *pdf);

/**
 * Retrieves a PDF document width
 * @param pdf PDF document to get width of
 * @return width of PDF document (in points)
 */
float pdf_width(const struct pdf_doc *pdf);

/**
 * Add a new page to the given pdf
 * @param pdf PDF document to append page to
 * @return new page object
 */
struct pdf_object *pdf_append_page(struct pdf_doc *pdf);

/**
 * Adjust the width/height of a specific page
 * @param pdf PDF document that the page belongs to
 * @param page object returned from @ref pdf_append_page
 * @param width Width of the page in points
 * @param height Height of the page in points
 * @return < 0 on failure, 0 on success
 */
int pdf_page_set_size(struct pdf_doc *pdf, struct pdf_object *page,
                      float width, float height);

/**
 * Save the given pdf document to the supplied filename.
 * @param pdf PDF document to save
 * @param filename Name of the file to store the PDF into (NULL for stdout)
 * @return < 0 on failure, >= 0 on success
 */
int pdf_save(struct pdf_doc *pdf, const char *filename);

/**
 * Save the given pdf document to the given FILE output
 * @param pdf PDF document to save
 * @param fp FILE pointer to store the data into (must be writable)
 * @return < 0 on failure, >= 0 on success
 */
int pdf_save_file(struct pdf_doc *pdf, FILE *fp);

/**
 * Add a text string to the document
 * @param pdf PDF document to add to
 * @param page Page to add object to (NULL => most recently added page)
 * @param text String to display
 * @param size Point size of the font
 * @param xoff X location to put it in
 * @param yoff Y location to put it in
 * @param colour Colour to draw the text
 * @return 0 on success, < 0 on failure
 */
int pdf_add_text(struct pdf_doc *pdf, struct pdf_object *page,
                 const char *text, float size, float xoff, float yoff,
                 uint32_t colour);

/**
 * Add a text string to the document, making it wrap if it is too
 * long
 * @param pdf PDF document to add to
 * @param page Page to add object to (NULL => most recently added page)
 * @param text String to display
 * @param size Point size of the font
 * @param xoff X location to put it in
 * @param yoff Y location to put it in
 * @param colour Colour to draw the text
 * @param wrap_width Width at which to wrap the text
 * @param align Text alignment (see PDF_ALIGN_xxx)
 * @param height Store the final height of the wrapped text here (optional)
 * @return < 0 on failure, >= 0 on success
 */
int pdf_add_text_wrap(struct pdf_doc *pdf, struct pdf_object *page,
                      const char *text, float size, float xoff, float yoff,
                      uint32_t colour, float wrap_width, int align,
                      float *height);

/**
 * Add a line to the document
 * @param pdf PDF document to add to
 * @param page Page to add object to (NULL => most recently added page)
 * @param x1 X offset of start of line
 * @param y1 Y offset of start of line
 * @param x2 X offset of end of line
 * @param y2 Y offset of end of line
 * @param width Width of the line
 * @param colour Colour to draw the line
 * @return 0 on success, < 0 on failure
 */
int pdf_add_line(struct pdf_doc *pdf, struct pdf_object *page, float x1,
                 float y1, float x2, float y2, float width, uint32_t colour);

/**
 * Add a cubic bezier curve to the document
 * @param pdf PDF document to add to
 * @param page Page to add object to (NULL => most recently added page)
 * @param x1 X offset of the initial point of the curve
 * @param y1 Y offset of the initial point of the curve
 * @param x2 X offset of the final point of the curve
 * @param y2 Y offset of the final point of the curve
 * @param xq1 X offset of the first control point of the curve
 * @param yq1 Y offset of the first control point of the curve
 * @param xq2 X offset of the second control of the curve
 * @param yq2 Y offset of the second control of the curve
 * @param width Width of the curve
 * @param colour Colour to draw the curve
 * @return 0 on success, < 0 on failure
 */
int pdf_add_cubic_bezier(struct pdf_doc *pdf, struct pdf_object *page,
                         float x1, float y1, float x2, float y2, float xq1,
                         float yq1, float xq2, float yq2, float width,
                         uint32_t colour);

/**
 * Add a quadratic bezier curve to the document
 * @param pdf PDF document to add to
 * @param page Page to add object to (NULL => most recently added page)
 * @param x1 X offset of the initial point of the curve
 * @param y1 Y offset of the initial point of the curve
 * @param x2 X offset of the final point of the curve
 * @param y2 Y offset of the final point of the curve
 * @param xq1 X offset of the control point of the curve
 * @param yq1 Y offset of the control point of the curve
 * @param width Width of the curve
 * @param colour Colour to draw the curve
 * @return 0 on success, < 0 on failure
 */
int pdf_add_quadratic_bezier(struct pdf_doc *pdf, struct pdf_object *page,
                             float x1, float y1, float x2, float y2,
                             float xq1, float yq1, float width,
                             uint32_t colour);

/**
 * Add a custom path to the document
 * @param pdf PDF document to add to
 * @param page Page to add object to (NULL => most recently added page)
 * @param operations Array of drawing operations
 * @param operation_count The number of operations
 * @param stroke_width Width of the stroke
 * @param stroke_colour Colour to stroke the curve
 * @param fill_colour Colour to fill the path
 * @return 0 on success, < 0 on failure
 */
int pdf_add_custom_path(struct pdf_doc *pdf, struct pdf_object *page,
                        struct pdf_path_operation *operations,
                        int operation_count, float stroke_width,
                        uint32_t stroke_colour, uint32_t fill_colour);

/**
 * Add an ellipse to the document
 * @param pdf PDF document to add to
 * @param page Page to add object to (NULL => most recently added page)
 * @param x X offset of the center of the ellipse
 * @param y Y offset of the center of the ellipse
 * @param xradius Radius of the ellipse in the X axis
 * @param yradius Radius of the ellipse in the Y axis
 * @param colour Colour to draw the ellipse outline stroke
 * @param width Width of the ellipse outline stroke
 * @param fill_colour Colour to fill the ellipse
 * @return 0 on success, < 0 on failure
 */
int pdf_add_ellipse(struct pdf_doc *pdf, struct pdf_object *page, float x,
                    float y, float xradius, float yradius, float width,
                    uint32_t colour, uint32_t fill_colour);

/**
 * Add a circle to the document
 * @param pdf PDF document to add to
 * @param page Page to add object to (NULL => most recently added page)
 * @param x X offset of the center of the circle
 * @param y Y offset of the center of the circle
 * @param radius Radius of the circle
 * @param width Width of the circle outline stroke
 * @param colour Colour to draw the circle outline stroke
 * @param fill_colour Colour to fill the circle
 * @return 0 on success, < 0 on failure
 */
int pdf_add_circle(struct pdf_doc *pdf, struct pdf_object *page, float x,
                   float y, float radius, float width, uint32_t colour,
                   uint32_t fill_colour);

/**
 * Add an outline rectangle to the document
 * @param pdf PDF document to add to
 * @param page Page to add object to (NULL => most recently added page)
 * @param x X offset to start rectangle at
 * @param y Y offset to start rectangle at
 * @param width Width of rectangle
 * @param height Height of rectangle
 * @param border_width Width of rectangle border
 * @param colour Colour to draw the rectangle
 * @return 0 on success, < 0 on failure
 */
int pdf_add_rectangle(struct pdf_doc *pdf, struct pdf_object *page, float x,
                      float y, float width, float height, float border_width,
                      uint32_t colour);

/**
 * Add a filled rectangle to the document
 * @param pdf PDF document to add to
 * @param page Page to add object to (NULL => most recently added page)
 * @param x X offset to start rectangle at
 * @param y Y offset to start rectangle at
 * @param width Width of rectangle
 * @param height Height of rectangle
 * @param border_width Width of rectangle border
 * @param colour Colour to draw the rectangle
 * @return 0 on success, < 0 on failure
 */
int pdf_add_filled_rectangle(struct pdf_doc *pdf, struct pdf_object *page,
                             float x, float y, float width, float height,
                             float border_width, uint32_t colour);

/**
 * Add an outline polygon to the document
 * @param pdf PDF document to add to
 * @param page Page to add object to (NULL => most recently added page)
 * @param x array of X offsets for points comprising the polygon
 * @param y array of Y offsets for points comprising the polygon
 * @param count Number of points comprising the polygon
 * @param border_width Width of polygon border
 * @param colour Colour to draw the polygon
 * @return 0 on success, < 0 on failure
 */
int pdf_add_polygon(struct pdf_doc *pdf, struct pdf_object *page, float x[],
                    float y[], int count, float border_width,
                    uint32_t colour);

/**
 * Add a filled polygon to the document
 * @param pdf PDF document to add to
 * @param page Page to add object to (NULL => most recently added page)
 * @param x array of X offsets of points comprising the polygon
 * @param y array of Y offsets of points comprising the polygon
 * @param count Number of points comprising the polygon
 * @param border_width Width of polygon border
 * @param colour Colour to draw the polygon
 * @return 0 on success, < 0 on failure
 */
int pdf_add_filled_polygon(struct pdf_doc *pdf, struct pdf_object *page,
                           float x[], float y[], int count,
                           float border_width, uint32_t colour);

/**
 * Add a bookmark to the document
 * @param pdf PDF document to add bookmark to
 * @param page Page to jump to for bookmark
               (or NULL for the most recently added page)
 * @param parent ID of a previously created bookmark that is the parent
               of this one. -1 if this should be a top-level bookmark.
 * @param name String to associate with the bookmark
 * @return < 0 on failure, new bookmark id on success
 */
int pdf_add_bookmark(struct pdf_doc *pdf, struct pdf_object *page, int parent,
                     const char *name);

/**
 * List of different barcode encodings that are supported
 */
enum {
    PDF_BARCODE_128A, //!< Produce code-128A style barcodes
    PDF_BARCODE_39,   //!< Produce code-39 style barcodes
};

/**
 * Add a barcode to the document
 * @param pdf PDF document to add barcode to
 * @param page Page to add barcode to (NULL => most recently added page)
 * @param code Type of barcode to add (PDF_BARCODE_xxx)
 * @param x X offset to put barcode at
 * @param y Y offset to put barcode at
 * @param width Width of barcode
 * @param height Height of barcode
 * @param string Barcode contents
 * @param colour Colour to draw barcode
 * @return < 0 on failure, >= 0 on success
 */
int pdf_add_barcode(struct pdf_doc *pdf, struct pdf_object *page, int code,
                    float x, float y, float width, float height,
                    const char *string, uint32_t colour);

/**
 * Add a PPM file as an image to the document
 * @param pdf PDF document to add PPM to
 * @param page Page to add PPM to (NULL => most recently added page)
 * @param x X offset to put PPM at
 * @param y Y offset to put PPM at
 * @param display_width Displayed width of image
 * @param display_height Displayed height of image
 * @param ppm_file Filename of P6 (binary) ppm file to display
 * @return < 0 on failure, >= 0 on success
 */
int pdf_add_ppm(struct pdf_doc *pdf, struct pdf_object *page, float x,
                float y, float display_width, float display_height,
                const char *ppm_file);

/**
 * Add a JPEG file as an image to the document
 * @param pdf PDF document to add JPEG to
 * @param page Page to add JPEG to (NULL => most recently added page)
 * @param x X offset to put JPEG at
 * @param y Y offset to put JPEG at
 * @param display_width Displayed width of image
 * @param display_height Displayed height of image
 * @param jpeg_file Filename of JPEG file to display
 * @return < 0 on failure, >= 0 on success
 */
int pdf_add_jpeg(struct pdf_doc *pdf, struct pdf_object *page, float x,
                 float y, float display_width, float display_height,
                 const char *jpeg_file);

/**
 * Add JPEG data as an image to the document
 * @param pdf PDF document to add JPEG to
 * @param page Page to add JPEG to (NULL => most recently added page)
 * @param x X offset to put JPEG at
 * @param y Y offset to put JPEG at
 * @param display_width Displayed width of image
 * @param display_height Displayed height of image
 * @param jpeg_data JPEG data to add
 * @param len Length of JPEG data
 * @return < 0 on failure, >= 0 on success
 */
int pdf_add_jpeg_data(struct pdf_doc *pdf, struct pdf_object *page, float x,
                      float y, float display_width, float display_height,
                      const unsigned char *jpeg_data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // PDFGEN_H
