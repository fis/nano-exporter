#define _POSIX_C_SOURCE 200809L

#include <string.h>
#include <sys/statvfs.h>

#include "collector.h"
#include "util.h"

// size of input buffer for paths and lines
#define BUF_SIZE 256

static void filesystem_collect(scrape_req *req, void *ctx);

const struct collector filesystem_collector = {
  .name = "filesystem",
  .collect = filesystem_collect,
};

static void filesystem_collect(scrape_req *req, void *ctx) {
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

    if (**dev != '/')
      continue;
    // TODO: device, mountpoint, filesystem exclude lists

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
