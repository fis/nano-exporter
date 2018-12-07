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

extern const struct collector cpu_collector;
void cpu_test_override_tick(void *ctx, long tick);

TEST(cpu_metrics) {
  test_write_file(
      env,
      "proc/stat",
      "cpu  1222 2444 3666 4888 6110 7332 8554 9776\n"
      "cpu0 1111 2222 3333 4444 5555 6666 7777 8888\n"
      "cpu1 111 222 333 444 555 666 777 888\n");
  scrape_req *req = mock_scrape_start(env);

  void *ctx = cpu_collector.init(0, 0);
  cpu_test_override_tick(ctx, 100);
  cpu_collector.collect(req, ctx);

  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "0"}, {"mode", "user"}), 11.11);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "0"}, {"mode", "nice"}), 22.22);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "0"}, {"mode", "system"}), 33.33);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "0"}, {"mode", "idle"}), 44.44);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "0"}, {"mode", "iowait"}), 55.55);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "0"}, {"mode", "irq"}), 66.66);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "0"}, {"mode", "softirq"}), 77.77);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "0"}, {"mode", "steal"}), 88.88);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "1"}, {"mode", "user"}), 1.11);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "1"}, {"mode", "nice"}), 2.22);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "1"}, {"mode", "system"}), 3.33);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "1"}, {"mode", "idle"}), 4.44);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "1"}, {"mode", "iowait"}), 5.55);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "1"}, {"mode", "irq"}), 6.66);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "1"}, {"mode", "softirq"}), 7.77);
  mock_scrape_expect(req, "node_cpu_seconds_total", LABEL_LIST({"cpu", "1"}, {"mode", "steal"}), 8.88);
  mock_scrape_expect_no_more(req);
  mock_scrape_free(req);
}

TEST(cpufreq_metrics) {
  test_write_file(env, "sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "1234567\n");
  test_write_file(env, "sys/devices/system/cpu/cpu1/cpufreq/scaling_cur_freq", "987654\n");
  scrape_req *req = mock_scrape_start(env);

  void *ctx = cpu_collector.init(0, 0);
  cpu_collector.collect(req, ctx);

  mock_scrape_expect(req, "node_cpu_frequency_hertz", LABEL_LIST({"cpu", "0"}), 1234567000.0);
  mock_scrape_expect(req, "node_cpu_frequency_hertz", LABEL_LIST({"cpu", "1"}), 987654000.0);
  mock_scrape_expect_no_more(req);
  mock_scrape_free(req);
}

TEST_SUITE {
  TEST_SUITE_START;
  RUN_TEST(cpu_metrics);
  RUN_TEST(cpufreq_metrics);
  TEST_SUITE_END;
}
