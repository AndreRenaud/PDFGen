// stb_truetype.h - v1.26 - public domain
// authored from 2009-2021 by Sean Barrett / RAD Game Tools

// #define your own (u)stbtt_int8/16/32 before including to override this
#ifndef stbtt_uint8
typedef unsigned char stbtt_uint8;
typedef signed char stbtt_int8;
typedef unsigned short stbtt_uint16;
typedef signed short stbtt_int16;
typedef unsigned int stbtt_uint32;
typedef signed int stbtt_int32;
#endif

typedef char stbtt__check_size32[sizeof(stbtt_int32) == 4 ? 1 : -1];
typedef char stbtt__check_size16[sizeof(stbtt_int16) == 2 ? 1 : -1];

// e.g. #define your own STBTT_ifloor/STBTT_iceil() to avoid math.h
#ifndef STBTT_ifloor
#include <math.h>
#define STBTT_ifloor(x) ((int)floor(x))
#define STBTT_iceil(x) ((int)ceil(x))
#endif

#ifndef STBTT_sqrt
#include <math.h>
#define STBTT_sqrt(x) sqrt(x)
#define STBTT_pow(x, y) pow(x, y)
#endif

#ifndef STBTT_fmod
#include <math.h>
#define STBTT_fmod(x, y) fmod(x, y)
#endif

#ifndef STBTT_cos
#include <math.h>
#define STBTT_cos(x) cos(x)
#define STBTT_acos(x) acos(x)
#endif

#ifndef STBTT_fabs
#include <math.h>
#define STBTT_fabs(x) fabs(x)
#endif

// #define your own functions "STBTT_malloc" / "STBTT_free" to avoid malloc.h
#ifndef STBTT_malloc
#include <stdlib.h>
#define STBTT_malloc(x, u) ((void)(u), malloc(x))
#define STBTT_free(x, u) ((void)(u), free(x))
#endif

#ifndef STBTT_assert
#include <assert.h>
#define STBTT_assert(x) assert(x)
#endif

#ifndef STBTT_strlen
#include <string.h>
#define STBTT_strlen(x) strlen(x)
#endif

#ifndef STBTT_memcpy
#include <string.h>
#define STBTT_memcpy memcpy
#define STBTT_memset memset
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
////
////   INTERFACE
////
////

#ifdef STBTT_STATIC
#define STBTT_DEF static
#else
#define STBTT_DEF extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

// private structure
typedef struct {
    unsigned char *data;
    int cursor;
    int size;
} stbtt__buf;

typedef struct stbtt_fontinfo stbtt_fontinfo;

//////////////////////////////////////////////////////////////////////////////
//
// FONT LOADING
//
//

STBTT_DEF int stbtt_GetNumberOfFonts(const unsigned char *data);
// This function will determine the number of fonts in a font file.  TrueType
// collection (.ttc) files may contain multiple fonts, while TrueType font
// (.ttf) files only contain one font. The number of fonts can be used for
// indexing with the previous function where the index is between zero and one
// less than the total fonts. If an error occurs, -1 is returned.

STBTT_DEF int stbtt_GetFontOffsetForIndex(const unsigned char *data,
                                          int index);
// Each .ttf/.ttc file may have more than one font. Each font has a sequential
// index number starting from 0. Call this function to get the font offset for
// a given index; it returns -1 if the index is out of range. A regular .ttf
// file will only define one font and it always be at offset 0, so it will
// return '0' for index 0, and -1 for all other indices.

// The following structure is defined publicly so you can declare one on
// the stack or as a global or etc, but you should treat it as opaque.
struct stbtt_fontinfo {
    void *userdata;
    unsigned char *data; // pointer to .ttf file
    int fontstart;       // offset of start of font

    int numGlyphs; // number of glyphs, needed for range checking

    int loca, head, glyf, hhea, hmtx, kern, gpos,
        svg;              // table locations as offset from start of .ttf
    int index_map;        // a cmap mapping for our chosen character encoding
    int indexToLocFormat; // format needed to map from glyph index to glyph

    stbtt__buf cff;         // cff font data
    stbtt__buf charstrings; // the charstring index
    stbtt__buf gsubrs;      // global charstring subroutines index
    stbtt__buf subrs;       // private charstring subroutines index
    stbtt__buf fontdicts;   // array of font dicts
    stbtt__buf fdselect;    // map from glyph to fontdict
};

STBTT_DEF int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data,
                             int offset);
// Given an offset into the file that defines a font, this function builds
// the necessary cached info for the rest of the system. You must allocate
// the stbtt_fontinfo yourself, and stbtt_InitFont will fill it out. You don't
// need to do anything special to free it, because the contents are pure
// value data with no additional data structures. Returns 0 on failure.

//////////////////////////////////////////////////////////////////////////////
//
// CHARACTER TO GLYPH-INDEX CONVERSIOn

STBTT_DEF int stbtt_FindGlyphIndex(const stbtt_fontinfo *info,
                                   int unicode_codepoint);
// If you're going to perform multiple operations on the same character
// and you want a speed-up, call this function with the character you're
// going to process, then use glyph-based functions instead of the
// codepoint-based functions.
// Returns 0 if the character codepoint is not defined in the font.

