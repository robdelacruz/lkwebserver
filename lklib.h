#ifndef LKLIB_H
#define LKLIB_H

// Predefined buffer sizes.
// Sample use: char buf[LK_BUFSIZE_SMALL]
#define LK_BUFSIZE_SMALL 512
#define LK_BUFSIZE_MEDIUM 1024
#define LK_BUFSIZE_LARGE 2048
#define LK_BUFSIZE_XL 4096
#define LK_BUFSIZE_XXL 8192

#ifdef DEBUGALLOC
#define malloc(size) (lk_malloc((size), "malloc"))
#define realloc(p, size) (lk_realloc((p), (size), "realloc"))
#define free(p) (lk_free((p))) 
#define strdup(s) (lk_strdup((s), "strdup"))
#define strndup(s, n) (lk_strndup((s), (n), "strndup"))
#endif

void lk_print_err(char *s);
void lk_exit_err(char *s);
char *lk_vasprintf(char *fmt, va_list args);
int is_empty_line(char *s);
int ends_with_newline(char *s);
int lk_popen3(char *cmd, int *fd_in, int *fd_out, int *fd_err);

// Return localtime in server format: 11/Mar/2023 14:05:46
// Usage:
// char timestr[TIME_STRING_SIZE];
// get_localtime_string(timestr, sizeof(timestr))
#define TIME_STRING_SIZE 25
void get_localtime_string(char *time_str, size_t time_str_len);

void lk_alloc_init();
void *lk_malloc(size_t size, char *label);
void *lk_realloc(void *p, size_t size, char *label);
void lk_free(void *p);
char *lk_strdup(const char *s, char *label);
char *lk_strndup(const char *s, size_t n, char *label);
void lk_print_allocitems();
// vasprintf(&ps, fmt, args); //$$ lk_vasprintf()?

// Return matching item in lookup table given testk.
// tbl is a null-terminated array of char* key-value pairs
// Ex. tbl = {"key1", "val1", "key2", "val2", "key3", "val3", NULL};
// where key1/val1, key2/val2, key3/val3 are the key-value pairs.
void **lk_lookup(void **tbl, char *testk);

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

void lk_string_prepend(LKString *lks, char *s);

int lk_string_sz_equal(LKString *lks, char *s);
int lk_string_equal(LKString *lks1, LKString *lks2);
int lk_string_starts_with(LKString *lks, char *s);
int lk_string_ends_with(LKString *lks, char *s);

void lk_string_trim(LKString *lks);
void lk_string_chop_start(LKString *lks, char *s);
void lk_string_chop_end(LKString *lks, char *s);
LKStringList *lk_string_split(LKString *lks, char *delim);
void lk_string_split_assign(LKString *s, char *delim, LKString *k, LKString *v);
void sz_string_split_assign(char *s, char *delim, LKString *k, LKString *v);


/*** LKStringTable ***/
typedef struct {
    LKString *k;
    LKString *v;
} LKStringTableItem;

typedef struct {
    LKStringTableItem *items;
    size_t items_len;
    size_t items_size;
} LKStringTable;

LKStringTable *lk_stringtable_new();
void lk_stringtable_free(LKStringTable *sm);
void lk_stringtable_set(LKStringTable *sm, char *ks, char *v);
char *lk_stringtable_get(LKStringTable *sm, char *ks);
void lk_stringtable_remove(LKStringTable *sm, char *ks);


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


/*** LKBuffer - Dynamic bytes buffer ***/
typedef struct {
    char *bytes;        // bytes buffer
    size_t bytes_cur;   // index to current buffer position
    size_t bytes_len;   // length of buffer
    size_t bytes_size;  // capacity of buffer in bytes
} LKBuffer;

LKBuffer *lk_buffer_new(size_t bytes_size);
void lk_buffer_free(LKBuffer *buf);
void lk_buffer_resize(LKBuffer *buf, size_t bytes_size);
void lk_buffer_clear(LKBuffer *buf);
int lk_buffer_append(LKBuffer *buf, char *bytes, size_t len);
int lk_buffer_append_sz(LKBuffer *buf, char *s);
void lk_buffer_append_sprintf(LKBuffer *buf, const char *fmt, ...);
size_t lk_buffer_readline(LKBuffer *buf, char *dst, size_t dst_len);


/*** LKRefList ***/
typedef struct {
    void **items;
    size_t items_len;
    size_t items_size;
    size_t items_cur;
} LKRefList;

LKRefList *lk_reflist_new();
void lk_reflist_free(LKRefList *l);
void lk_reflist_append(LKRefList *l, void *p);
void *lk_reflist_get(LKRefList *l, unsigned int i);
void *lk_reflist_get_cur(LKRefList *l);
void lk_reflist_remove(LKRefList *l, unsigned int i);
void lk_reflist_clear(LKRefList *l);

#endif

