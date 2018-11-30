#define _POSIX_C_SOURCE 200809L

#include <string.h>
#include <sys/statvfs.h>

#include "collector.h"
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

  return ctx;
}

static void filesystem_collect(scrape_req *req, void *ctx_ptr) {
  struct filesystem_context *ctx = ctx_ptr;

  // buffers

  const char *labels[][2] = {  // all filled by code
    { "device", 0 },
    { "fstype", 0 },
    { "mountpoint", 0 },
    { 0, 0 },
  };
  const char **dev = &labels[0][1];
  const char **fstype = &labels[1][1];
  const char **mount = &labels[2][1];

  char buf[BUF_SIZE];

  FILE *f;
  struct statvfs fs;

  // loop over /proc/mounts to get visible mounts

  f = fopen("/proc/mounts", "r");
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
      if (!slist_contains(ctx->include_device, *dev))
        continue;
    } else {
      if (**dev != '/')
        continue;
      if (ctx->exclude_device && slist_contains(ctx->exclude_device, *dev))
        continue;
    }
    if (ctx->include_mount) {
      if (!slist_contains(ctx->include_mount, *mount))
        continue;
    } else if (ctx->exclude_mount) {
      if (slist_contains(ctx->exclude_mount, *mount))
        continue;
    }
    if (ctx->include_type) {
      if (!slist_contains(ctx->include_type, *fstype))
        continue;
    } else if (ctx->exclude_type) {
      if (slist_contains(ctx->exclude_type, *fstype))
        continue;
    }

    // report metrics from statfs

    if (statvfs(*mount, &fs) != 0)
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