//////////////////////////////////////////////////////////////////////////////
//
// CHARACTER PROPERTIES
//

STBTT_DEF float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info,
                                          float pixels);
// computes a scale factor to produce a font whose "height" is 'pixels' tall.
// Height is measured as the distance from the highest ascender to the lowest
// descender; in other words, it's equivalent to calling stbtt_GetFontVMetrics
// and computing:
//       scale = pixels / (ascent - descent)
// so if you prefer to measure height by the ascent only, use a similar
// calculation.

STBTT_DEF float stbtt_ScaleForMappingEmToPixels(const stbtt_fontinfo *info,
                                                float pixels);
// computes a scale factor to produce a font whose EM size is mapped to
// 'pixels' tall. This is probably what traditional APIs compute, but
// I'm not positive.

STBTT_DEF void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int *ascent,
                                     int *descent, int *lineGap);
// ascent is the coordinate above the baseline the font extends; descent
// is the coordinate below the baseline the font extends (i.e. it is typically
// negative) lineGap is the spacing between one row's descent and the next
// row's ascent... so you should advance the vertical position by "*ascent -
// *descent + *lineGap"
//   these are expressed in unscaled coordinates, so you must multiply by
//   the scale factor for a given size

STBTT_DEF int stbtt_GetFontVMetricsOS2(const stbtt_fontinfo *info,
                                       int *typoAscent, int *typoDescent,
                                       int *typoLineGap);
// analogous to GetFontVMetrics, but returns the "typographic" values from the
// OS/2 table (specific to MS/Windows TTF files).
//
// Returns 1 on success (table present), 0 on failure.

STBTT_DEF void stbtt_GetFontBoundingBox(const stbtt_fontinfo *info, int *x0,
                                        int *y0, int *x1, int *y1);
// the bounding box around all possible characters

STBTT_DEF void stbtt_GetCodepointHMetrics(const stbtt_fontinfo *info,
                                          int codepoint, int *advanceWidth,
                                          int *leftSideBearing);
// leftSideBearing is the offset from the current horizontal position to the
// left edge of the character advanceWidth is the offset from the current
// horizontal position to the next horizontal position
//   these are expressed in unscaled coordinates

STBTT_DEF int stbtt_GetCodepointKernAdvance(const stbtt_fontinfo *info,
                                            int ch1, int ch2);
// an additional amount to add to the 'advance' value between ch1 and ch2

STBTT_DEF int stbtt_GetCodepointBox(const stbtt_fontinfo *info, int codepoint,
                                    int *x0, int *y0, int *x1, int *y1);
// Gets the bounding box of the visible part of the glyph, in unscaled
// coordinates

STBTT_DEF void stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info,
                                      int glyph_index, int *advanceWidth,
                                      int *leftSideBearing);
STBTT_DEF int stbtt_GetGlyphKernAdvance(const stbtt_fontinfo *info,
                                        int glyph1, int glyph2);
STBTT_DEF int stbtt_GetGlyphBox(const stbtt_fontinfo *info, int glyph_index,
                                int *x0, int *y0, int *x1, int *y1);
// as above, but takes one or more glyph indices for greater efficiency

// These functions compute a discretized SDF field for a single character,
// suitable for storing in a single-channel texture, sampling with bilinear
// filtering, and testing against larger than some threshold to produce
// scalable fonts.
//        info              --  the font
//        scale             --  controls the size of the resulting SDF bitmap,
//        same as it would be creating a regular bitmap glyph/codepoint   --
//        the character to generate the SDF for padding           --  extra
//        "pixels" around the character which are filled with the distance to
//        the character (not 0),
//                                 which allows effects like bit outlines
//        onedge_value      --  value 0-255 to test the SDF against to
//        reconstruct the character (i.e. the isocontour of the character)
//        pixel_dist_scale  --  what value the SDF should increase by when
//        moving one SDF "pixel" away from the edge (on the 0..255 scale)
//                                 if positive, > onedge_value is inside; if
//                                 negative, < onedge_value is inside
//        width,height      --  output height & width of the SDF bitmap
//        (including padding) xoff,yoff         --  output origin of the
//        character return value      --  a 2D array of bytes 0..255,
//        width*height in size
//
// pixel_dist_scale & onedge_value are a scale & bias that allows you to make
// optimal use of the limited 0..255 for your application, trading off
// precision and special effects. SDF values outside the range 0..255 are
// clamped to 0..255.
//
// Example:
//      scale = stbtt_ScaleForPixelHeight(22)
//      padding = 5
//      onedge_value = 180
//      pixel_dist_scale = 180/5.0 = 36.0
//
//      This will create an SDF bitmap in which the character is about 22
//      pixels high but the whole bitmap is about 22+5+5=32 pixels high. To
//      produce a filled shape, sample the SDF at each pixel and fill the
//      pixel if the SDF value is greater than or equal to 180/255. (You'll
//      actually want to antialias, which is beyond the scope of this
//      example.) Additionally, you can compute offset outlines (e.g. to
//      stroke the character border inside & outside, or only outside). For
//      example, to fill outside the character up to 3 SDF pixels, you would
//      compare against (180-36.0*3)/255 = 72/255. The above choice of
//      variables maps a range from 5 pixels outside the shape to 2 pixels
//      inside the shape to 0..255; this is intended primarily for apply
//      outside effects only (the interior range is needed to allow proper
//      antialiasing of the font at *smaller* sizes)
//
// The function computes the SDF analytically at each SDF pixel, not by e.g.
// building a higher-res bitmap and approximating it. In theory the quality
// should be as high as possible for an SDF of this size & representation, but
// unclear if this is true in practice (perhaps building a higher-res bitmap
// and computing from that can allow drop-out prevention).
//
// The algorithm has not been optimized at all, so expect it to be slow
// if computing lots of characters or very large sizes.

