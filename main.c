#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "collector.h"
#include "scrape.h"
#include "util.h"

#ifndef XCOLLECTORS
#define XCOLLECTORS \
  X(cpu) X(diskstats) X(filesystem) X(hwmon) \
  X(meminfo) X(network) X(textfile) X(uname)
#endif

#define X(c) extern const struct collector c##_collector;
XCOLLECTORS
#undef X

#define X(c) &c##_collector,
static const struct collector *collectors[] = {
  XCOLLECTORS
};
#undef X
#define NCOLLECTORS (sizeof collectors / sizeof *collectors)

enum tristate { flag_off = -1, flag_undef = 0, flag_on = 1 };

struct config {
  const char *port;
  bool daemonize;
};

const struct config default_config = {
  .port = "9100",
  .daemonize = true,
};

struct handler_ctx {
  struct {
    enum tristate enabled;
    struct slist *args;
    struct slist **next_arg;
    void *ctx;
  } collectors[NCOLLECTORS];
};

static bool parse_args(int argc, char *argv[], struct config *cfg, struct handler_ctx *ctx);
static bool initialize(struct handler_ctx *ctx, int max_args);
static bool daemonize(void);
static void handler(scrape_req *req, void *ctx_ptr);

int main(int argc, char *argv[]) {
  struct config cfg = default_config;
  struct handler_ctx ctx;

  if (!parse_args(argc, argv, &cfg, &ctx))
    return 1;
  if (!initialize(&ctx, argc > 0 ? argc - 1 : 0))
    return 1;

  scrape_server *server = scrape_listen(cfg.port);
  if (!server)
    return 1;

  if (cfg.daemonize)
    if (!daemonize())
      return 1;

  scrape_serve(server, handler, &ctx);
  scrape_close(server);

  return 0;
};

static bool parse_args(int argc, char *argv[], struct config *cfg, struct handler_ctx *ctx) {
  for (size_t i = 0; i < NCOLLECTORS; i++) {
    ctx->collectors[i].args = 0;
    ctx->collectors[i].next_arg = &ctx->collectors[i].args;
    ctx->collectors[i].enabled = flag_undef;
  }

  enum tristate enabled_default = flag_on;

  for (int arg = 1; arg < argc; arg++) {
    // check for collector arguments

    for (size_t i = 0; i < NCOLLECTORS; i++) {
      size_t name_len = strlen(collectors[i]->name);
      if (strncmp(argv[arg], "--", 2) == 0
          && strncmp(argv[arg] + 2, collectors[i]->name, name_len) == 0
          && argv[arg][2 + name_len] == '-') {
        char *carg = argv[arg] + 2 + name_len + 1;
        if (strcmp(carg, "on") == 0) {
          ctx->collectors[i].enabled = flag_on;
          enabled_default = flag_off;
        } else if (strcmp(carg, "off") == 0) {
          ctx->collectors[i].enabled = flag_off;
        } else if (collectors[i]->init && collectors[i]->has_args) {
          ctx->collectors[i].next_arg = slist_append(ctx->collectors[i].next_arg, carg);
        } else {
          fprintf(stderr, "unknown argument: %s (collector %s takes no arguments)\n", argv[arg], collectors[i]->name);
          return false;
        }
        goto next_arg;
      }
    }

    // parse any non-collector arguments

    // TODO --help
    if (strncmp(argv[arg], "--port=", 7) == 0) {
      cfg->port = &argv[arg][7];
      goto next_arg;
    } else if (strcmp(argv[arg], "--foreground") == 0) {
      cfg->daemonize = false;
      goto next_arg;
    }

    fprintf(stderr, "unknown argument: %s\n", argv[arg]);
    return false;
 next_arg: ;
  }

  for (size_t i = 0; i < NCOLLECTORS; i++)
    if (ctx->collectors[i].enabled == flag_undef)
      ctx->collectors[i].enabled = enabled_default;

  return true;
}

static bool initialize(struct handler_ctx *ctx, int max_args) {
  int argc;
  char *argv[max_args + 1];

  for (size_t i = 0; i < NCOLLECTORS; i++) {
    if (ctx->collectors[i].enabled == flag_on && collectors[i]->init) {
      argc = 0;
      for (struct slist *arg = ctx->collectors[i].args; arg && argc < max_args; arg = arg->next)
        argv[argc++] = arg->data;
      argv[argc] = 0;

      ctx->collectors[i].ctx = collectors[i]->init(argc, argv);

      if (!ctx->collectors[i].ctx) {
        fprintf(stderr, "failed to initialize collector %s\n", collectors[i]->name);
        return false;
      }
    }
  }

  return true;
}

static bool daemonize(void) {
  pid_t pid;

  pid = fork();
  if (pid == -1) {
    perror("fork");
    return false;
  }

  if (pid > 0) {
    // in the parent, verify the intermediate process exits successfully
    int status;
    if (waitpid(pid, &status, 0) == -1) {
      perror("waitpid(daemon)");
      exit(1);
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      fprintf(stderr, "daemonized process exited unexpectedly: %x\n", status);
      exit(1);
    }
    exit(0);
  }

  // in the intermediate process, prepare for daemon life and fork again

  if (setsid() == -1) {
    perror("setsid");
    exit(1);
  }

  close(0);
  close(1);
  close(2);

  pid = fork();
  if (pid == -1)
    exit(2);
  else if (pid > 0)
    exit(0);  // lets the parent know all is well

  // in the grandchild, continue execution

  return true;
}

static void handler(scrape_req *req, void *ctx_ptr) {
  struct handler_ctx *ctx = ctx_ptr;

  for (size_t c = 0; c < NCOLLECTORS; c++) {
    if (ctx->collectors[c].enabled == flag_on)
      collectors[c]->collect(req, ctx->collectors[c].ctx);
  }
}
