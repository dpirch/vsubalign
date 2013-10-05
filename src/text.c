#include "text.h"

#include <errno.h>


#define REPLACE 0xfffd // replacement char

const uint8_t utf8_bom[3] = { 0xef, 0xbb, 0xbf };

bool utf8_validate_string(const char *s)
{
    bool invalid = false;
    while (*s && !invalid)
        utf8_decode_char(&s, &invalid);
    return !invalid;
}

unsigned utf8_decode_char(const char **ptr, bool *invalid)
{
    uint8_t byte = *(const uint8_t*)*ptr;
    if (!byte) return 0;
    if (byte <= 0x7f) { ++*ptr; return byte; }

    int clen;
    unsigned cp;
    if ((byte & 0xe0) == 0xc0) cp = byte & 0x1f, clen = 2;
    else if ((byte & 0xf0) == 0xe0) cp = byte & 0x0f, clen = 3;
    else if ((byte & 0xf8) == 0xf0) cp = byte & 0x07, clen = 4;
    else { ++*ptr; goto invalid; }

    for (int i = 1; i < clen; i++)
        if ((((const uint8_t*)*ptr)[i] & 0xc0) == 0x80)
            cp = cp << 6 | (((const uint8_t*)*ptr)[i] & 0x3f);
        else { *ptr += i; goto invalid; }

    *ptr += clen;

    // overlong or invalid codepoint
    if (cp <= 0x7f || (cp <= 0x7ff && clen > 2) || (cp <= 0xffff && clen > 3) ||
            (cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff)
        goto invalid;

    return cp;

invalid:
    if (invalid) *invalid = true;
    return REPLACE;
}


static unsigned utf8_encode_char(unsigned cp, char buf[4])
{
    if (cp <= 0x7f) {
        buf[0] = cp;
        return 1;
    } else if (cp <= 0x7ff) {
        ((uint8_t*)buf)[0] = 0xc0 | cp >> 6;
        ((uint8_t*)buf)[1] = 0x80 | (0x3f & cp);
        return 2;
    } else if (cp <= 0xffff) {
        if (cp >= 0xd800 && cp <= 0xdfff) goto invalid;
        ((uint8_t*)buf)[0] = 0xe0 | cp >> 12;
        ((uint8_t*)buf)[1] = 0x80 | (0x3f & cp >> 6);
        ((uint8_t*)buf)[2] = 0x80 | (0x3f & cp);
        return 3;
    } else {
        if (cp > 0x10ffff) goto invalid;
        ((uint8_t*)buf)[0] = 0xf0 | cp >> 18;
        ((uint8_t*)buf)[1] = 0x80 | (0x3f & cp >> 12);
        ((uint8_t*)buf)[2] = 0x80 | (0x3f & cp >> 6);
        ((uint8_t*)buf)[3] = 0x80 | (0x3f & cp);
        return 4;
    }
invalid:
    return utf8_encode_char(REPLACE, buf);
}

void cp1252_to_utf8(char **bufp, size_t *bufcap, const char *src)
{
    static const uint16_t table[32] = {
        0x20ac, REPLACE, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
        0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, REPLACE, 0x017d, REPLACE,
        REPLACE, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
        0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, REPLACE, 0x017e, 0x0178,
    };

    size_t n = 0;
    for (;;) {
        if (n + 4 > *bufcap)
            *bufp = grow_array(*bufp, 1, bufcap, n + 3);

        uint8_t byte = *(const uint8_t*)src++;
        if (byte < 128)
            (*bufp)[n++] = byte;
        else
            n += utf8_encode_char(
                    byte >= 160 ? byte : table[byte - 128], *bufp + n);

        if (byte == 0) break;
    }
}



struct linereader {
    FILE *file;
    char *buffer;
    size_t bsize, bpos;
    bool done, error, readend;
    unsigned linenum;
    bool bom_found;
};

linereader_t *linereader_open(const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (!file) {
        error("Could not open '%s': %s", filename, strerror(errno));
        return NULL;
    }

    linereader_t *lr = xmalloc(sizeof *lr);
    *lr = (linereader_t){ .file = file };
    return lr;
}

void linereader_close(linereader_t *lr)
{
    free(lr->buffer);
    fclose(lr->file);
    free(lr);
}

/**
 * Shifts or enlarges the buffer and tries to read more data.
 */
static bool read_more(linereader_t *lr)
{
    if (lr->readend) return false;
    size_t rest = lr->bsize - lr->bpos;

    if (!lr->buffer) {
        lr->bsize = 4096;
        lr->buffer = xmalloc(lr->bsize);
    } else if (rest <= lr->bpos) {
        memcpy(lr->buffer, lr->buffer + lr->bpos, rest);
        lr->bpos = 0;
    } else if (lr->bpos == 0) {
        CHECK(lr->bsize < SIZE_MAX / 2);
        lr->bsize *= 2;
        lr->buffer = xrealloc(lr->buffer, lr->bsize);
    } else {
        CHECK(lr->bsize < SIZE_MAX / 2);
        lr->bsize *= 2;
        char *newbuf = xmalloc(lr->bsize);
        memcpy(newbuf, lr->buffer + lr->bpos, rest);
        free(lr->buffer);
        lr->buffer = newbuf;
        lr->bpos = 0;
    }

    size_t n = fread(lr->buffer + rest, 1, lr->bsize - rest, lr->file);
    if (n < lr->bsize - rest) {
        // eof: set bsize to end of available data.
        // the actual buffer if still larger and has space for terminating 0
        lr->bsize = rest + n;
        lr->readend = true;
        lr->error = ferror(lr->file);
        if (lr->error) { error("File read error"); return NULL; }
    }
    return n > 0;
}


char *linereader_getline(linereader_t *lr)
{
    if (lr->done) return NULL;

    char *line;
    size_t n = 0;

    for (;; n++) {
        if (lr->bpos + n >= lr->bsize && !read_more(lr)) {
            lr->done = true;
            line = lr->buffer + lr->bpos;
            break;
        }

        char c = lr->buffer[lr->bpos + n];

        if (c == '\r' || c == '\n') {
            bool windows_linebreak = c == '\r'
                    && (lr->bpos + n + 1 < lr->bsize || read_more(lr))
                    && lr->buffer[lr->bpos + n + 1] == '\n';
            line = lr->buffer + lr->bpos;
            lr->bpos = lr->bpos + n + (windows_linebreak ? 2 : 1);
            break;
        }

        if (c == 0) {
            lr->error = lr->done = true;
            error("Not a text file or unsupported encoding");
            return NULL;
        }

    }

    if (lr->linenum == 0 && n >= 3 && !memcmp(line, utf8_bom, 3))
        line += 3, n -= 3, lr->bom_found = true;

    if (n == 0 && lr->done) return NULL;

    line[n] = 0;
    lr->linenum++;
    return line;
}

bool linereader_error(const linereader_t *lr) { return lr->error; }

unsigned linereader_linenum(const linereader_t *lr) { return lr->linenum; }

bool linereader_bom_found(const linereader_t *lr) { return lr->bom_found; }
