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

#ifndef NANO_EXPORTER_SCRAPE_H_
#define NANO_EXPORTER_SCRAPE_H_ 1

#include <stdbool.h>
#include <stddef.h>

/** Opaque type to represent the scrape server. */
typedef struct scrape_server scrape_server;

/** Opaque type to represent an ongoing scrape request. */
typedef struct scrape_req scrape_req;

/** Interface type for implementing a collector that can be scraped. */
struct collector {
  const char *name;
  void (*collect)(scrape_req *req, void *ctx);
  void *(*init)(int argc, char *argv[]);
  bool has_args;
};

/** Sets up a scrape server listening at the given port. */
scrape_server *scrape_listen(const char *port);

/** Enters a loop serving scrape requests of the provided collectors. */
void scrape_serve(scrape_server *server, unsigned ncoll, const struct collector *coll[], void *coll_ctx[]);

/** Closes the scrape server sockets and frees any resources. */
void scrape_close(scrape_server *server);

/** Container for label keys and values. */
struct label {
  char *key;
  char *value;
};

#define LABEL_END ((struct label){ .key = 0, .value = 0 })

#define LABEL_LIST(...) ((struct label[]){ __VA_ARGS__, LABEL_END })

/**
 * Writes a metric value as a response to a scrape.
 *
 * The \p labels parameter can be `NULL` if no extra labels need to be
 * attached. If not null, it should point at the first element of an
 * array of `struct label` objects, terminated by a sentinel value
 * with a null pointer as the key.
 *
 * Returns `false` if setting up the server failed, otherwise does not return.
 */
void scrape_write(scrape_req *req, const char *metric, const struct label *labels, double value);

/**
 * Writes raw data to the scrape response.
 *
 * It's the callers responsibility to make sure it writes syntactically valid metric data.
 */
void scrape_write_raw(scrape_req *req, const void *buf, size_t len);

#endif // NANO_EXPORTER_SCRAPE_H_
