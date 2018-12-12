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

#include <errno.h>
#include <string.h>
#include <sys/statvfs.h>

#include "harness.h"
#include "mock_scrape.h"
#include "../collector.h"

extern const struct collector filesystem_collector;
void filesystem_test_override_statvfs(void *ctx, int (*statvfs_func)(const char *path, struct statvfs *buf));

static void mock_statvfs_clear(void);
static void mock_statvfs_add(test_env *env, const char *path, struct statvfs *buf);
static int mock_statvfs_func(const char *path, struct statvfs *buf);

TEST(filesystem_metrics) {
  test_write_file(
      env,
      "proc/mounts",
      "/dev/mapper/vg00-root / ext4 rw,relatime,errors=remount-ro 0 0\n"
      "/dev/sda1 /boot ext2 rw,noatime 0 0\n"
      "/dev/mapper/fake /mnt/ro btrfs ro 0 0\n");
  mock_statvfs_clear();
  mock_statvfs_add(env, "/", &(struct statvfs){
      .f_frsize = 512,
      .f_blocks = 12345678, .f_bfree = 987654, .f_bavail = 876543,
      .f_files = 123456, .f_ffree = 98765, .f_favail = 87654,
      .f_flag = 0 });
  mock_statvfs_add(env, "/boot", &(struct statvfs){
      .f_frsize = 4096,
      .f_blocks = 1234567, .f_bfree = 98765, .f_bavail = 87654,
      .f_files = 12345, .f_ffree = 9876, .f_favail = 8765,
      .f_flag = 0 });
  mock_statvfs_add(env, "/mnt/ro", &(struct statvfs){
      .f_frsize = 512,
      .f_blocks = 2345678, .f_bfree = 87654, .f_bavail = 76543,
      .f_files = 23456, .f_ffree = 8765, .f_favail = 7654,
      .f_flag = ST_RDONLY });
  scrape_req *req = mock_scrape_start(env);

  void *ctx = filesystem_collector.init(0, 0);
  filesystem_test_override_statvfs(ctx, mock_statvfs_func);
  filesystem_collector.collect(req, ctx);

  struct label *labels;
  labels = LABEL_LIST({"device", "/dev/mapper/vg00-root"}, {"fstype", "ext4"}, {"mountpoint", "/"});
  mock_scrape_expect(req, "node_filesystem_avail_bytes", labels, 448790016);
  mock_scrape_expect(req, "node_filesystem_files", labels, 123456);
  mock_scrape_expect(req, "node_filesystem_files_free", labels, 98765);
  mock_scrape_expect(req, "node_filesystem_free_bytes", labels, 505678848);
  mock_scrape_expect(req, "node_filesystem_readonly", labels, 0);
  mock_scrape_expect(req, "node_filesystem_size_bytes", labels, 6320987136);
  labels = LABEL_LIST({"device", "/dev/sda1"}, {"fstype", "ext2"}, {"mountpoint", "/boot"});
  mock_scrape_expect(req, "node_filesystem_avail_bytes", labels, 359030784);
  mock_scrape_expect(req, "node_filesystem_files", labels, 12345);
  mock_scrape_expect(req, "node_filesystem_files_free", labels, 9876);
  mock_scrape_expect(req, "node_filesystem_free_bytes", labels, 404541440);
  mock_scrape_expect(req, "node_filesystem_readonly", labels, 0);
  mock_scrape_expect(req, "node_filesystem_size_bytes", labels, 5056786432);
  labels = LABEL_LIST({"device", "/dev/mapper/fake"}, {"fstype", "btrfs"}, {"mountpoint", "/mnt/ro"});
  mock_scrape_expect(req, "node_filesystem_avail_bytes", labels, 39190016);
  mock_scrape_expect(req, "node_filesystem_files", labels, 23456);
  mock_scrape_expect(req, "node_filesystem_files_free", labels, 8765);
  mock_scrape_expect(req, "node_filesystem_free_bytes", labels, 44878848);
  mock_scrape_expect(req, "node_filesystem_readonly", labels, 1);
  mock_scrape_expect(req, "node_filesystem_size_bytes", labels, 1200987136);
  mock_scrape_expect_no_more(req);
  mock_scrape_free(req);
}

TEST_SUITE {
  TEST_SUITE_START;
  RUN_TEST(filesystem_metrics);
  TEST_SUITE_END;
}

// helpers for mocking statvfs calls

#define MAX_STATVFS_MOCKS 16
static struct { const char *path; struct statvfs buf; } mock_statvfs_data[MAX_STATVFS_MOCKS];
static unsigned mock_statvfs_data_count = 0;

static void mock_statvfs_clear(void) {
  mock_statvfs_data_count = 0;
}

static void mock_statvfs_add(test_env *env, const char *path, struct statvfs *buf) {
  if (mock_statvfs_data_count >= MAX_STATVFS_MOCKS)
    test_fail(env, "too many statvfs(2) mocks");
  mock_statvfs_data[mock_statvfs_data_count].path = path;
  mock_statvfs_data[mock_statvfs_data_count].buf = *buf;
  mock_statvfs_data_count++;
}

static int mock_statvfs_func(const char *path, struct statvfs *buf) {
  for (unsigned i = 0; i < mock_statvfs_data_count; i++) {
    if (strcmp(mock_statvfs_data[i].path, path) == 0) {
      *buf = mock_statvfs_data[i].buf;
      return 0;
    }
  }

  errno = ENOENT;
  return -1;
}
