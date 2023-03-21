#ifndef LKLIB_H
#define LKLIB_H

typedef struct {
    char *str;
    size_t str_len;
    size_t str_size;
} lkstr_s;

lkstr_s *lkstr_new(char *s);
lkstr_s *lkstr_size_new(size_t size);
void lkstr_free(lkstr_s *lkstr); 

void lkstr_sprintf(lkstr_s *lkstr, char *fmt, ...);
void lkstr_append(lkstr_s *lkstr, char *s);

#endif

