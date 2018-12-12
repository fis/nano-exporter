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
#include <string.h>

#include "collector.h"
#include "util.h"

// size of input buffer for paths and lines
#define BUF_SIZE 256

static void stat_collect(scrape_req *req, void *ctx);

const struct collector stat_collector = {
  .name = "stat",
  .collect = stat_collect,
};

static const struct {
  const char *name;
  const char *key;
  unsigned key_len;
} metrics[] = {
  { .name = "node_boot_time_seconds", .key = "btime ", .key_len = 6 },
  { .name = "node_context_switches_total", .key = "ctxt ", .key_len = 5 },
  { .name = "node_forks_total", .key = "processes ", .key_len = 10 },
  { .name = "node_intr_total", .key = "intr ", .key_len = 5 },
  { .name = "node_procs_blocked", .key = "procs_blocked ", .key_len = 14 },
  { .name = "node_procs_running", .key = "procs_running ", .key_len = 14 },
};
#define NMETRICS (sizeof metrics / sizeof *metrics)

static void stat_collect(scrape_req *req, void *ctx) {
  (void) ctx;

  // buffers

  char buf[BUF_SIZE];

  FILE *f;

  // scan /proc/stat for metrics

  f = fopen(PATH("/proc/stat"), "r");
  if (!f)
    return;

  while (fgets_line(buf, sizeof buf, f)) {
    for (size_t m = 0; m < NMETRICS; m++) {
      if (strncmp(buf, metrics[m].key, metrics[m].key_len) != 0)
        continue;

      char *end;
      double d = strtod(buf + metrics[m].key_len, &end);
      if (*end != '\0' && *end != ' ' && *end != '\n')
        continue;

      scrape_write(req, metrics[m].name, 0, d);
      break;
    }
  }

  fclose(f);
}
