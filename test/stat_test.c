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

extern const struct collector stat_collector;

TEST(stat_metrics) {
  test_write_file(
      env,
      "proc/stat",
      "cpu  63127434 327625 23779819 2507475383 43094144 0 6968769 0 0 0\n"
      "cpu0 15869790 86128 5127633 626530909 11971750 0 3047872 0 0 0\n"
      "cpu1 16077564 83099 5249139 627646482 11108332 0 2155429 0 0 0\n"
      "intr 9977823731 9 0 0 0 0 0 0 0 1 0 0 0\n"
      "ctxt 17392647926\n"
      "btime 1538002179\n"
      "processes 9325143\n"
      "procs_running 1\n"
      "procs_blocked 0\n"
      "softirq 4290947107 27801943 1780530729 1705513 249559277 0 0 187155918 1259122343 22702 785048682\n");
  scrape_req *req = mock_scrape_start(env);

  stat_collector.collect(req, 0);

  mock_scrape_expect(req, "node_intr_total", 0, 9977823731);
  mock_scrape_expect(req, "node_context_switches_total", 0, 17392647926);
  mock_scrape_expect(req, "node_boot_time_seconds", 0, 1538002179);
  mock_scrape_expect(req, "node_forks_total", 0, 9325143);
  mock_scrape_expect(req, "node_procs_running", 0, 1);
  mock_scrape_expect(req, "node_procs_blocked", 0, 0);
  mock_scrape_expect_no_more(req);
  mock_scrape_free(req);
}

TEST_SUITE {
  TEST_SUITE_START;
  RUN_TEST(stat_metrics);
  TEST_SUITE_END;
}
