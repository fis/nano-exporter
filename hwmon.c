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

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "collector.h"
#include "util.h"

// size of input buffer for paths and lines
#define BUF_SIZE 256
// size of input buffer for labels
#define LABEL_SIZE 32

static void hwmon_collect(scrape_req *req, void *ctx);

const struct collector hwmon_collector = {
  .name = "hwmon",
  .collect = hwmon_collect,
};

static double hwmon_conv_millis(const char *text) {
  char *endptr;
  double value = strtod(text, &endptr);
  if (*endptr == '\n' || *endptr == '\0')
    return value / 1000;
  return NAN;
}

static double hwmon_conv_id(const char *text) {
  char *endptr;
  double value = strtod(text, &endptr);
  if (*endptr == '\n' || *endptr == '\0')
    return value;
  return NAN;
}

static double hwmon_conv_flag(const char *text) {
  if ((text[0] == '0' || text[0] == '1') && (text[1] == '\n' || text[1] == '\0'))
    return text[0] - '0';
  return NAN;
}

struct metric_type {
  const char *suffix;
  const char *metric;
  double (*conv)(const char *text);
};

struct metric_data {
  const char *prefix;
  const struct metric_type *types;
};

static const struct metric_data metrics[] = {
  {
    .prefix = "in",
    .types = (const struct metric_type[]){
      {
        .suffix = "_input",
        .metric = "node_hwmon_in_volts",
        .conv = hwmon_conv_millis,
      },
      {
        .suffix = "_min",
        .metric = "node_hwmon_in_min_volts",
        .conv = hwmon_conv_millis,
      },
      {
        .suffix = "_max",
        .metric = "node_hwmon_in_max_volts",
        .conv = hwmon_conv_millis,
      },
      {
        .suffix = "_alarm",
        .metric = "node_hwmon_in_alarm",
        .conv = hwmon_conv_flag,
      },
      { .suffix = 0 },
    },
  },
  {
    .prefix = "fan",
    .types = (const struct metric_type[]){
      {
        .suffix = "_input",
        .metric = "node_hwmon_fan_rpm",
        .conv = hwmon_conv_id,
      },
      {
        .suffix = "_min",
        .metric = "node_hwmon_fan_min_rpm",
        .conv = hwmon_conv_id,
      },
      {
        .suffix = "_alarm",
        .metric = "node_hwmon_fan_alarm",
        .conv = hwmon_conv_flag,
      },
      { .suffix = 0 },
    },
  },
  {
    .prefix = "temp",
    .types = (const struct metric_type[]){
      {
        .suffix = "_input",
        .metric = "node_hwmon_temp_celsius",
        .conv = hwmon_conv_millis,
      },
      { .suffix = 0 },
    },
  },
  { .prefix = 0 },
};

static void hwmon_name(const char *path, char *dst, size_t dst_len) {
  char buf[BUF_SIZE];
  FILE *f;
  size_t len;

  // if the path is a symlink to "../../devices/X/Y/...", use X/Y as the name,
  // except if it is "virtual/hwmon"

  len = readlink(path, buf, sizeof buf);
  if (len > 14 && memcmp(buf, "../../devices/", 14) == 0) {
    char *start = buf + 14;
    char *end = strchr(start, '/');
    if (end)
      end = strchr(end + 1, '/');
    if (end)
      *end = '\0';
    if (strcmp(start, "virtual/hwmon") != 0) {
      snprintf(dst, dst_len, "%s", start);
      return;
    }
  }

  // try to use the 'name' file

  snprintf(buf, sizeof buf, "%s/name", path);
  f = fopen(buf, "r");
  if (f) {
    bool found = false;
    if (fgets(buf, sizeof buf, f)) {
      len = strlen(buf);
      if (len > 0 && buf[len-1] == '\n') {
        len--;
        buf[len] = '\0';
      }
      if (len > 0) {
        snprintf(dst, dst_len, "hwmon/%s", buf);
        found = true;
      }
    }
    fclose(f);
    if (found)
      return;
  }

  // give up, just call this unknown

  // TODO: add an index
  snprintf(dst, dst_len, "unknown");
}

static void hwmon_collect(scrape_req *req, void *ctx) {
  (void) ctx;

  // buffers

  char chip_label[LABEL_SIZE];
  char sensor_label[LABEL_SIZE];

  struct label labels[] = {
    { .key = "chip", .value = chip_label },
    { .key = "sensor", .value = sensor_label },
    LABEL_END,
  };

  char path[BUF_SIZE];
  char buf[BUF_SIZE];

  FILE *f;

  DIR *root;
  struct dirent *dent;

  // iterate over all hwmon instances in /sys/class/hwmon

  root = opendir(PATH("/sys/class/hwmon"));
  if (!root)
    return;

  while ((dent = readdir(root))) {
    if (strncmp(dent->d_name, "hwmon", 5) != 0)
      continue;
    snprintf(path, sizeof path, PATH("/sys/class/hwmon/%s"), dent->d_name);

    hwmon_name(path, chip_label, sizeof chip_label);

    DIR *dir = opendir(path);
    if (!dir)
      continue;

    while ((dent = readdir(dir))) {
      for (const struct metric_data *metric = metrics; metric->prefix; metric++) {
        if (strncmp(dent->d_name, metric->prefix, strlen(metric->prefix)) != 0)
          continue;
        char *suffix = strchr(dent->d_name, '_');
        if (!suffix)
          continue;

        snprintf(sensor_label, sizeof sensor_label, "%.*s", (int)(suffix - dent->d_name), dent->d_name);

        for (const struct metric_type *type = metric->types; type->suffix; type++) {
          if (strcmp(suffix, type->suffix) != 0)
            continue;

          snprintf(buf, sizeof buf, "%s/%s", path, dent->d_name);
          f = fopen(buf, "r");
          if (!f)
            continue;
          if (fgets(buf, sizeof buf, f)) {
            double value = type->conv(buf);
            if (!isnan(value))
              scrape_write(req, type->metric, labels, value);
          }
          fclose(f);
        }
      }
    }

    closedir(dir);
  }

  closedir(root);
}
