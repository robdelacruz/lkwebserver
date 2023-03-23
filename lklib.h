#ifndef LKLIB_H
#define LKLIB_H

/*** lkstr_s ***/
typedef struct {
    char *s;
    size_t s_len;
    size_t s_size;
} lkstr_s;

lkstr_s *lkstr_new(char *s);
lkstr_s *lkstr_size_new(size_t size);
void lkstr_free(lkstr_s *lks); 
void lkstr_voidp_free(void *plkstr); 

void lkstr_assign(lkstr_s *lks, char *s);
void lkstr_sprintf(lkstr_s *lks, char *fmt, ...);
void lkstr_append(lkstr_s *lks, char *s);
int lkstr_sz_equal(lkstr_s *lks, char *s);
int lkstr_equal(lkstr_s *lks1, lkstr_s *lks2);


/*** lkstringmap_s ***/
typedef struct {
    lkstr_s *k;
    void *v;
} lkstringmap_item_s;

typedef struct {
    lkstringmap_item_s *items;
    size_t items_len;
    size_t items_size;
    void (*freev_func)(void*); // function ptr to free v
} lkstringmap_s;

lkstringmap_s *lkstringmap_new();
lkstringmap_s *lkstringmap_funcs_new(void (*freefunc)(void*));
void lkstringmap_free(lkstringmap_s *sm);
void lkstringmap_set(lkstringmap_s *sm, char *ks, void *v);
void *lkstringmap_get(lkstringmap_s *sm, char *ks);
void lkstringmap_remove(lkstringmap_s *sm, char *ks);


/*** lkbuf_s - Dynamic bytes buffer ***/
typedef struct {
    char *bytes;        // bytes buffer
    size_t bytes_cur;   // index to current buffer position
    size_t bytes_len;   // length of buffer
    size_t bytes_size;  // capacity of buffer in bytes
} lkbuf_s;

lkbuf_s *lkbuf_new(size_t bytes_size);
void lkbuf_free(lkbuf_s *buf);
int lkbuf_append(lkbuf_s *buf, char *bytes, size_t len);
void lkbuf_append_sprintf(lkbuf_s *buf, const char *fmt, ...);


/*** lkstringlist_s ***/
typedef struct {
    lkstr_s **items;
    size_t items_len;
    size_t items_size;
} lkstringlist_s;

lkstringlist_s *lkstringlist_new();
void lkstringlist_free(lkstringlist_s *sl);
void lkstringlist_append(lkstringlist_s *sl, char *s);
void lkstringlist_append_sprintf(lkstringlist_s *sl, const char *fmt, ...);
lkstr_s *lkstringlist_get(lkstringlist_s *sl, unsigned int i);
void lkstringlist_remove(lkstringlist_s *sl, unsigned int i);

#endif

