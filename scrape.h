#ifndef NANO_EXPORTER_SCRAPE_H_
#define NANO_EXPORTER_SCRAPE_H_ 1

#include <stdbool.h>
#include <stddef.h>

/** Opaque type to represent the scrape server. */
typedef struct scrape_server scrape_server;

/** Opaque type to represent an ongoing scrape request. */
typedef struct scrape_req scrape_req;

/** Function type for a scrape server callback. */
typedef void scrape_handler(scrape_req *req, void *ctx);

/** Sets up a scrape server listening at the given port. */
scrape_server *scrape_listen(const char *port);

/** Enters a loop serving scrape requests using the provided handler. */
void scrape_serve(scrape_server *server, scrape_handler *handler, void *handler_ctx);

/** Closes the scrape server sockets and frees any resources. */
void scrape_close(scrape_server *server);

/**
 * Writes a metric value as a response to a scrape.
 *
 * The \p labels parameter can be `NULL` if no extra labels need to be attached. If not null, it
 * should point at the first element of an array of 2-element arrays of pointers, where the first
 * element of each pair is the label name, and the second the label value. A pair of null pointers
 * terminates the label array.
 *
 * Returns `false` if setting up the server failed, otherwise does not return.
 */
void scrape_write(scrape_req *req, const char *metric, const char *(*labels)[2], double value);

/**
 * Writes raw data to the scrape response.
 *
 * It's the callers responsibility to make sure it writes syntactically valid metric data.
 */
void scrape_write_raw(scrape_req *req, const void *buf, size_t len);

#endif // NANO_EXPORTER_SCRAPE_H_