//////////////////////////////////////////////////////////////////////////////
//
// Finding the right font...
//
// You should really just solve this offline, keep your own tables
// of what font is what, and don't try to get it out of the .ttf file.
// That's because getting it out of the .ttf file is really hard, because
// the names in the file can appear in many possible encodings, in many
// possible languages, and e.g. if you need a case-insensitive comparison,
// the details of that depend on the encoding & language in a complex way
// (actually underspecified in truetype, but also gigantic).
//
// But you can use the provided functions in two possible ways:
//     stbtt_FindMatchingFont() will use *case-sensitive* comparisons on
//             unicode-encoded names to try to find the font you want;
//             you can run this before calling stbtt_InitFont()
//
//     stbtt_GetFontNameString() lets you get any of the various strings
//             from the file yourself and do your own comparisons on them.
//             You have to have called stbtt_InitFont() first.

STBTT_DEF int stbtt_FindMatchingFont(const unsigned char *fontdata,
                                     const char *name, int flags);
// returns the offset (not index) of the font that matches, or -1 if none
//   if you use STBTT_MACSTYLE_DONTCARE, use a font name like "Arial Bold".
//   if you use any other flag, use a font name like "Arial"; this checks
//     the 'macStyle' header field; i don't know if fonts set this
//     consistently
#define STBTT_MACSTYLE_DONTCARE 0
#define STBTT_MACSTYLE_BOLD 1
#define STBTT_MACSTYLE_ITALIC 2
#define STBTT_MACSTYLE_UNDERSCORE 4
#define STBTT_MACSTYLE_NONE                                                  \
    8 // <= not same as 0, this makes us check the bitfield is 0

STBTT_DEF int stbtt_CompareUTF8toUTF16_bigendian(const char *s1, int len1,
                                                 const char *s2, int len2);
// returns 1/0 whether the first string interpreted as utf8 is identical to
// the second string interpreted as big-endian utf16... useful for strings
// from next func

STBTT_DEF const char *stbtt_GetFontNameString(const stbtt_fontinfo *font,
                                              int *length, int platformID,
                                              int encodingID, int languageID,
                                              int nameID);
// returns the string (which may be big-endian double byte, e.g. for unicode)
// and puts the length in bytes in *length.
//
// some of the values for the IDs are below; for more see the truetype spec:
//     http://developer.apple.com/textfonts/TTRefMan/RM06/Chap6name.html
//     http://www.microsoft.com/typography/otspec/name.htm

enum { // platformID
    STBTT_PLATFORM_ID_UNICODE = 0,
    STBTT_PLATFORM_ID_MAC = 1,
    STBTT_PLATFORM_ID_ISO = 2,
    STBTT_PLATFORM_ID_MICROSOFT = 3
};

enum { // encodingID for STBTT_PLATFORM_ID_UNICODE
    STBTT_UNICODE_EID_UNICODE_1_0 = 0,
    STBTT_UNICODE_EID_UNICODE_1_1 = 1,
    STBTT_UNICODE_EID_ISO_10646 = 2,
    STBTT_UNICODE_EID_UNICODE_2_0_BMP = 3,
    STBTT_UNICODE_EID_UNICODE_2_0_FULL = 4
};

enum { // encodingID for STBTT_PLATFORM_ID_MICROSOFT
    STBTT_MS_EID_SYMBOL = 0,
    STBTT_MS_EID_UNICODE_BMP = 1,
    STBTT_MS_EID_SHIFTJIS = 2,
    STBTT_MS_EID_UNICODE_FULL = 10
};

enum { // encodingID for STBTT_PLATFORM_ID_MAC; same as Script Manager codes
    STBTT_MAC_EID_ROMAN = 0,
    STBTT_MAC_EID_ARABIC = 4,
    STBTT_MAC_EID_JAPANESE = 1,
    STBTT_MAC_EID_HEBREW = 5,
    STBTT_MAC_EID_CHINESE_TRAD = 2,
    STBTT_MAC_EID_GREEK = 6,
    STBTT_MAC_EID_KOREAN = 3,
    STBTT_MAC_EID_RUSSIAN = 7
};

enum { // languageID for STBTT_PLATFORM_ID_MICROSOFT; same as LCID...
       // problematic because there are e.g. 16 english LCIDs and 16 arabic
       // LCIDs
    STBTT_MS_LANG_ENGLISH = 0x0409,
    STBTT_MS_LANG_ITALIAN = 0x0410,
    STBTT_MS_LANG_CHINESE = 0x0804,
    STBTT_MS_LANG_JAPANESE = 0x0411,
    STBTT_MS_LANG_DUTCH = 0x0413,
    STBTT_MS_LANG_KOREAN = 0x0412,
    STBTT_MS_LANG_FRENCH = 0x040c,
    STBTT_MS_LANG_RUSSIAN = 0x0419,
    STBTT_MS_LANG_GERMAN = 0x0407,
    STBTT_MS_LANG_SPANISH = 0x0409,
    STBTT_MS_LANG_HEBREW = 0x040d,
    STBTT_MS_LANG_SWEDISH = 0x041D
};

