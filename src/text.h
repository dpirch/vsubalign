#ifndef TEXT_H_
#define TEXT_H_

#include "common.h"

extern const uint8_t utf8_bom[3];

unsigned utf8_decode_char(const char **ptr, bool *invalid);
bool utf8_validate_string(const char *s);

void cp1252_to_utf8(char **bufp, size_t *bufcap, const char *src);


typedef struct linereader linereader_t;

linereader_t *linereader_open(const char *filename);
void linereader_close(linereader_t *lr);
char *linereader_getline(linereader_t *lr);
bool linereader_error(const linereader_t *lr);
unsigned linereader_linenum(const linereader_t *lr);
bool linereader_bom_found(const linereader_t *lr);


#endif /* TEXT_H_ */
