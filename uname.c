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
  struct label labels[max_label + 1];
};

static void uname_set_labels(struct uname_context *ctx, struct utsname *name) {
  ctx->labels[label_machine].key = "machine";
  ctx->labels[label_machine].value = must_strdup(name->machine);
  ctx->labels[label_nodename].key = "nodename";
  ctx->labels[label_nodename].value = must_strdup(name->nodename);
  ctx->labels[label_release].key = "release";
  ctx->labels[label_release].value = must_strdup(name->release);
  ctx->labels[label_sysname].key = "sysname";
  ctx->labels[label_sysname].value = must_strdup(name->sysname);
  ctx->labels[label_version].key = "version";
  ctx->labels[label_version].value = must_strdup(name->version);
  ctx->labels[max_label].key = ctx->labels[max_label].value = 0;
}

static void *uname_init(int argc, char *argv[]) {
  (void) argc; (void) argv;

  struct utsname name;
  if (uname(&name) == -1) {
    perror("uname");
    return 0;
  }

  struct uname_context *ctx = must_malloc(sizeof *ctx);
  uname_set_labels(ctx, &name);
  return ctx;
}

static void uname_collect(scrape_req *req, void *ctx_ptr) {
  struct uname_context *ctx = ctx_ptr;
  scrape_write(req, "node_uname_info", ctx->labels, 1.0);
}

#ifdef NANO_EXPORTER_TEST
void uname_test_override_data(void *ctx, struct utsname *name) {
  uname_set_labels((struct uname_context *) ctx, name);
}
#endif // NANO_EXPORTER_TEST