enum { // languageID for STBTT_PLATFORM_ID_MAC
    STBTT_MAC_LANG_ENGLISH = 0,
    STBTT_MAC_LANG_JAPANESE = 11,
    STBTT_MAC_LANG_ARABIC = 12,
    STBTT_MAC_LANG_KOREAN = 23,
    STBTT_MAC_LANG_DUTCH = 4,
    STBTT_MAC_LANG_RUSSIAN = 32,
    STBTT_MAC_LANG_FRENCH = 1,
    STBTT_MAC_LANG_SPANISH = 6,
    STBTT_MAC_LANG_GERMAN = 2,
    STBTT_MAC_LANG_SWEDISH = 5,
    STBTT_MAC_LANG_HEBREW = 10,
    STBTT_MAC_LANG_CHINESE_SIMPLIFIED = 33,
    STBTT_MAC_LANG_ITALIAN = 3,
    STBTT_MAC_LANG_CHINESE_TRAD = 19
};

#ifdef __cplusplus
}
#endif


///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
////
////   IMPLEMENTATION
////
////

//////////////////////////////////////////////////////////////////////////
//
// stbtt__buf helpers to parse data from file
//

static stbtt_uint8 stbtt__buf_get8(stbtt__buf *b)
{
    if (b->cursor >= b->size)
        return 0;
    return b->data[b->cursor++];
}

static stbtt_uint8 stbtt__buf_peek8(stbtt__buf *b)
{
    if (b->cursor >= b->size)
        return 0;
    return b->data[b->cursor];
}

static void stbtt__buf_seek(stbtt__buf *b, int o)
{
    STBTT_assert(!(o > b->size || o < 0));
    b->cursor = (o > b->size || o < 0) ? b->size : o;
}

static void stbtt__buf_skip(stbtt__buf *b, int o)
{
    stbtt__buf_seek(b, b->cursor + o);
}

static stbtt_uint32 stbtt__buf_get(stbtt__buf *b, int n)
{
    stbtt_uint32 v = 0;
    int i;
    STBTT_assert(n >= 1 && n <= 4);
    for (i = 0; i < n; i++)
        v = (v << 8) | stbtt__buf_get8(b);
    return v;
}

static stbtt__buf stbtt__new_buf(const void *p, size_t size)
{
    stbtt__buf r;
    STBTT_assert(size < 0x40000000);
    r.data = (stbtt_uint8 *)p;
    r.size = (int)size;
    r.cursor = 0;
    return r;
}

#define stbtt__buf_get16(b) stbtt__buf_get((b), 2)
#define stbtt__buf_get32(b) stbtt__buf_get((b), 4)

static stbtt__buf stbtt__buf_range(const stbtt__buf *b, int o, int s)
{
    stbtt__buf r = stbtt__new_buf(NULL, 0);
    if (o < 0 || s < 0 || o > b->size || s > b->size - o)
        return r;
    r.data = b->data + o;
    r.size = s;
    return r;
}

static stbtt__buf stbtt__cff_get_index(stbtt__buf *b)
{
    int count, start, offsize;
    start = b->cursor;
    count = stbtt__buf_get16(b);
    if (count) {
        offsize = stbtt__buf_get8(b);
        STBTT_assert(offsize >= 1 && offsize <= 4);
        stbtt__buf_skip(b, offsize * count);
        stbtt__buf_skip(b, stbtt__buf_get(b, offsize) - 1);
    }
    return stbtt__buf_range(b, start, b->cursor - start);
}

static stbtt_uint32 stbtt__cff_int(stbtt__buf *b)
{
    int b0 = stbtt__buf_get8(b);
    if (b0 >= 32 && b0 <= 246)
        return b0 - 139;
    else if (b0 >= 247 && b0 <= 250)
        return (b0 - 247) * 256 + stbtt__buf_get8(b) + 108;
    else if (b0 >= 251 && b0 <= 254)
        return -(b0 - 251) * 256 - stbtt__buf_get8(b) - 108;
    else if (b0 == 28)
        return stbtt__buf_get16(b);
    else if (b0 == 29)
        return stbtt__buf_get32(b);
    STBTT_assert(0);
    return 0;
}

static void stbtt__cff_skip_operand(stbtt__buf *b)
{
    int v, b0 = stbtt__buf_peek8(b);
    STBTT_assert(b0 >= 28);
    if (b0 == 30) {
        stbtt__buf_skip(b, 1);
        while (b->cursor < b->size) {
            v = stbtt__buf_get8(b);
            if ((v & 0xF) == 0xF || (v >> 4) == 0xF)
                break;
        }
    } else {
        stbtt__cff_int(b);
    }
}

