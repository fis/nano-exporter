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

extern const struct collector meminfo_collector;

TEST(meminfo_metrics) {
  test_write_file(
      env,
      "proc/meminfo",
      "MemTotal:       16316872 kB\n"
      "MemFree:         1986284 kB\n"
      "MemAvailable:   11459644 kB\n"
      "Active(anon):    3434548 kB\n"
      "Inactive(anon):   417032 kB\n"
      "HugePages_Total:     123\n"
      "HugePages_Free:       12\n");
  scrape_req *req = mock_scrape_start(env);

  meminfo_collector.collect(req, 0);

  mock_scrape_expect(req, "node_memory_MemTotal_bytes", 0, 16708476928);
  mock_scrape_expect(req, "node_memory_MemFree_bytes", 0, 2033954816);
  mock_scrape_expect(req, "node_memory_MemAvailable_bytes", 0, 11734675456);
  mock_scrape_expect(req, "node_memory_Active_anon_bytes", 0, 3516977152);
  mock_scrape_expect(req, "node_memory_Inactive_anon_bytes", 0, 427040768);
  mock_scrape_expect(req, "node_memory_HugePages_Total", 0, 123);
  mock_scrape_expect(req, "node_memory_HugePages_Free", 0, 12);
  mock_scrape_expect_no_more(req);
  mock_scrape_free(req);
}

TEST_SUITE {
  TEST_SUITE_START;
  RUN_TEST(meminfo_metrics);
  TEST_SUITE_END;
}
