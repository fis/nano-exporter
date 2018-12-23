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

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "scrape.h"
#include "util.h"

#define BUF_INITIAL 1024
#define BUF_MAX 65536

#define MAX_LISTEN_SOCKETS 4
#define MAX_BACKLOG 16
#define MAX_REQUESTS 16

#define TIMEOUT_SEC 30
#define TIMEOUT_NSEC 0

enum req_state {
  req_state_inactive,
  req_state_read,
  req_state_write_headers,
  req_state_write_metrics,
  req_state_write_error,
};

enum http_parse_state {
  http_read_start,
  http_read_path,
  http_read_version,
  http_skip_headers_1,
  http_skip_headers_2,
};

struct scrape_req {
  enum req_state state;
  union {
    enum http_parse_state parse_state;
    unsigned collector;
  };
  bbuf *buf;
  char *io;
  size_t io_size;
  struct timespec timeout;
};

struct scrape_server {
  struct scrape_req reqs[MAX_REQUESTS];
  struct pollfd fds[MAX_LISTEN_SOCKETS + MAX_REQUESTS];
  nfds_t nfds_listen;
  nfds_t nfds_req;
};

static void req_start(struct scrape_server *srv, int socket);
static void req_close(struct scrape_server *srv, unsigned r);
static void req_process(struct scrape_server *srv, unsigned r, unsigned ncoll, const struct collector *coll[], void *coll_ctx[]);

static void timeout_start(scrape_req *req);
static bool timeout_test(scrape_req *req);
static int timeout_next_millis(scrape_req *reqs);

// TCP socket server

scrape_server *scrape_listen(const char *port) {
  scrape_server *srv = must_malloc(sizeof *srv);

  srv->nfds_listen = 0;
  srv->nfds_req = 0;
  for (unsigned i = 0; i < MAX_REQUESTS; i++) {
    srv->reqs[i].state = req_state_inactive;
    srv->reqs[i].buf = 0;
  }

  int ret;

  {
    struct addrinfo hints = {
      .ai_family = AF_UNSPEC,
      .ai_socktype = SOCK_STREAM,
      .ai_protocol = 0,
      .ai_flags = AI_PASSIVE | AI_ADDRCONFIG,
    };
    struct addrinfo *addrs;

    ret = getaddrinfo(0, port, &hints, &addrs);
    if (ret != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
      return false;
    }

    for (struct addrinfo *a = addrs; a && srv->nfds_listen < MAX_LISTEN_SOCKETS; a = a->ai_next) {
      int s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
      if (s == -1) {
        perror("socket");
        continue;
      }

      setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (int[]){1}, sizeof (int));
#if defined(IPPROTO_IPV6) && defined(IPV6_V6ONLY) && defined(AF_INET6)
      if (a->ai_family == AF_INET6)
        setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (int[]){1}, sizeof (int));
#endif

      ret = bind(s, a->ai_addr, a->ai_addrlen);
      if (ret == -1) {
        perror("bind");
        close(s);
        continue;
      }

      ret = listen(s, MAX_BACKLOG);
      if (ret == -1) {
        perror("listen");
        close(s);
        continue;
      }

      srv->fds[srv->nfds_listen].fd = s;
      srv->fds[srv->nfds_listen].events = POLLIN;
      srv->nfds_listen++;
    }
  }

  if (srv->nfds_listen == 0) {
    fprintf(stderr, "failed to bind any sockets\n");
    return 0;
  }

  return srv;
}

void scrape_serve(scrape_server *srv, unsigned ncoll, const struct collector *coll[], void *coll_ctx[]) {
  int ret;

  while (1) {
    ret = poll(srv->fds, srv->nfds_listen + srv->nfds_req, timeout_next_millis(srv->reqs));
    if (ret == -1) {
      perror("poll");
      break;
    }

    // handle incoming connections

    for (nfds_t i = 0; i < srv->nfds_listen; i++) {
      if (srv->fds[i].revents == 0)
        continue;
      if (srv->fds[i].revents != POLLIN) {
        fprintf(stderr, "poll .revents = %d\n", srv->fds[i].revents);
        return;
      }

      int s = accept(srv->fds[i].fd, 0, 0);
      if (s == -1) {
        perror("accept");
        continue;
      }

      req_start(srv, s);
    }

    // handle ongoing requests

    for (nfds_t i = srv->nfds_listen; i < srv->nfds_listen + srv->nfds_req; i++) {
      unsigned r = i - srv->nfds_listen;

      if (timeout_test(&srv->reqs[r])) {
        req_close(srv, r);
        continue;
      }

      if (srv->fds[i].fd < 0 || srv->fds[i].revents == 0)
        continue;

      if ((srv->fds[i].revents & ~(POLLIN | POLLOUT)) != 0)
        req_close(srv, r);
      else
        req_process(srv, r, ncoll, coll, coll_ctx);
    }
  }
}