static stbtt__buf stbtt__dict_get(stbtt__buf *b, int key)
{
    stbtt__buf_seek(b, 0);
    while (b->cursor < b->size) {
        int start = b->cursor, end, op;
        while (stbtt__buf_peek8(b) >= 28)
            stbtt__cff_skip_operand(b);
        end = b->cursor;
        op = stbtt__buf_get8(b);
        if (op == 12)
            op = stbtt__buf_get8(b) | 0x100;
        if (op == key)
            return stbtt__buf_range(b, start, end - start);
    }
    return stbtt__buf_range(b, 0, 0);
}

static void stbtt__dict_get_ints(stbtt__buf *b, int key, int outcount,
                                 stbtt_uint32 *out)
{
    int i;
    stbtt__buf operands = stbtt__dict_get(b, key);
    for (i = 0; i < outcount && operands.cursor < operands.size; i++)
        out[i] = stbtt__cff_int(&operands);
}

#if 0
static int stbtt__cff_index_count(stbtt__buf *b)
{
    stbtt__buf_seek(b, 0);
    return stbtt__buf_get16(b);
}
#endif
static stbtt__buf stbtt__cff_index_get(stbtt__buf b, int i)
{
    int count, offsize, start, end;
    stbtt__buf_seek(&b, 0);
    count = stbtt__buf_get16(&b);
    offsize = stbtt__buf_get8(&b);
    STBTT_assert(i >= 0 && i < count);
    STBTT_assert(offsize >= 1 && offsize <= 4);
    stbtt__buf_skip(&b, i * offsize);
    start = stbtt__buf_get(&b, offsize);
    end = stbtt__buf_get(&b, offsize);
    return stbtt__buf_range(&b, 2 + (count + 1) * offsize + start,
                            end - start);
}

//////////////////////////////////////////////////////////////////////////
//
// accessors to parse data from file
//

// on platforms that don't allow misaligned reads, if we want to allow
// truetype fonts that aren't padded to alignment, define
// ALLOW_UNALIGNED_TRUETYPE

#define ttBYTE(p) (*(stbtt_uint8 *)(p))
#define ttCHAR(p) (*(stbtt_int8 *)(p))
#define ttFixed(p) ttLONG(p)

static stbtt_uint16 ttUSHORT(stbtt_uint8 *p)
{
    return p[0] * 256 + p[1];
}
static stbtt_int16 ttSHORT(stbtt_uint8 *p)
{
    return p[0] * 256 + p[1];
}
static stbtt_uint32 ttULONG(stbtt_uint8 *p)
{
    return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
}
static stbtt_int32 ttLONG(stbtt_uint8 *p)
{
    return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
}

#define stbtt_tag4(p, c0, c1, c2, c3)                                        \
    ((p)[0] == (c0) && (p)[1] == (c1) && (p)[2] == (c2) && (p)[3] == (c3))
#define stbtt_tag(p, str) stbtt_tag4(p, str[0], str[1], str[2], str[3])

static int stbtt__isfont(stbtt_uint8 *font)
{
    // check the version number
    if (stbtt_tag4(font, '1', 0, 0, 0))
        return 1; // TrueType 1
    if (stbtt_tag(font, "typ1"))
        return 1; // TrueType with type 1 font -- we don't support this!
    if (stbtt_tag(font, "OTTO"))
        return 1; // OpenType with CFF
    if (stbtt_tag4(font, 0, 1, 0, 0))
        return 1; // OpenType 1.0
    if (stbtt_tag(font, "true"))
        return 1; // Apple specification for TrueType fonts
    return 0;
}

// @OPTIMIZE: binary search
static stbtt_uint32 stbtt__find_table(stbtt_uint8 *data,
                                      stbtt_uint32 fontstart, const char *tag)
{
    stbtt_int32 num_tables = ttUSHORT(data + fontstart + 4);
    stbtt_uint32 tabledir = fontstart + 12;
    stbtt_int32 i;
    for (i = 0; i < num_tables; ++i) {
        stbtt_uint32 loc = tabledir + 16 * i;
        if (stbtt_tag(data + loc + 0, tag))
            return ttULONG(data + loc + 8);
    }
    return 0;
}

static int
stbtt_GetFontOffsetForIndex_internal(unsigned char *font_collection,
                                     int index)
{
    // if it's just a font, there's only one valid index
    if (stbtt__isfont(font_collection))
        return index == 0 ? 0 : -1;

    // check if it's a TTC
    if (stbtt_tag(font_collection, "ttcf")) {
        // version 1?
        if (ttULONG(font_collection + 4) == 0x00010000 ||
            ttULONG(font_collection + 4) == 0x00020000) {
            stbtt_int32 n = ttLONG(font_collection + 8);
            if (index >= n)
                return -1;
            return ttULONG(font_collection + 12 + index * 4);
        }
    }
    return -1;
}

#if 0
static int stbtt_GetNumberOfFonts_internal(unsigned char *font_collection)
{
    // if it's just a font, there's only one valid font
    if (stbtt__isfont(font_collection))
        return 1;

    // check if it's a TTC
    if (stbtt_tag(font_collection, "ttcf")) {
        // version 1?
        if (ttULONG(font_collection + 4) == 0x00010000 ||
            ttULONG(font_collection + 4) == 0x00020000) {
            return ttLONG(font_collection + 8);
        }
    }
    return 0;
}
#endif

