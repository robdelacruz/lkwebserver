#ifndef LKLIB_H
#define LKLIB_H

typedef struct {
    char *s;
    size_t s_len;
    size_t s_size;
} lkstr_s;

lkstr_s *lkstr_new(char *s);
lkstr_s *lkstr_size_new(size_t size);
void lkstr_free(lkstr_s *lkstr); 

void lkstr_assign(lkstr_s *lks, char *s);
void lkstr_sprintf(lkstr_s *lkstr, char *fmt, ...);
void lkstr_append(lkstr_s *lkstr, char *s);
int lkstr_equal(lkstr_s *lks1, lkstr_s *lks2);

#endif

