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
#include <unistd.h>

#include "collector.h"
#include "util.h"

// limits for CPU numbers
#define MAX_CPU_ID 9999999
#define MAX_CPU_DIGITS 7
// size of input buffer for reading lines
#define BUF_SIZE 256

static void *cpu_init(int argc, char *argv[]);
static void cpu_collect(scrape_req *req, void *ctx_ptr);

const struct collector cpu_collector = {
  .name = "cpu",
  .collect = cpu_collect,
  .init = cpu_init,
};

struct cpu_context {
  long clock_tick;
};

void *cpu_init(int argc, char *argv[]) {
  (void) argc; (void) argv;

  long clock_tick = sysconf(_SC_CLK_TCK);
  if (clock_tick <= 0) {
    perror("sysconf(_SC_CLK_TCK)");
    return 0;
  }

  struct cpu_context *ctx = malloc(sizeof *ctx);
  if (!ctx)
    return 0;
  ctx->clock_tick = clock_tick;
  return ctx;
}

void cpu_collect(scrape_req *req, void *ctx_ptr) {
  struct cpu_context *ctx = ctx_ptr;

  // buffers

  char cpu_label[MAX_CPU_DIGITS + 1] = "";
  static const char *modes[] = {
    "user", "nice", "system", "idle", "iowait", "irq", "softirq", "steal",
    0,
  };

  const char *stat_labels[][2] = {
    { "cpu", cpu_label },
    { "mode", 0 },  // filled by code
    { 0, 0 },
  };
  const char *freq_labels[][2] = {
    { "cpu", cpu_label },
    { 0, 0 },
  };

  char buf[BUF_SIZE];

  FILE *f;

  // collect node_cpu_seconds_total metrics from /proc/stat

  f = fopen("/proc/stat", "r");
  if (f) {
    while (fgets_line(buf, sizeof buf, f)) {
      if (strncmp(buf, "cpu", 3) != 0 || (buf[3] < '0' || buf[3] > '9'))
        continue;

      char *at = buf + 3;
      char *sep = strchr(at, ' ');
      if (!sep || sep - at + 1 > (ptrdiff_t) sizeof cpu_label)
        continue;
      *sep = '\0';
      strcpy(cpu_label, at);

      at = sep + 1;
      for (const char **mode = modes; *mode; mode++) {
        while (*at == ' ')
          at++;
        sep = strpbrk(at, " \n");
        if (!sep)
          break;
        *sep = '\0';

        char *endptr;
        double value = strtod(at, &endptr);
        if (*endptr != '\0')
          break;
        value /= ctx->clock_tick;

        stat_labels[1][1] = *mode;
        scrape_write(req, "node_cpu_seconds_total", stat_labels, value);

        at = sep + 1;
      }
    }
    fclose(f);
  }

  // collect node_cpu_frequency_hertz metrics from /sys/devices/system/cpu/cpu*/cpufreq

  for (int cpu = 0; cpu <= MAX_CPU_ID; cpu++) {
#define PATH_FORMAT "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq"
    char path[sizeof PATH_FORMAT - 2 + MAX_CPU_DIGITS + 1];
    snprintf(path, sizeof path, PATH_FORMAT, cpu);

    f = fopen(path, "r");
    if (!f)
      break;

    if (fgets(buf, sizeof buf, f)) {
      char *endptr;
      double value = strtod(buf, &endptr);
      if (*endptr == '\0' || *endptr == '\n') {
        value *= 1000;
        snprintf(cpu_label, sizeof cpu_label, "%d", cpu);
        scrape_write(req, "node_cpu_frequency_hertz", freq_labels, value);
      }
    }

    fclose(f);
  }
}
