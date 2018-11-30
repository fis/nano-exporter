#ifndef NANO_EXPORTER_UTIL_H_
#define NANO_EXPORTER_UTIL_H_ 1

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// character buffers

typedef struct cbuf cbuf;

cbuf *cbuf_alloc(size_t initial_size, size_t max_size);
void cbuf_reset(cbuf *buf);
size_t cbuf_len(cbuf *buf);
void cbuf_put(cbuf *buf, const void *src, size_t len);
void cbuf_puts(cbuf *buf, const char *src);
void cbuf_putc(cbuf *buf, int c);
void cbuf_putf(cbuf *buf, const char *fmt, ...);
const char *cbuf_get(struct cbuf *buf, size_t *len);
/** Compares the contents of \p buf to the string in \p other, in shortlex order. */
int cbuf_cmp(cbuf *buf, const char *other);

// string lists

struct slist {
  struct slist *next;
  char data[];
};

struct slist *slist_split(const char *str, const char *delim);
struct slist **slist_append(struct slist **prev, const char *str);
struct slist *slist_prepend(struct slist *list, const char *str);
bool slist_contains(const struct slist *list, const char *key);

// miscellaneous utilities

/** Calls `malloc(size)` and aborts if memory allocation failed. */
void *must_malloc(size_t size);

/** Calls `realloc(ptr, size)` and aborts if memory allocation failed. */
void *must_realloc(void *ptr, size_t size);

/** Calls `strdup(src)` and aborts if memory allocation failed. */
char *must_strdup(const char *src);

/**
 * Reads a full line from \p stream into buffer \p s of size \p size.
 *
 * This function is otherwise identical to standard `fgets`, except that if a full line did not fit
 * in the input buffer, characters are discarded from the stream up to and including the next
 * newline character. If the file ends before a newline is encountered, a null pointer is
 * required. This way this function always reads complete lines, which are just truncated if they
 * don't fit in the buffer.
 */
char *fgets_line(char *s, int size, FILE *stream);

/**
 * Fully writes the contents of \p buf (\p len bytes) into file descriptor \p fd.
 *
 * If not all bytes could be written, this function just tries again. It returns 0 on success, or -1
 * if any of the `write` calls failed with an error.
 */
int write_all(int fd, const void *buf, size_t len);

#endif // NANO_EXPORTER_UTIL_H_
