#define _POSIX_C_SOURCE 200809L

#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
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

bool scrape_serve(const char *port, scrape_handler *handler, void *handler_ctx) {
  struct scrape_req req;

  struct pollfd fds[MAX_LISTEN_SOCKETS];
  nfds_t nfds = 0;

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

    for (struct addrinfo *a = addrs; a && nfds < MAX_LISTEN_SOCKETS; a = a->ai_next) {
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

      fds[nfds].fd = s;
      fds[nfds].events = POLLIN;
      nfds++;
    }
  }

  if (nfds == 0) {
    fprintf(stderr, "failed to bind any sockets");
    return false;
  }

  req.buf = cbuf_alloc(BUF_INITIAL, BUF_MAX);
  if (!req.buf) {
    perror("cbuf_alloc");
    for (nfds_t i = 0; i < nfds; i++)
      close(fds[i].fd);
    return false;
  }

  while (1) {
    ret = poll(fds, nfds, -1);
    if (ret == -1) {
      perror("poll");
      break;
    }

    for (nfds_t i = 0; i < nfds; i++) {
      if (fds[i].revents == 0)
        continue;
      if (fds[i].revents != POLLIN) {
        fprintf(stderr, "poll .revents = %d\n", fds[i].revents);
        goto break_loop;
      }

      req.socket = accept(fds[i].fd, 0, 0);
      if (req.socket == -1) {
        perror("accept");
        continue;
      }

      // TODO: HTTP stuff
      handler(&req, handler_ctx);
      close(req.socket);
    }
  }
break_loop:

  for (nfds_t i = 0; i < nfds; i++)
    close(fds[i].fd);

  return false;
}

void scrape_write(scrape_req *req, const char *metric, const char *(*labels)[2], double value) {
  cbuf_reset(req->buf);

  cbuf_puts(req->buf, metric);

  if (labels && (*labels)[0]) {
    cbuf_putc(req->buf, '{');
    for (const char *(*l)[2] = labels; (*l)[0]; l++) {
      if (l != labels)
        cbuf_putc(req->buf, ',');
      cbuf_putf(req->buf, "%s=\"%s\"", (*l)[0], (*l)[1]);
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
