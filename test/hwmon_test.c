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

extern const struct collector hwmon_collector;

TEST(hwmon_metrics) {
  test_write_file(env, "sys/devices/virtual/hwmon/hwmon0/name", "acpitz\n");
  test_write_file(env, "sys/devices/virtual/hwmon/hwmon0/temp1_input", "27800\n");
  test_write_file(env, "sys/devices/virtual/hwmon/hwmon0/temp2_input", "29800\n");
  test_add_link(env, "sys/class/hwmon/hwmon0", "../../devices/virtual/hwmon/hwmon0");
  test_write_file(env, "sys/devices/platform/it87.2608/hwmon/hwmon1/name", "unused_name\n");
  test_write_file(env, "sys/devices/platform/it87.2608/hwmon/hwmon1/fan1_input", "1305\n");
  test_write_file(env, "sys/devices/platform/it87.2608/hwmon/hwmon1/fan1_min", "500\n");
  test_write_file(env, "sys/devices/platform/it87.2608/hwmon/hwmon1/fan1_alarm", "0\n");
  test_write_file(env, "sys/devices/platform/it87.2608/hwmon/hwmon1/fan2_input", "0\n");
  test_write_file(env, "sys/devices/platform/it87.2608/hwmon/hwmon1/fan2_min", "500\n");
  test_write_file(env, "sys/devices/platform/it87.2608/hwmon/hwmon1/fan2_alarm", "1\n");
  test_add_link(env, "sys/class/hwmon/hwmon1", "../../devices/platform/it87.2608/hwmon/hwmon1");
  test_write_file(env, "sys/class/hwmon/hwmon2/in0_input", "3020\n");
  test_write_file(env, "sys/class/hwmon/hwmon2/in0_min", "2700\n");
  test_write_file(env, "sys/class/hwmon/hwmon2/in0_max", "3300\n");
  test_write_file(env, "sys/class/hwmon/hwmon2/in0_alarm", "0\n");
  test_write_file(env, "sys/class/hwmon/hwmon2/in1_input", "2109\n");
  test_write_file(env, "sys/class/hwmon/hwmon2/in1_min", "2700\n");
  test_write_file(env, "sys/class/hwmon/hwmon2/in1_max", "3300\n");
  test_write_file(env, "sys/class/hwmon/hwmon2/in1_alarm", "1\n");
  scrape_req *req = mock_scrape_start(env);

  hwmon_collector.collect(req, 0);

  struct label *labels_acpitz_temp1 = LABEL_LIST({"chip", "hwmon/acpitz"}, {"sensor", "temp1"});
  struct label *labels_acpitz_temp2 = LABEL_LIST({"chip", "hwmon/acpitz"}, {"sensor", "temp2"});
  struct label *labels_it87_fan1 = LABEL_LIST({"chip", "platform/it87.2608"}, {"sensor", "fan1"});
  struct label *labels_it87_fan2 = LABEL_LIST({"chip", "platform/it87.2608"}, {"sensor", "fan2"});
  struct label *labels_unknown_in0 = LABEL_LIST({"chip", "unknown"}, {"sensor", "in0"});
  struct label *labels_unknown_in1 = LABEL_LIST({"chip", "unknown"}, {"sensor", "in1"});
  mock_scrape_sort(req);
  mock_scrape_expect(req, "node_hwmon_fan_alarm", labels_it87_fan1, 0);
  mock_scrape_expect(req, "node_hwmon_fan_alarm", labels_it87_fan2, 1);
  mock_scrape_expect(req, "node_hwmon_fan_min_rpm", labels_it87_fan1, 500);
  mock_scrape_expect(req, "node_hwmon_fan_min_rpm", labels_it87_fan2, 500);
  mock_scrape_expect(req, "node_hwmon_fan_rpm", labels_it87_fan1, 1305);
  mock_scrape_expect(req, "node_hwmon_fan_rpm", labels_it87_fan2, 0);
  mock_scrape_expect(req, "node_hwmon_in_alarm", labels_unknown_in0, 0);
  mock_scrape_expect(req, "node_hwmon_in_alarm", labels_unknown_in1, 1);
  mock_scrape_expect(req, "node_hwmon_in_max_volts", labels_unknown_in0, 3.3);
  mock_scrape_expect(req, "node_hwmon_in_max_volts", labels_unknown_in1, 3.3);
  mock_scrape_expect(req, "node_hwmon_in_min_volts", labels_unknown_in0, 2.7);
  mock_scrape_expect(req, "node_hwmon_in_min_volts", labels_unknown_in1, 2.7);
  mock_scrape_expect(req, "node_hwmon_in_volts", labels_unknown_in0, 3.02);
  mock_scrape_expect(req, "node_hwmon_in_volts", labels_unknown_in1, 2.109);
  mock_scrape_expect(req, "node_hwmon_temp_celsius", labels_acpitz_temp1, 27.8);
  mock_scrape_expect(req, "node_hwmon_temp_celsius", labels_acpitz_temp2, 29.8);
  mock_scrape_expect_no_more(req);
  mock_scrape_free(req);
}

TEST_SUITE {
  TEST_SUITE_START;
  RUN_TEST(hwmon_metrics);
  TEST_SUITE_END;
}
