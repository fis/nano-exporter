#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "meminfo.h"
#include "util.h"

// size of input buffer for paths and lines
#define BUF_SIZE 256

// prefix to add to /proc/meminfo lines; must fit in BUF_SIZE
#define METRIC_PREFIX "node_memory_"
#define METRIC_PREFIX_LEN (sizeof METRIC_PREFIX - 1)

// suffix to add for " kB" metrics
#define BYTES_SUFFIX "_bytes"
#define BYTES_SUFFIX_LEN (sizeof BYTES_SUFFIX - 1)

static void meminfo_collect(scrape_req *req, void *ctx);

const struct collector meminfo_collector = {
  .name = "meminfo",
  .collect = meminfo_collect,
};

static void meminfo_collect(scrape_req *req, void *ctx) {
  // buffers

  char buf[BUF_SIZE] = METRIC_PREFIX;

  FILE *f;

  // convert /proc/meminfo to metrics format

  f = fopen("/proc/meminfo", "r");
  if (!f)
    return;

  while (fgets_line(buf + METRIC_PREFIX_LEN, sizeof buf - METRIC_PREFIX_LEN, f)) {
    char *p = buf + METRIC_PREFIX_LEN;
    while (*p != '\0' && *p != ':') {
      if (!isalnum((unsigned char)*p))
        *p = '_';
      p++;
    }
    if (*p != ':')
      continue;
    char *metric_end = p;

    while (metric_end > buf + METRIC_PREFIX_LEN && metric_end[-1] == '_')
      --metric_end;
    *metric_end = '\0';

    do p++; while (*p == ' ');

    double value = strtod(p, &p);
    if (*p != '\0' && *p != ' ')
      continue;

    if (*p == ' ') {
      do p++; while (*p == ' ');
      if (p[0] == 'k' && p[1] == 'B') {
        if ((metric_end - buf) + BYTES_SUFFIX_LEN >= sizeof buf)
          continue;
        strcpy(metric_end, BYTES_SUFFIX);
        value *= 1024.0;
      }
    }

    scrape_write(req, buf, 0, value);
  }

  fclose(f);
}
