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

#include "harness.h"
#include "mock_scrape.h"
#include "../collector.h"

extern const struct collector diskstats_collector;

TEST(diskstats_metrics) {
  test_write_file(
      env,
      "proc/diskstats",
      "   8       0 sda 1111 2222 3333 4444 5555 6666 7777 8888 9999 101010 111111 121212 131313 141414 151515\n"
      "   8      16 sdb 111 222 333 444 555 666 777 888 999 1010 1111 1212 1313 1414 1515\n");
  scrape_req *req = mock_scrape_start(env);

  void *ctx = diskstats_collector.init(0, 0);
  diskstats_collector.collect(req, ctx);

  struct label *labels;
  labels = LABEL_LIST({"device", "sda"});
  mock_scrape_expect(req, "node_disk_reads_completed_total",          labels, 1111.0);
  mock_scrape_expect(req, "node_disk_reads_merged_total",             labels, 2222.0);
  mock_scrape_expect(req, "node_disk_read_bytes_total",               labels, 1706496.0);
  mock_scrape_expect(req, "node_disk_read_time_seconds_total",        labels, 4.444);
  mock_scrape_expect(req, "node_disk_writes_completed_total",         labels, 5555.0);
  mock_scrape_expect(req, "node_disk_writes_merged_total",            labels, 6666.0);
  mock_scrape_expect(req, "node_disk_written_bytes_total",            labels, 3981824.0);
  mock_scrape_expect(req, "node_disk_write_time_seconds_total",       labels, 8.888);
  mock_scrape_expect(req, "node_disk_io_now",                         labels, 9999.0);
  mock_scrape_expect(req, "node_disk_io_time_seconds_total",          labels, 101.010);
  mock_scrape_expect(req, "node_disk_io_time_weighted_seconds_total", labels, 111.111);
  mock_scrape_expect(req, "node_disk_discards_completed_total",       labels, 121212.0);
  mock_scrape_expect(req, "node_disk_discards_merged_total",          labels, 131313.0);
  mock_scrape_expect(req, "node_disk_discarded_sectors_total",        labels, 141414.0);
  mock_scrape_expect(req, "node_disk_discard_time_seconds_total",     labels, 151.515);
  labels = LABEL_LIST({"device", "sdb"});
  mock_scrape_expect(req, "node_disk_reads_completed_total",          labels, 111.0);
  mock_scrape_expect(req, "node_disk_reads_merged_total",             labels, 222.0);
  mock_scrape_expect(req, "node_disk_read_bytes_total",               labels, 170496.0);
  mock_scrape_expect(req, "node_disk_read_time_seconds_total",        labels, 0.444);
  mock_scrape_expect(req, "node_disk_writes_completed_total",         labels, 555.0);
  mock_scrape_expect(req, "node_disk_writes_merged_total",            labels, 666.0);
  mock_scrape_expect(req, "node_disk_written_bytes_total",            labels, 397824.0);
  mock_scrape_expect(req, "node_disk_write_time_seconds_total",       labels, 0.888);
  mock_scrape_expect(req, "node_disk_io_now",                         labels, 999.0);
  mock_scrape_expect(req, "node_disk_io_time_seconds_total",          labels, 1.010);
  mock_scrape_expect(req, "node_disk_io_time_weighted_seconds_total", labels, 1.111);
  mock_scrape_expect(req, "node_disk_discards_completed_total",       labels, 1212.0);
  mock_scrape_expect(req, "node_disk_discards_merged_total",          labels, 1313.0);
  mock_scrape_expect(req, "node_disk_discarded_sectors_total",        labels, 1414.0);
  mock_scrape_expect(req, "node_disk_discard_time_seconds_total",     labels, 1.515);
  mock_scrape_expect_no_more(req);
  mock_scrape_free(req);
}

TEST_SUITE {
  TEST_SUITE_START;
  RUN_TEST(diskstats_metrics);
  TEST_SUITE_END;
}
