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
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "scrape.h"
#include "util.h"

#define BUF_INITIAL 1024
#define BUF_MAX 65536

#define MAX_LISTEN_SOCKETS 4
#define MAX_BACKLOG 16

struct scrape_req {
  int socket;
  cbuf *buf;
};

struct scrape_server {
  struct pollfd fds[MAX_LISTEN_SOCKETS];
  nfds_t nfds;
};

static bool handle_http(struct scrape_req *req);

scrape_server *scrape_listen(const char *port) {
  scrape_server *srv = must_malloc(sizeof *srv);
  srv->nfds = 0;

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

    for (struct addrinfo *a = addrs; a && srv->nfds < MAX_LISTEN_SOCKETS; a = a->ai_next) {
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

      srv->fds[srv->nfds].fd = s;
      srv->fds[srv->nfds].events = POLLIN;
      srv->nfds++;
    }
  }

  if (srv->nfds == 0) {
    fprintf(stderr, "failed to bind any sockets\n");
    return 0;
  }

  return srv;
}

void scrape_serve(scrape_server *srv, scrape_handler *handler, void *handler_ctx) {
  struct scrape_req req;
  req.buf = cbuf_alloc(BUF_INITIAL, BUF_MAX);

  int ret;

  while (1) {
    ret = poll(srv->fds, srv->nfds, -1);
    if (ret == -1) {
      perror("poll");
      break;
    }

    for (nfds_t i = 0; i < srv->nfds; i++) {
      if (srv->fds[i].revents == 0)
        continue;
      if (srv->fds[i].revents != POLLIN) {
        fprintf(stderr, "poll .revents = %d\n", srv->fds[i].revents);
        return;
      }

      req.socket = accept(srv->fds[i].fd, 0, 0);
      if (req.socket == -1) {
        perror("accept");
        continue;
      }

      if (handle_http(&req))
        handler(&req, handler_ctx);
      close(req.socket);
    }
  }
}

void scrape_close(scrape_server *srv) {
  for (nfds_t i = 0; i < srv->nfds; i++)
    close(srv->fds[i].fd);
  free(srv);
}

void scrape_write(scrape_req *req, const char *metric, const struct label *labels, double value) {
  cbuf_reset(req->buf);

  cbuf_puts(req->buf, metric);

  if (labels && labels->key) {
    cbuf_putc(req->buf, '{');
    for (const struct label *l = labels; l->key; l++) {
      if (l != labels)
        cbuf_putc(req->buf, ',');
      cbuf_putf(req->buf, "%s=\"%s\"", l->key, l->value);
    }
    cbuf_putc(req->buf, '}');
  }

  cbuf_putf(req->buf, " %.16g\n", value);

  size_t buf_len;
  const char *buf = cbuf_get(req->buf, &buf_len);
  write_all(req->socket, buf, buf_len);
}

void scrape_write_raw(scrape_req *req, const void *buf, size_t len) {
  write_all(req->socket, buf, len);
}

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

static bool handle_http(struct scrape_req *req) {
  unsigned char http_buf[1024];
  cbuf *tmp_buf = req->buf;

  enum {
    state_read_method,
    state_read_path,
    state_read_version,
    state_skip_headers_1,
    state_skip_headers_2
  } state = state_read_method;
  cbuf_reset(tmp_buf);

  while (true) {
    ssize_t got = read(req->socket, &http_buf, sizeof http_buf);
    if (got <= 0)
      return false;
    for (ssize_t i = 0; i < got; i++) {
      int c = http_buf[i];
      if (c == '\r')
        continue;

      switch (state) {
        case state_read_method:
          if (c == ' ') {
            if (cbuf_cmp(tmp_buf, "GET") != 0)
              goto fail;
            state = state_read_path;
            cbuf_reset(tmp_buf);
            break;
          }
          if (!isalnum(c) || cbuf_len(tmp_buf) >= 16)
            goto fail;
          cbuf_putc(tmp_buf, c);
          break;

        case state_read_path:
          if (c == ' ') {
            if (cbuf_cmp(tmp_buf, "/metrics") != 0)
              goto fail;
            state = state_read_version;
            cbuf_reset(tmp_buf);
            break;
          }
          if (!isprint(c) || c == '\n' || cbuf_len(tmp_buf) >= 128)
            goto fail;
          cbuf_putc(tmp_buf, c);
          break;

        case state_read_version:
          if (c == '\n') {
            if (cbuf_cmp(tmp_buf, "HTTP/1.1") != 0)
              goto fail;
            state = state_skip_headers_1;
            cbuf_reset(tmp_buf);
            break;
          }
          if (!isgraph(c) || cbuf_len(tmp_buf) >= 16)
            goto fail;
          cbuf_putc(tmp_buf, c);
          break;

        case state_skip_headers_1:
          if (c == '\n')
            goto succeed;
          state = state_skip_headers_2;
          break;
        case state_skip_headers_2:
          if (c == '\n')
            state = state_skip_headers_1;
          break;
      }
    }
  }

succeed:
  write_all(req->socket, http_success, sizeof http_success - 1);
  return true;

fail:
  write_all(req->socket, http_error, sizeof http_error - 1);
  return false;
}
