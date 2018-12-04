/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NANO_EXPORTER_UTIL_H_
#define NANO_EXPORTER_UTIL_H_ 1

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// character buffers

/** Opaque type for an internally maintained character (byte) buffer. */
typedef struct cbuf cbuf;

/** Allocates a new buffer with \p initial_size, growing up to \p max_size. */
cbuf *cbuf_alloc(size_t initial_size, size_t max_size);
/** Clears the contents of the buffer, resetting it to the empty string. */
void cbuf_reset(cbuf *buf);
/** Returns the current length of the buffer contents. */
size_t cbuf_len(cbuf *buf);
/** Appends \p len bytes from address \p src to the buffer \p buf. */
void cbuf_put(cbuf *buf, const void *src, size_t len);
/** Appends the null-terminated string at \p src to the buffer \p buf. */
void cbuf_puts(cbuf *buf, const char *src);
/** Appends a single byte \p c to \p buf. */
void cbuf_putc(cbuf *buf, int c);
/** Appends a formatted string to \p buf. */
void cbuf_putf(cbuf *buf, const char *fmt, ...);
/**
 * Returns the contents of \p buf, writing the length to \p len.
 *
 * There may not be a terminating '\0' byte after the contents. And even if there is, the returned
 * length will not include it.
 */
const char *cbuf_get(struct cbuf *buf, size_t *len);
/** Compares the contents of \p buf to the string in \p other, in shortlex order. */
int cbuf_cmp(cbuf *buf, const char *other);

// string lists

/** Type for a singly linked list of strings. */
struct slist {
  struct slist *next;
  char data[];
};

/** Splits \p str into a list of strings, using \p delim as the delimiter. */
struct slist *slist_split(const char *str, const char *delim);
/**
 * Appends \p str to a string list.
 *
 * The address of the new element is written to the location pointed by \p prev, which should be
 * either the `next` pointer of the last element of an existing list, or (for an empty list) the
 * future pointer to a list head. The return value will point at the `next` pointer of the newly
 * allocated element, so code like the following can be used to iteratively construct a list:
 *
 *     struct slist *list = 0;
 *     struct slist **prev = &list;
 *     while (...) {
 *       // do something to get a string 'str'
 *       prev = slist_append(prev, str);
 *     }
 */
struct slist **slist_append(struct slist **prev, const char *str);
/** Prepends \p str to a string list, returning the new head node. */
struct slist *slist_prepend(struct slist *list, const char *str);
/** Returns `true` if \p list contains as element the string \p key. */
bool slist_contains(const struct slist *list, const char *key);
/**
 * Returns `true` if any element of \p list matches \p key.
 *
 * The strings must match exactly, with the exception that if the list elements ends in the '*'
 * character, any \p key string that starts with the prefix before the '*' will count as matching.
 */
bool slist_matches(const struct slist *list, const char *key);

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
