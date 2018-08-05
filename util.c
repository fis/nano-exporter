#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

// character buffers

struct cbuf {
  char *data;
  size_t len;
  size_t size;
  size_t max_size;
};

static bool cbuf_reserve(cbuf *buf, size_t len) {
  if (buf->len + len <= buf->size)
    return true;

  size_t new_size = buf->size;
  while (buf->len + len > new_size && new_size < buf->max_size)
    new_size *= 2;
  if (buf->len + len > new_size)
    return false;

  char *new_data = must_realloc(buf->data, new_size);
  buf->data = new_data;
  buf->size = new_size;
  return true;
}

cbuf *cbuf_alloc(size_t initial_size, size_t max_size) {
  cbuf *buf = must_malloc(sizeof *buf);
  buf->data = must_malloc(initial_size);
  buf->len = 0;
  buf->size = initial_size;
  buf->max_size = max_size;
  return buf;
}

void cbuf_reset(cbuf *buf) {
  buf->len = 0;
}

void cbuf_put(cbuf *buf, const void *src, size_t len) {
  if (!cbuf_reserve(buf, len))
    return;
  memcpy(buf->data + buf->len, src, len);
  buf->len += len;
}

void cbuf_puts(cbuf *buf, const char *src) {
  cbuf_put(buf, src, strlen(src));
}

void cbuf_putc(cbuf *buf, int c) {
  if (!cbuf_reserve(buf, 1))
    return;
  buf->data[buf->len++] = c;
}

void cbuf_putf(cbuf *buf, const char *fmt, ...) {
  va_list ap;

  int len = 0;
  va_start(ap, fmt);
  len = vsnprintf(0, 0, fmt, ap);
  va_end(ap);

  if (len > 0) {
    if (!cbuf_reserve(buf, len + 1))
      return;
    va_start(ap, fmt);
    vsnprintf(buf->data + buf->len, len + 1, fmt, ap);
    va_end(ap);
    buf->len += len;
  }
}

const char *cbuf_get(struct cbuf *buf, size_t *len) {
  *len = buf->len;
  return buf->data;
}

// string lists

struct slist *slist_split(const char *str, const char *delim) {
  struct slist *list = 0, **prev = &list, *next;

  while (*str) {
    size_t span = strcspn(str, delim);
    if (span == 0) {
      str++;
      continue;
    }

    next = must_malloc(sizeof *next + span + 1);
    memcpy(next->data, str, span);
    next->data[span] = '\0';
    *prev = next;
    prev = &next->next;

    str += span;
    while (*str && strchr(delim, *str))
      str++;
  }
  *prev = 0;

  return list;
}

struct slist **slist_append(struct slist **prev, const char *str) {
  size_t len = strlen(str);
  struct slist *new = *prev = must_malloc(sizeof *new + len + 1);
  new->next = 0;
  memcpy(new->data, str, len + 1);
  return &new->next;
}

struct slist *slist_prepend(struct slist *list, const char *str) {
  size_t len = strlen(str);
  struct slist *new = must_malloc(sizeof *new + len + 1);
  new->next = list;
  memcpy(new->data, str, len + 1);
  return new;
}

bool slist_contains(const struct slist *list, const char *key) {
  for (; list; list = list->next)
    if (strcmp(list->data, key) == 0)
      return true;
  return false;
}

// miscellaneous utilities

void *must_malloc(size_t size) {
  void *ptr = malloc(size);
  if (!ptr) {
    perror("malloc");
    abort();
  }
  return ptr;
}

void *must_realloc(void *ptr, size_t size) {
  void *new_ptr = realloc(ptr, size);
  if (!new_ptr) {
    perror("realloc");
    abort();
  }
  return new_ptr;
}

char *must_strdup(const char *src) {
  char *dst = strdup(src);
  if (!dst) {
    perror("strdup");
    abort();
  }
  return dst;
}

char *fgets_line(char *s, int size, FILE *stream) {
  s[size - 1] = '\0';
  char *got = fgets(s, size, stream);
  if (!got)
    return 0;
  if (s[size - 1] == '\0' || s[size - 1] == '\n')
    return got;

  int c;
  while ((c = fgetc(stream)) != EOF)
    if (c == '\n')
      return got;
  return 0;
}

int write_all(int fd, const void *buf_ptr, size_t len) {
  const char *buf = buf_ptr;

  while (len > 0) {
    ssize_t wrote = write(fd, buf, len);
    if (wrote <= 0)
      return -1;
    buf += wrote;
    len -= wrote;
  }

  return 0;
}
