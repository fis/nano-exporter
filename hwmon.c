#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "hwmon.h"

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

static void hwmon_collect(scrape_req *req, void *ctx) {
  // buffers

  char chip_label[LABEL_SIZE];
  char sensor_label[LABEL_SIZE];

  const char *labels[][2] = {
    { "chip", chip_label },
    { "sensor", sensor_label },
    { 0, 0 },
  };

  char path[BUF_SIZE];
  char buf[BUF_SIZE];
  size_t len;

  FILE *f;

  DIR *root;
  struct dirent *dent;

  // iterate over all hwmon instances in /sys/class/hwmon

  root = opendir("/sys/class/hwmon");
  if (!root)
    return;

  while ((dent = readdir(root))) {
    if (strncmp(dent->d_name, "hwmon", 5) != 0)
      continue;
    snprintf(path, sizeof path, "/sys/class/hwmon/%s", dent->d_name);

    len = readlink(path, buf, sizeof buf);
    if (len > 14 && memcmp(buf, "../../devices/", 14) == 0) {
      char *start = buf + 14;
      char *end = strchr(start, '/');
      if (end)
        end = strchr(end + 1, '/');
      if (end)
        *end = '\0';
      snprintf(chip_label, sizeof chip_label, "%s", start);
    } else {
      snprintf(chip_label, sizeof chip_label, "unknown");
    }

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