void scrape_close(scrape_server *srv) {
  for (nfds_t i = 0; i < srv->nfds_listen; i++)
    close(srv->fds[i].fd);
  for (unsigned r = 0; r < MAX_REQUESTS; r++)
    if (srv->reqs[r].state != req_state_inactive)
      req_close(srv, r);
  if (srv->reqs[0].buf)
    bbuf_free(srv->reqs[0].buf);
  free(srv);
}

// scrape write API implementation

void scrape_write(scrape_req *req, const char *metric, const struct label *labels, double value) {
  if (req->state != req_state_write_metrics)
    return;

  bbuf_puts(req->buf, metric);

  if (labels && labels->key) {
    bbuf_putc(req->buf, '{');
    for (const struct label *l = labels; l->key; l++) {
      if (l != labels)
        bbuf_putc(req->buf, ',');
      bbuf_putf(req->buf, "%s=\"%s\"", l->key, l->value);
    }
    bbuf_putc(req->buf, '}');
  }

  bbuf_putf(req->buf, " %.16g\n", value);
}

void scrape_write_raw(scrape_req *req, const void *buf, size_t len) {
  bbuf_put(req->buf, buf, len);
}

// request state management

static void req_start(struct scrape_server *srv, int s) {
  int flags = fcntl(s, F_GETFL);
  if (flags == -1 || fcntl(s, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl");
    close(s);
    return;
  }

  unsigned r = 0;
  while (r < MAX_REQUESTS && srv->reqs[r].state != req_state_inactive)
    r++;
  if (r == MAX_REQUESTS) {
    close(s);
    return;
  }

  if (r >= srv->nfds_req)
    srv->nfds_req = r + 1;

  scrape_req *req = &srv->reqs[r];
  struct pollfd *pfd = &srv->fds[srv->nfds_listen + r];

  req->state = req_state_read;
  req->parse_state = http_read_start;
  if (!req->buf)
    req->buf = bbuf_alloc(BUF_INITIAL, BUF_MAX);
  timeout_start(req);

  pfd->fd = s;
  pfd->events = POLLIN;
  pfd->revents = POLLIN;  // pretend, to do the first read immediately
}

static void req_close(struct scrape_server *srv, unsigned r) {
  srv->reqs[r].state = req_state_inactive;
  if (r == 0) {
    // keep the reqs[0] buffer for reuse
    bbuf_reset(srv->reqs[0].buf);
  } else {
    bbuf_free(srv->reqs[r].buf);
    srv->reqs[r].buf = 0;
  }

  nfds_t n = srv->nfds_listen + r;
  close(srv->fds[n].fd);

  srv->fds[n].fd = -1;
  while (srv->nfds_req > 0 && srv->fds[srv->nfds_listen + srv->nfds_req - 1].fd < 0)
    srv->nfds_req--;
}

enum http_parse_result {
  http_parse_incomplete,
  http_parse_valid,
  http_parse_invalid,
};

static enum http_parse_result http_parse(int socket, enum http_parse_state *state, bbuf *buf);

static const char http_success[] =
    "HTTP/1.1 200 OK\r\n"
    "Server: nano-exporter\r\n"
    "Content-Type: text/plain; charset=UTF-8\r\n"
    "Connection: close\r\n"
    "\r\n"
    ;
static const char http_error[] =
    "HTTP/1.1 400 Bad Request\r\n"
    "Server: nano-exporter\r\n"
    "Content-Type: text/plain; charset=UTF-8\r\n"
    "Connection: close\r\n"
    "\r\n"
    "This is not a general-purpose HTTP server.\r\n"
    ;

static void req_process(struct scrape_server *srv, unsigned r, unsigned ncoll, const struct collector *coll[], void *coll_ctx[]) {
  scrape_req *req = &srv->reqs[r];
  struct pollfd *pfd = &srv->fds[srv->nfds_listen + r];

  if (req->state == req_state_inactive)
    return;

  if (req->state == req_state_read) {
    enum http_parse_result ret = http_parse(pfd->fd, &req->parse_state, req->buf);

    if (ret == http_parse_incomplete)
      return;  // try again after polling

    if (ret == http_parse_valid) {
      req->state = req_state_write_headers;
      req->io = (char *) http_success;
      req->io_size = sizeof http_success - 1;
    } else {
      req->state = req_state_write_error;
      req->io = (char *) http_error;
      req->io_size = sizeof http_error - 1;
    }

    pfd->events = POLLOUT;
  }

rewrite:
  while (req->io_size > 0) {
    ssize_t wrote = write(pfd->fd, req->io, req->io_size);

    if (wrote == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
      return;  // try again after polling
    if (wrote <= 0) {
      req_close(srv, r);
      return;
    }

    req->io += wrote;
    req->io_size -= wrote;
  }

  if (req->state == req_state_write_error) {
    req_close(srv, r);
    return;
  }

  if (req->state == req_state_write_headers) {
    req->state = req_state_write_metrics;
    req->collector = 0;
  }

  while (req->collector < ncoll) {
    bbuf_reset(req->buf);
    coll[req->collector]->collect(req, coll_ctx[req->collector]);
    req->collector++;

    if (bbuf_len(req->buf) > 0) {
      req->io = bbuf_get(req->buf, &req->io_size);
      goto rewrite;
    }
  }

  req_close(srv, r);
}

// timeout implementation

static void timeout_start(scrape_req *req) {
  struct timespec *t = &req->timeout;

  if (clock_gettime(CLOCK_MONOTONIC, t) != -1) {
    t->tv_sec += TIMEOUT_SEC;
    t->tv_nsec += TIMEOUT_NSEC;
    if (t->tv_nsec >= 1000000000) {
      t->tv_nsec -= 1000000000;
      t->tv_sec += 1;
    }
  } else {
    t->tv_sec = 0;
    t->tv_nsec = 0;
  }
}

static bool timeout_test(scrape_req *req) {
  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
    return false;
  struct timespec *t = &req->timeout;
  return now.tv_sec > t->tv_sec || (now.tv_sec == t->tv_sec && now.tv_nsec > t->tv_nsec);
}

static int timeout_next_millis(scrape_req *reqs) {
  struct timespec next = { .tv_sec = 0, .tv_nsec = 0 };

  for (unsigned i = 0; i < MAX_REQUESTS; i++) {
    scrape_req *req = &reqs[i];
    if (req->state == req_state_inactive)
      continue;
    struct timespec *t = &req->timeout;
    if (t->tv_sec == 0 && t->tv_nsec == 0)
      continue;

    if ((next.tv_sec == 0 && next.tv_nsec == 0) || t->tv_sec < next.tv_sec || (t->tv_sec == next.tv_sec && t->tv_nsec < next.tv_nsec))
      next = *t;
  }

  if (next.tv_sec == 0 && next.tv_nsec == 0)
    return -1;  // no expiring timeouts

  struct timespec now;
  if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
    return -1;  // can't tell the time

  long long millis = next.tv_sec - now.tv_sec;
  millis *= 1000;
  millis += (next.tv_nsec - now.tv_nsec) / 1000000;
  millis += 10;  // cut some slack

  if (millis <= 10)
    return 10;
  else if (millis > INT_MAX)
    return INT_MAX;
  else
    return millis;
}

// HTTP protocol functions

static enum http_parse_result http_parse(int socket, enum http_parse_state *state, bbuf *buf) {
  unsigned char http_buf[1024];

  while (true) {
    ssize_t got = read(socket, &http_buf, sizeof http_buf);

    if (got == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
      return http_parse_incomplete;
    else if (got <= 0)
      return http_parse_invalid;

    for (ssize_t i = 0; i < got; i++) {
      int c = http_buf[i];
      if (c == '\r')
        continue;

      switch (*state) {
        case http_read_start:
          if (c == ' ') {
            if (bbuf_cmp(buf, "GET") != 0)
              return http_parse_invalid;
            *state = http_read_path;
            bbuf_reset(buf);
            break;
          }
          if (!isalnum(c) || bbuf_len(buf) >= 16)
            return http_parse_invalid;
          bbuf_putc(buf, c);
          break;

        case http_read_path:
          if (c == ' ') {
            if (bbuf_cmp(buf, "/metrics") != 0)
              return http_parse_invalid;
            *state = http_read_version;
            bbuf_reset(buf);
            break;
          }
          if (!isprint(c) || c == '\n' || bbuf_len(buf) >= 128)
            return http_parse_invalid;
          bbuf_putc(buf, c);
          break;

        case http_read_version:
          if (c == '\n') {
            if (bbuf_cmp(buf, "HTTP/1.1") != 0)
              return http_parse_invalid;
            *state = http_skip_headers_1;
            bbuf_reset(buf);
            break;
          }
          if (!isgraph(c) || bbuf_len(buf) >= 16)
            return http_parse_invalid;
          bbuf_putc(buf, c);
          break;

        case http_skip_headers_1:
          if (c == '\n')
            return http_parse_valid;
          *state = http_skip_headers_2;
          break;
        case http_skip_headers_2:
          if (c == '\n')
            *state = http_skip_headers_1;
          break;
      }
    }
  }
}
