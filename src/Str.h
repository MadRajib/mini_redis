#ifndef _STR_T
#define _STR_T

#ifndef STRDEF
#define STRDEF
#endif /*end of STRDEF*/

#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

typedef struct {
    size_t count;
    const char *data;
} Str_t;

STRDEF Str_t * rtrim(Str_t *str);
STRDEF Str_t * ltrim(Str_t *str);
STRDEF Str_t * trim(Str_t *str);

STRDEF Str_t * rtrim(Str_t *str) {
    size_t i = 0;
    while (i < str->count && str->data[i] == ' ')
        i++;

    return NULL;
}

STRDEF Str_t * ltrim(Str_t *str) {
    size_t i = 0;
    while (i < str->count && str->data[str->count - 1 - i] == ' ')
        i++;
    
    return NULL;
}

STRDEF Str_t * trim(Str_t *str) {
    return rtrim(ltrim(str));
}

#endif /*End of _STR_T*/