static stbtt__buf stbtt__get_subrs(stbtt__buf cff, stbtt__buf fontdict)
{
    stbtt_uint32 subrsoff = 0, private_loc[2] = {0, 0};
    stbtt__buf pdict;
    stbtt__dict_get_ints(&fontdict, 18, 2, private_loc);
    if (!private_loc[1] || !private_loc[0])
        return stbtt__new_buf(NULL, 0);
    pdict = stbtt__buf_range(&cff, private_loc[1], private_loc[0]);
    stbtt__dict_get_ints(&pdict, 19, 1, &subrsoff);
    if (!subrsoff)
        return stbtt__new_buf(NULL, 0);
    stbtt__buf_seek(&cff, private_loc[1] + subrsoff);
    return stbtt__cff_get_index(&cff);
}

static int stbtt_InitFont_internal(stbtt_fontinfo *info, unsigned char *data,
                                   int fontstart)
{
    stbtt_uint32 cmap, t;
    stbtt_int32 i, numTables;

    info->data = data;
    info->fontstart = fontstart;
    info->cff = stbtt__new_buf(NULL, 0);

    cmap = stbtt__find_table(data, fontstart, "cmap");       // required
    info->loca = stbtt__find_table(data, fontstart, "loca"); // required
    info->head = stbtt__find_table(data, fontstart, "head"); // required
    info->glyf = stbtt__find_table(data, fontstart, "glyf"); // required
    info->hhea = stbtt__find_table(data, fontstart, "hhea"); // required
    info->hmtx = stbtt__find_table(data, fontstart, "hmtx"); // required
    //info->kern = stbtt__find_table(data, fontstart, "kern"); // not required
    //info->gpos = stbtt__find_table(data, fontstart, "GPOS"); // not required

    if (!cmap || !info->head || !info->hhea || !info->hmtx)
        return 0;
    if (info->glyf) {
        // required for truetype
        if (!info->loca)
            return 0;
    } else {
        // initialization for CFF / Type2 fonts (OTF)
        stbtt__buf b, topdict, topdictidx;
        stbtt_uint32 cstype = 2, charstrings = 0, fdarrayoff = 0,
                     fdselectoff = 0;
        stbtt_uint32 cff;

        cff = stbtt__find_table(data, fontstart, "CFF ");
        if (!cff)
            return 0;

        info->fontdicts = stbtt__new_buf(NULL, 0);
        info->fdselect = stbtt__new_buf(NULL, 0);

        // @TODO this should use size from table (not 512MB)
        info->cff = stbtt__new_buf(data + cff, 512 * 1024 * 1024);
        b = info->cff;

        // read the header
        stbtt__buf_skip(&b, 2);
        stbtt__buf_seek(&b, stbtt__buf_get8(&b)); // hdrsize

        // @TODO the name INDEX could list multiple fonts,
        // but we just use the first one.
        stbtt__cff_get_index(&b); // name INDEX
        topdictidx = stbtt__cff_get_index(&b);
        topdict = stbtt__cff_index_get(topdictidx, 0);
        stbtt__cff_get_index(&b); // string INDEX
        info->gsubrs = stbtt__cff_get_index(&b);

        stbtt__dict_get_ints(&topdict, 17, 1, &charstrings);
        stbtt__dict_get_ints(&topdict, 0x100 | 6, 1, &cstype);
        stbtt__dict_get_ints(&topdict, 0x100 | 36, 1, &fdarrayoff);
        stbtt__dict_get_ints(&topdict, 0x100 | 37, 1, &fdselectoff);
        info->subrs = stbtt__get_subrs(b, topdict);

        // we only support Type 2 charstrings
        if (cstype != 2)
            return 0;
        if (charstrings == 0)
            return 0;

        if (fdarrayoff) {
            // looks like a CID font
            if (!fdselectoff)
                return 0;
            stbtt__buf_seek(&b, fdarrayoff);
            info->fontdicts = stbtt__cff_get_index(&b);
            info->fdselect =
                stbtt__buf_range(&b, fdselectoff, b.size - fdselectoff);
        }

        stbtt__buf_seek(&b, charstrings);
        info->charstrings = stbtt__cff_get_index(&b);
    }

    t = stbtt__find_table(data, fontstart, "maxp");
    if (t)
        info->numGlyphs = ttUSHORT(data + t + 4);
    else
        info->numGlyphs = 0xffff;

    info->svg = -1;

    // find a cmap encoding table we understand *now* to avoid searching
    // later. (todo: could make this installable)
    // the same regardless of glyph.
    numTables = ttUSHORT(data + cmap + 2);
    info->index_map = 0;
    for (i = 0; i < numTables; ++i) {
        stbtt_uint32 encoding_record = cmap + 4 + 8 * i;
        // find an encoding we understand:
        switch (ttUSHORT(data + encoding_record)) {
        case STBTT_PLATFORM_ID_MICROSOFT:
            switch (ttUSHORT(data + encoding_record + 2)) {
            case STBTT_MS_EID_UNICODE_BMP:
            case STBTT_MS_EID_UNICODE_FULL:
                // MS/Unicode
                info->index_map = cmap + ttULONG(data + encoding_record + 4);
                break;
            }
            break;
        case STBTT_PLATFORM_ID_UNICODE:
            // Mac/iOS has these
            // all the encodingIDs are unicode, so we don't bother to check it
            info->index_map = cmap + ttULONG(data + encoding_record + 4);
            break;
        }
    }
    if (info->index_map == 0)
        return 0;

    info->indexToLocFormat = ttUSHORT(data + info->head + 50);
    return 1;
}

