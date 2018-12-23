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

#include <string.h>
#include <sys/statvfs.h>

#include "scrape.h"
#include "util.h"

// size of input buffer for paths and lines
#define BUF_SIZE 256

static void *filesystem_init(int argc, char *argv[]);
static void filesystem_collect(scrape_req *req, void *ctx);

const struct collector filesystem_collector = {
  .name = "filesystem",
  .collect = filesystem_collect,
  .init = filesystem_init,
  .has_args = true,
};

struct filesystem_context {
  struct slist *include_device;
  struct slist *exclude_device;
  struct slist *include_mount;
  struct slist *exclude_mount;
  struct slist *include_type;
  struct slist *exclude_type;
  int (*statvfs_func)(const char *path, struct statvfs *buf);
};

static void *filesystem_init(int argc, char *argv[]) {
  struct filesystem_context *ctx = must_malloc(sizeof *ctx);

  ctx->include_device = 0;
  ctx->exclude_device = 0;
  ctx->include_mount = 0;
  ctx->exclude_mount = 0;
  ctx->include_type = 0;
  ctx->exclude_type = 0;

  for (int arg = 0; arg < argc; arg++) {
    if (strncmp(argv[arg], "include-device=", 15) == 0) {
      ctx->include_device = slist_split(&argv[arg][15], ",");
    } else if (strncmp(argv[arg], "exclude-device=", 15) == 0) {
      ctx->exclude_device = slist_split(&argv[arg][15], ",");
    } else if (strncmp(argv[arg], "include-mount=", 14) == 0) {
      ctx->include_mount = slist_split(&argv[arg][14], ",");
    } else if (strncmp(argv[arg], "exclude-mount=", 14) == 0) {
      ctx->exclude_mount = slist_split(&argv[arg][14], ",");
    } else if (strncmp(argv[arg], "include-type=", 13) == 0) {
      ctx->include_type = slist_split(&argv[arg][13], ",");
    } else if (strncmp(argv[arg], "exclude-type=", 13) == 0) {
      ctx->exclude_type = slist_split(&argv[arg][13], ",");
    } else {
      fprintf(stderr, "unknown argument for filesystem collector: %s", argv[arg]);
      return 0;
    }
  }

  ctx->statvfs_func = statvfs;
  return ctx;
}

static void filesystem_collect(scrape_req *req, void *ctx_ptr) {
  struct filesystem_context *ctx = ctx_ptr;

  // buffers

  struct label labels[] = {  // all values filled by code
    { .key = "device", .value = 0 },
    { .key = "fstype", .value = 0 },
    { .key = "mountpoint", .value = 0 },
    LABEL_END,
  };
  char **dev = &labels[0].value;
  char **fstype = &labels[1].value;
  char **mount = &labels[2].value;

  char buf[BUF_SIZE];

  FILE *f;
  struct statvfs fs;

  // loop over /proc/mounts to get visible mounts

  f = fopen(PATH("/proc/mounts"), "r");
  if (!f)
    return;

  while (fgets_line(buf, sizeof buf, f)) {
    // extract device, mountpoint and filesystem type

    char *p;
    *dev = strtok_r(buf, " ", &p);
    if (!*dev)
      continue;
    *mount = strtok_r(0, " ", &p);
    if (!*mount)
      continue;
    *fstype = strtok_r(0, " ", &p);
    if (!*fstype)
      continue;

    if (ctx->include_device) {
      if (!slist_matches(ctx->include_device, *dev))
        continue;
    } else {
      if (**dev != '/')
        continue;
      if (ctx->exclude_device && slist_matches(ctx->exclude_device, *dev))
        continue;
    }
    if (ctx->include_mount) {
      if (!slist_matches(ctx->include_mount, *mount))
        continue;
    } else if (ctx->exclude_mount) {
      if (slist_matches(ctx->exclude_mount, *mount))
        continue;
    }
    if (ctx->include_type) {
      if (!slist_matches(ctx->include_type, *fstype))
        continue;
    } else if (ctx->exclude_type) {
      if (slist_matches(ctx->exclude_type, *fstype))
        continue;
    }

    // report metrics from statfs

    if (ctx->statvfs_func(*mount, &fs) != 0)
      continue;

    double bs = fs.f_frsize;
    scrape_write(req, "node_filesystem_avail_bytes", labels, fs.f_bavail * bs);
    scrape_write(req, "node_filesystem_files", labels, fs.f_files);
    scrape_write(req, "node_filesystem_files_free", labels, fs.f_ffree);
    scrape_write(req, "node_filesystem_free_bytes", labels, fs.f_bfree * bs);
    scrape_write(req, "node_filesystem_readonly", labels, fs.f_flag & ST_RDONLY ? 1.0 : 0.0);
    scrape_write(req, "node_filesystem_size_bytes", labels, fs.f_blocks * bs);
  }

  fclose(f);
}

#ifdef NANO_EXPORTER_TEST
void filesystem_test_override_statvfs(void *ctx, int (*statvfs_func)(const char *path, struct statvfs *buf)) {
  ((struct filesystem_context *) ctx)->statvfs_func = statvfs_func;
}
#endif // NANO_EXPORTER_TEST
