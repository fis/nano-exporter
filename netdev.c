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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scrape.h"
#include "util.h"

// size of input buffer for paths and lines
#define BUF_SIZE 256
// maximum number of columns in the file
#define MAX_COLUMNS 32

// default list of interfaces to exclude
#define DEFAULT_EXCLUDE "lo"

static void *netdev_init(int argc, char *argv[]);
static void netdev_collect(scrape_req *req, void *ctx);

const struct collector netdev_collector = {
  .name = "netdev",
  .collect = netdev_collect,
  .init = netdev_init,
  .has_args = true,
};

struct netdev_context {
  size_t ncolumns;
  char *columns[MAX_COLUMNS];
  struct slist *include;
  struct slist *exclude;
};

static void *netdev_init(int argc, char *argv[]) {
  // parse header from /proc/net/dev and prepare metric names

  FILE *f = fopen(PATH("/proc/net/dev"), "r");
  if (!f) {
    perror("fopen /proc/net/dev");
    return 0;
  }

  char buf[BUF_SIZE];
  char *p;

  fgets_line(buf, sizeof buf, f);
  p = fgets_line(buf, sizeof buf, f);
  fclose(f);
  if (!p) {
    fprintf(stderr, "second header line in /proc/net/dev missing\n");
    return 0;
  }

  static const char *const prefixes[2] = { "node_network_receive_", "node_network_transmit_" };
  char *parts[2];
  char *saveptr;

  strtok_r(buf, "|", &saveptr);
  parts[0] = strtok_r(0, "|", &saveptr);
  parts[1] = strtok_r(0, "|", &saveptr);
  p = strtok_r(0, "|", &saveptr);
  if (!parts[0] || !parts[1] || p) {
    fprintf(stderr, "too %s parts in /proc/net/dev header\n", p ? "many" : "few");
    return 0;
  }

  struct netdev_context *ctx = malloc(sizeof *ctx);
  if (!ctx) {
    perror("malloc");
    return 0;
  }

  ctx->ncolumns = 0;
  for (int part = 0; part < 2; part++) {
    size_t prefix_len = strlen(prefixes[part]);

    for (p = strtok_r(parts[part], " \n", &saveptr); p; p = strtok_r(0, " \n", &saveptr)) {
      if (ctx->ncolumns >= MAX_COLUMNS) {
        fprintf(stderr, "too many columns in /proc/net/dev\n");
        goto cleanup;
      }

      size_t header_len = strlen(p);
      size_t metric_len = prefix_len + header_len + 6;  // 6 for "_total"

      ctx->columns[ctx->ncolumns] = malloc(metric_len + 1);
      if (!ctx->columns[ctx->ncolumns]) {
        perror("malloc");
        goto cleanup;
      }

      snprintf(ctx->columns[ctx->ncolumns], metric_len + 1, "%s%s_total", prefixes[part], p);
      ctx->ncolumns++;
    }
  }

  // parse command-line arguments

  ctx->include = 0;
  ctx->exclude = 0;
  bool exclude_set = false;

  for (int arg = 0; arg < argc; arg++) {
    if (strncmp(argv[arg], "include=", 8) == 0) {
      ctx->include = slist_split(argv[arg] + 8, ",");
      continue;
    }
    if (strncmp(argv[arg], "exclude=", 8) == 0) {
      ctx->exclude = slist_split(argv[arg] + 8, ",");
      continue;
    }

    fprintf(stderr, "unknown argument for netdev collector: %s\n", argv[arg]);
    goto cleanup;
  }

  if (!exclude_set)
    ctx->exclude = slist_split(DEFAULT_EXCLUDE, ",");

  return ctx;

cleanup:
  for (size_t i = 0; i < ctx->ncolumns; i++)
    free(ctx->columns[i]);
  free(ctx);
  return 0;
}

static void netdev_collect(scrape_req *req, void *ctx_ptr) {
  struct netdev_context *ctx = ctx_ptr;

  // buffers

  struct label labels[] = {
    { .key = "device", .value = 0 },  // value filled by code
    LABEL_END,
  };

  char buf[BUF_SIZE];

  FILE *f;

  // read network stats from /proc/net/dev

  f = fopen(PATH("/proc/net/dev"), "r");
  if (!f)
    return;

  fgets_line(buf, sizeof buf, f);
  fgets_line(buf, sizeof buf, f);  // skipped header

  while (fgets_line(buf, sizeof buf, f)) {
    char *dev = buf;
    while (*dev == ' ')
      dev++;

    char *p = strchr(dev, ':');
    if (!p)
      continue;
    *p = '\0';
    labels[0].value = dev;
    p++;

    if (ctx->include) {
      if (!slist_matches(ctx->include, dev))
        continue;
    } else if (ctx->exclude) {
      if (slist_matches(ctx->exclude, dev))
        continue;
    }

    char *saveptr;
    p = strtok_r(p, " \n", &saveptr);
    for (size_t i = 0; i < ctx->ncolumns && p; i++, p = strtok_r(0, " \n", &saveptr)) {
      char *endptr;
      double value = strtod(p, &endptr);
      if (*endptr != '\0')
        continue;
      scrape_write(req, ctx->columns[i], labels, value);
    }
  }

  fclose(f);
}