STBTT_DEF int stbtt_FindGlyphIndex(const stbtt_fontinfo *info,
                                   int unicode_codepoint)
{
    stbtt_uint8 *data = info->data;
    stbtt_uint32 index_map = info->index_map;

    stbtt_uint16 format = ttUSHORT(data + index_map + 0);
    if (format == 0) { // apple byte encoding
        stbtt_int32 bytes = ttUSHORT(data + index_map + 2);
        if (unicode_codepoint < bytes - 6)
            return ttBYTE(data + index_map + 6 + unicode_codepoint);
        return 0;
    } else if (format == 6) {
        stbtt_uint32 first = ttUSHORT(data + index_map + 6);
        stbtt_uint32 count = ttUSHORT(data + index_map + 8);
        if ((stbtt_uint32)unicode_codepoint >= first &&
            (stbtt_uint32)unicode_codepoint < first + count)
            return ttUSHORT(data + index_map + 10 +
                            (unicode_codepoint - first) * 2);
        return 0;
    } else if (format == 2) {
        STBTT_assert(
            0); // @TODO: high-byte mapping for japanese/chinese/korean
        return 0;
    } else if (format == 4) { // standard mapping for windows fonts: binary
                              // search collection of ranges
        stbtt_uint16 segcount = ttUSHORT(data + index_map + 6) >> 1;
        stbtt_uint16 searchRange = ttUSHORT(data + index_map + 8) >> 1;
        stbtt_uint16 entrySelector = ttUSHORT(data + index_map + 10);
        stbtt_uint16 rangeShift = ttUSHORT(data + index_map + 12) >> 1;

        // do a binary search of the segments
        stbtt_uint32 endCount = index_map + 14;
        stbtt_uint32 search = endCount;

        if (unicode_codepoint > 0xffff)
            return 0;

        // they lie from endCount .. endCount + segCount
        // but searchRange is the nearest power of two, so...
        if (unicode_codepoint >= ttUSHORT(data + search + rangeShift * 2))
            search += rangeShift * 2;

        // now decrement to bias correctly to find smallest
        search -= 2;
        while (entrySelector) {
            stbtt_uint16 end;
            searchRange >>= 1;
            end = ttUSHORT(data + search + searchRange * 2);
            if (unicode_codepoint > end)
                search += searchRange * 2;
            --entrySelector;
        }
        search += 2;

        {
            stbtt_uint16 offset, start, last;
            stbtt_uint16 item = (stbtt_uint16)((search - endCount) >> 1);

            start =
                ttUSHORT(data + index_map + 14 + segcount * 2 + 2 + 2 * item);
            last = ttUSHORT(data + endCount + 2 * item);
            if (unicode_codepoint < start || unicode_codepoint > last)
                return 0;

            offset =
                ttUSHORT(data + index_map + 14 + segcount * 6 + 2 + 2 * item);
            if (offset == 0)
                return (stbtt_uint16)(unicode_codepoint +
                                      ttSHORT(data + index_map + 14 +
                                              segcount * 4 + 2 + 2 * item));

            return ttUSHORT(data + offset + (unicode_codepoint - start) * 2 +
                            index_map + 14 + segcount * 6 + 2 + 2 * item);
        }
    } else if (format == 12 || format == 13) {
        stbtt_uint32 ngroups = ttULONG(data + index_map + 12);
        stbtt_int32 low, high;
        low = 0;
        high = (stbtt_int32)ngroups;
        // Binary search the right group.
        while (low < high) {
            stbtt_int32 mid = low + ((high - low) >>
                                     1); // rounds down, so low <= mid < high
            stbtt_uint32 start_char =
                ttULONG(data + index_map + 16 + mid * 12);
            stbtt_uint32 end_char =
                ttULONG(data + index_map + 16 + mid * 12 + 4);
            if ((stbtt_uint32)unicode_codepoint < start_char)
                high = mid;
            else if ((stbtt_uint32)unicode_codepoint > end_char)
                low = mid + 1;
            else {
                stbtt_uint32 start_glyph =
                    ttULONG(data + index_map + 16 + mid * 12 + 8);
                if (format == 12)
                    return start_glyph + unicode_codepoint - start_char;
                else // format == 13
                    return start_glyph;
            }
        }
        return 0; // not found
    }
    // @TODO
    STBTT_assert(0);
    return 0;
}

