#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diskstats.h"
#include "util.h"

// size of input buffer for paths and lines
#define BUF_SIZE 256

static void *diskstats_init(int argc, char *argv[]);
static void diskstats_collect(scrape_req *req, void *ctx);

const struct collector diskstats_collector = {
  .name = "diskstats",
  .collect = diskstats_collect,
  .init = diskstats_init,
  .has_args = true,
};

struct diskstats_context {
  struct slist *include;
  struct slist *exclude;
};

static void *diskstats_init(int argc, char *argv[]) {
  struct diskstats_context *ctx = must_malloc(sizeof *ctx);

  ctx->include = 0;
  ctx->exclude = 0;

  for (int arg = 0; arg < argc; arg++) {
    if (strncmp(argv[arg], "include=", 8) == 0) {
      ctx->include = slist_split(&argv[arg][8], ",");
    } else if (strncmp(argv[arg], "exclude=", 8) == 0) {
      ctx->exclude = slist_split(&argv[arg][8], ",");
    } else {
      fprintf(stderr, "unknown argument for diskstats collector: %s", argv[arg]);
      return 0;
    }
  }

  return ctx;
}

static const struct {
  const char *metric;
  double factor;
} columns[] = {
  { .metric = "node_disk_reads_completed_total", .factor = 1.0 },
  { .metric = "node_disk_reads_merged_total", .factor = 1.0 },
  { .metric = "node_disk_read_bytes_total", .factor = 512.0 },  // TODO: always 512-byte sectors?
  { .metric = "node_disk_read_time_seconds_total", .factor = 0.001 },
  { .metric = "node_disk_writes_completed_total", .factor = 1.0 },
  { .metric = "node_disk_writes_merged_total", .factor = 1.0 },
  { .metric = "node_disk_written_bytes_total", .factor = 512.0 },  // TODO: constant?
  { .metric = "node_disk_write_time_seconds_total", .factor = 0.001 },  // TODO: constant?
  { .metric = "node_disk_io_now", .factor = 1.0 },
  { .metric = "node_disk_io_time_seconds_total", .factor = 0.001 },
  { .metric = "node_disk_io_time_weighted_seconds_total", .factor = 0.001 },
  // TODO: metrics for 4.18+ discard fields
  // - # of discards completed
  // - # of discards merged
  // - # of sectors discarded
  // - # of milliseconds spent discarding
};
#define NCOLUMNS (sizeof columns / sizeof *columns)

static void diskstats_collect(scrape_req *req, void *ctx_ptr) {
  struct diskstats_context *ctx = ctx_ptr;

  // buffers

  const char *labels[][2] = {
    { "device", 0 },  // filled by code
    { 0, 0 },
  };

  char buf[BUF_SIZE];

  FILE *f;

  // emit metrics for all known columns of /proc/diskstats for included devices

  f = fopen("/proc/diskstats", "r");
  if (!f)
    return;

  while (fgets_line(buf, sizeof buf, f)) {
    char *p;

    // skip first two columns (device node numbers)

    strtok_r(buf, " ", &p);
    strtok_r(0, " ", &p);

    // extract device name

    char *dev = strtok_r(0, " ", &p);
    if (!dev || *dev == '\0')
      continue;
    labels[0][1] = dev;

    // filter

    if (ctx->include) {
      if (!slist_contains(ctx->include, dev))
        continue;
    } else if (ctx->exclude) {
      if (slist_contains(ctx->exclude, dev))
        continue;
    }

    // emit metrics while known columns last

    for (size_t c = 0; c < NCOLUMNS; c++) {
      char *v = strtok_r(0, " ", &p);
      if (!v || *v == '\0')
        break;

      char *end;
      double d = strtod(v, &end);
      if (*end != '\0')
        continue;

      scrape_write(req, columns[c].metric, labels, d * columns[c].factor);
    }
  }

  fclose(f);
}
