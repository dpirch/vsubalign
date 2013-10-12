#include "subtitle.h"

#include "alloc.h"
#include "text.h"
#include "dict.h"
#include "subwords.h"

struct cuetime { unsigned start, end; };


static bool parse_cuetime(const char *line, struct cuetime *time)
{
    unsigned sh = 0, sm, ss, sms, eh = 0, em, es, ems;
    if (sscanf(line, "%u:%u%*1[.,]%u --> %u:%u%*1[.,]%u",
               &sm, &ss, &sms, &em, &es, &ems) == 6 ||
        sscanf(line, "%u:%u:%u%*1[.,]%u --> %u:%u:%u%*1[.,]%u",
               &sh, &sm, &ss, &sms, &eh, &em, &es, &ems) == 8) {
        time->start = sh * 3600000 + sm * 60000 + ss * 1000 + sms;
        time->end = eh * 3600000 + em * 60000 + es * 1000 + ems;
        return time->start < time->end;
    }
    return false;
}



static inline bool is_upper(char c) { return c >= 'A' && c <= 'Z'; }
static inline bool is_lower(char c) { return c >= 'a' && c <= 'z'; }
static inline char to_lower(char c) { return is_upper(c) ? c - 'A' + 'a' : c; }

static void blank_brackets(char *str, char open, char close)
{
    for (char *start = NULL, *p = str; *p; p++) {
        if (*p == open)
            start = p;
        else if (*p == close && start)
            memset(start, ' ', p - start + 1), start = NULL;
    }
}

static void process_wordstring(char *str, const struct cuetime *cuetime,
        struct swnodelist *wl, const struct dict *dict)
{
    for (;;) {
        struct dictword *word = dict_lookup(dict, str);
        if (word) {
            swnodelist_add(wl, word, cuetime->start, cuetime->end);
            break;
        } else {
            char *hyphen = strchr(str, '-');
            if (hyphen) {
                *hyphen = '\0';
                word = dict_lookup(dict, str); // possibly NULL
                swnodelist_add(wl, word, cuetime->start, cuetime->end);
                str = hyphen + 1;
                continue;
            }
            swnodelist_add(wl, NULL, cuetime->start, cuetime->end);
            break;
        }
    }
}


static void process_line(char *line, const struct cuetime *cuetime,
        struct swnodelist *wl, const struct dict *dict)
{
    // remove html tags and brackets with text for hearing impaired
    blank_brackets(line, '<', '>');
    blank_brackets(line, '[', ']');
    blank_brackets(line, '{', '}');
    blank_brackets(line, '(', ')');

    // remove upper-case text before colon (speaker name)
    for (char *p = line; *p; p++) {
        if (*p == ':') { memset(line, ' ', p - line + 1); break; }
        if (is_lower(*p)) break; // cancel if lower case before colon
    }

    // convert to lower case
    for (char *p = line; *p; p++)
        *p = to_lower(*p);

    // remove non-letter chars, except '-' and '\'' in the middle of words
    for (char *p = line; *p; p++) {
        if (!is_lower(*p) && !((*p == '-' || *p == '\'')
                && p > line && is_lower(p[-1]) && is_lower(p[1])))
            *p = ' ';
    }

    // split into words
    for (char *p = line;;) {
        while (*p == ' ') p++;
        if (!*p) break;
        char *end = strchr(p, ' ');
        if (end) *end = '\0';
        process_wordstring(p, cuetime, wl, dict);
        if (!end) break;
        p = end + 1;
    }
}

bool subtitle_readwords(const char *filename,
        struct swnodelist *wl, const struct dict *dict)
{
    linereader_t *lr = linereader_open(filename);
    if (!lr) return false;

    for (char *line; line = linereader_getline(lr), line;) {
        struct cuetime time;
        if (!parse_cuetime(line, &time)) continue;

        while (line = linereader_getline(lr), line && *line)
            process_line(line, &time, wl, dict);
    }

    bool success = !linereader_error(lr);
    linereader_close(lr);
    return success;
}





//struct rawword {
//};

/*




struct subword *read_subtitle_words(const char *filename,
        struct dict *stdict, const struct dict *srcdict, fixed_alloc_t *stwalloc)
{
    textreader_t *tr = textreader_open(filename);
    if (!tr) return NULL;

    const char *line;
    while ((line = textreader_getline(tr, NULL))) {
        subtitle_cuetime_t time;
        if (!parse_cuetime(line, &time)) continue;

        printf("new cue %u %u\n", time.start, time.end);

        while (((line = textreader_getline(tr, NULL))) && *line) {
            printf("line (%d): %s\n", *line, line);



        }
    }

    textreader_close(tr);
}

void subtitle_wordlist_init(subtitle_wordlist_t *list,
        linereader_t *lr, struct dict *dict)
{
    *list = (subtitle_wordlist_t){0};
    fixed_alloc_init(&list->allocator, sizeof *list->head, 256);
}

void subtitle_wordlist_destroy(subtitle_wordlist_t *list)
{
    fixed_alloc_destroy(&list->allocator);
}
*/

#if 0

struct SubtitleFile {
    //bool bom_found;
    size_t nsubs;
    struct Sub {
        long starttime, endtime;
        size_t nlines;
        struct Line { char *text; size_t length; } *lines;
    } *subs;
};




static bool read_sub(FILE *file, struct Sub *sub,
        struct Line **lines_buf, size_t *lines_alloc,
        char **text_buf, size_t *text_alloc)
{
    do if (!readline(file, text_buf, text_alloc, true, NULL)) return false;
    while (!parse_timings(*text_buf, &sub->starttime, &sub->endtime));


}

static bool read_bom(FILE *file)
{
    int c = getc(file);
    if (c != 239) { ungetc(c, file); return false; }
    else if (getc(file) == 187 && getc(file) == 191) return true;
    else { rewind(file); return false; }
}




#endif