STBTT_DEF void stbtt_GetGlyphHMetrics(const stbtt_fontinfo *info,
                                      int glyph_index, int *advanceWidth,
                                      int *leftSideBearing)
{
    stbtt_uint16 numOfLongHorMetrics = ttUSHORT(info->data + info->hhea + 34);
    if (glyph_index < numOfLongHorMetrics) {
        if (advanceWidth)
            *advanceWidth =
                ttSHORT(info->data + info->hmtx + 4 * glyph_index);
        if (leftSideBearing)
            *leftSideBearing =
                ttSHORT(info->data + info->hmtx + 4 * glyph_index + 2);
    } else {
        if (advanceWidth)
            *advanceWidth = ttSHORT(info->data + info->hmtx +
                                    4 * (numOfLongHorMetrics - 1));
        if (leftSideBearing)
            *leftSideBearing =
                ttSHORT(info->data + info->hmtx + 4 * numOfLongHorMetrics +
                        2 * (glyph_index - numOfLongHorMetrics));
    }
}

STBTT_DEF void stbtt_GetCodepointHMetrics(const stbtt_fontinfo *info,
                                          int codepoint, int *advanceWidth,
                                          int *leftSideBearing)
{
    stbtt_GetGlyphHMetrics(info, stbtt_FindGlyphIndex(info, codepoint),
                           advanceWidth, leftSideBearing);
}

STBTT_DEF void stbtt_GetFontVMetrics(const stbtt_fontinfo *info, int *ascent,
                                     int *descent, int *lineGap)
{
    if (ascent)
        *ascent = ttSHORT(info->data + info->hhea + 4);
    if (descent)
        *descent = ttSHORT(info->data + info->hhea + 6);
    if (lineGap)
        *lineGap = ttSHORT(info->data + info->hhea + 8);
}

#if 0
STBTT_DEF int stbtt_GetFontVMetricsOS2(const stbtt_fontinfo *info,
                                       int *typoAscent, int *typoDescent,
                                       int *typoLineGap)
{
    int tab = stbtt__find_table(info->data, info->fontstart, "OS/2");
    if (!tab)
        return 0;
    if (typoAscent)
        *typoAscent = ttSHORT(info->data + tab + 68);
    if (typoDescent)
        *typoDescent = ttSHORT(info->data + tab + 70);
    if (typoLineGap)
        *typoLineGap = ttSHORT(info->data + tab + 72);
    return 1;
}
#endif
STBTT_DEF void stbtt_GetFontBoundingBox(const stbtt_fontinfo *info, int *x0,
                                        int *y0, int *x1, int *y1)
{
    *x0 = ttSHORT(info->data + info->head + 36);
    *y0 = ttSHORT(info->data + info->head + 38);
    *x1 = ttSHORT(info->data + info->head + 40);
    *y1 = ttSHORT(info->data + info->head + 42);
}

STBTT_DEF float stbtt_ScaleForPixelHeight(const stbtt_fontinfo *info,
                                          float height)
{
    int fheight = ttSHORT(info->data + info->hhea + 4) -
                  ttSHORT(info->data + info->hhea + 6);
    return (float)height / fheight;
}

STBTT_DEF int stbtt_GetFontOffsetForIndex(const unsigned char *data,
                                          int index)
{
    return stbtt_GetFontOffsetForIndex_internal((unsigned char *)data, index);
}

#if 0
STBTT_DEF int stbtt_GetNumberOfFonts(const unsigned char *data)
{
    return stbtt_GetNumberOfFonts_internal((unsigned char *)data);
}
#endif
STBTT_DEF int stbtt_InitFont(stbtt_fontinfo *info, const unsigned char *data,
                             int offset)
{
    return stbtt_InitFont_internal(info, (unsigned char *)data, offset);
}

#ifndef PDFGEN_H
#include <stdio.h>

unsigned char ttf_buffer[1 << 25];

int main(int argc, char **argv)
{
    stbtt_fontinfo font;
    // unsigned char *bitmap;
    // int w,h,i,j,c = (argc > 1 ? atoi(argv[1]) : 'a'), s = (argc > 2 ?
    // atoi(argv[2]) : 20);
    const char *filename = "./data/Blaec-DOKe1.ttf";
    int codepoint = 'H';

    (void)argc;
    (void)argv;
    FILE *fp = fopen(filename, "rb");
    if (!fp)
        return -1;

    fread(ttf_buffer, 1, sizeof(ttf_buffer), fp);
    fclose(fp);

    stbtt_InitFont(&font, ttf_buffer,
                   stbtt_GetFontOffsetForIndex(ttf_buffer, 0));
#if 1
    float scale = stbtt_ScaleForPixelHeight(&font, 12);
    printf("scale: %f\n", scale);
#endif
#if 1
    int x0, y0, x1, y1;
    stbtt_GetFontBoundingBox(&font, &x0, &y0, &x1, &y1);
    printf("bbox: %d,%d %d,%d\n", x0, y0, x1, y1);
    printf("/2 %f,%f %f,%f\n", 0.5 * x0, 0.5 * y0, 0.5 * x1, 0.5 * y1);
#endif
#if 1
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);
    printf("ascent: %i descent: %i lineGap: %i\n", ascent, descent, lineGap);
    printf("/2: %f %f %f\n", 0.5 * ascent, 0.5 * descent, 0.5 * lineGap);
#endif
#if 1
    int advanceWidth, leftSideBearing;
    stbtt_GetCodepointHMetrics(&font, codepoint, &advanceWidth,
                               &leftSideBearing);
    printf("codepoint '%c' advanceWidth: %i leftSideBearing: %i\n", codepoint, advanceWidth,
           leftSideBearing);
#endif

    return 0;
}
#endif
