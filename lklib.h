#ifndef LKLIB_H
#define LKLIB_H

void lk_print_err(char *s);
void lk_exit_err(char *s);
char *lk_vasprintf(char *fmt, va_list args);

/*** LKString ***/
typedef struct {
    char *s;
    size_t s_len;
    size_t s_size;
} LKString;

typedef struct lkstringlist LKStringList;

LKString *lk_string_new(char *s);
LKString *lk_string_size_new(size_t size);
void lk_string_free(LKString *lks); 
void lk_string_voidp_free(void *plkstr); 

void lk_string_assign(LKString *lks, char *s);
void lk_string_assign_sprintf(LKString *lks, char *fmt, ...);
void lk_string_append(LKString *lks, char *s);
void lk_string_append_sprintf(LKString *lks, char *fmt, ...);
void lk_string_append_char(LKString *lks, char c);

int lk_string_sz_equal(LKString *lks, char *s);
int lk_string_equal(LKString *lks1, LKString *lks2);
int lk_string_starts_with(LKString *lks, char *s);
int lk_string_ends_with(LKString *lks, char *s);

void lk_string_trim(LKString *lks);
void lk_string_chop_start(LKString *lks, char *s);
void lk_string_chop_end(LKString *lks, char *s);
LKStringList *lk_string_split(LKString *lks, char *delim);


/*** LKStringMap ***/
typedef struct {
    LKString *k;
    void *v;
} LKStringMapItem;

typedef struct {
    LKStringMapItem *items;
    size_t items_len;
    size_t items_size;
    void (*freev_func)(void*); // function ptr to free v
} LKStringMap;

LKStringMap *lk_stringmap_new();
LKStringMap *lk_stringmap_funcs_new(void (*freefunc)(void*));
void lk_stringmap_free(LKStringMap *sm);
void lk_stringmap_set(LKStringMap *sm, char *ks, void *v);
void *lk_stringmap_get(LKStringMap *sm, char *ks);
void lk_stringmap_remove(LKStringMap *sm, char *ks);


/*** LKBuffer - Dynamic bytes buffer ***/
typedef struct {
    char *bytes;        // bytes buffer
    size_t bytes_cur;   // index to current buffer position
    size_t bytes_len;   // length of buffer
    size_t bytes_size;  // capacity of buffer in bytes
} LKBuffer;

LKBuffer *lk_buffer_new(size_t bytes_size);
void lk_buffer_free(LKBuffer *buf);
void lk_buffer_clear(LKBuffer *buf);
int lk_buffer_append(LKBuffer *buf, char *bytes, size_t len);
int lk_buffer_append_sz(LKBuffer *buf, char *s);
void lk_buffer_append_sprintf(LKBuffer *buf, const char *fmt, ...);


/*** LKStringList ***/
typedef struct lkstringlist {
    LKString **items;
    size_t items_len;
    size_t items_size;
} LKStringList;

LKStringList *lk_stringlist_new();
void lk_stringlist_free(LKStringList *sl);
void lk_stringlist_append_lkstring(LKStringList *sl, LKString *lks);
void lk_stringlist_append(LKStringList *sl, char *s);
void lk_stringlist_append_sprintf(LKStringList *sl, const char *fmt, ...);
LKString *lk_stringlist_get(LKStringList *sl, unsigned int i);
void lk_stringlist_remove(LKStringList *sl, unsigned int i);

#endif

