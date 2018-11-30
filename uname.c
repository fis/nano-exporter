#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>

#include "collector.h"
#include "util.h"

static void *uname_init(int argc, char *argv[]);
static void uname_collect(scrape_req *req, void *ctx);

const struct collector uname_collector = {
  .name = "uname",
  .collect = uname_collect,
  .init = uname_init,
};

enum {
  label_machine,
  label_nodename,
  label_release,
  label_sysname,
  label_version,
  max_label,
};

struct uname_context {
  const char *labels[max_label + 1][2];
};

static void *uname_init(int argc, char *argv[]) {
  struct utsname name;
  if (uname(&name) == -1) {
    perror("uname");
    return 0;
  }

  struct uname_context *ctx = must_malloc(sizeof *ctx);
  ctx->labels[label_machine][0] = "machine";
  ctx->labels[label_machine][1] = must_strdup(name.machine);
  ctx->labels[label_nodename][0] = "nodename";
  ctx->labels[label_nodename][1] = must_strdup(name.nodename);
  ctx->labels[label_release][0] = "release";
  ctx->labels[label_release][1] = must_strdup(name.release);
  ctx->labels[label_sysname][0] = "sysname";
  ctx->labels[label_sysname][1] = must_strdup(name.sysname);
  ctx->labels[label_version][0] = "version";
  ctx->labels[label_version][1] = must_strdup(name.version);
  ctx->labels[max_label][0] = ctx->labels[max_label][1] = 0;
  return ctx;
}

static void uname_collect(scrape_req *req, void *ctx_ptr) {
  struct uname_context *ctx = ctx_ptr;
  scrape_write(req, "node_uname_info", ctx->labels, 1.0);
}
