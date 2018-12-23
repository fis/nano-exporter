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

extern const struct collector textfile_collector;

TEST(textfile_metrics) {
  test_write_file(
      env,
      "textfile/metrics.prom",
      "test_metric{label=\"value\"} 1234\n");
  scrape_req *req = mock_scrape_start(env);

  void *ctx = textfile_collector.init(1, (char *[]){ "dir=textfile", 0 });
  textfile_collector.collect(req, ctx);

  mock_scrape_expect_raw(req, "test_metric{label=\"value\"} 1234\n");
  mock_scrape_expect_no_more(req);
  mock_scrape_free(req);
}

TEST(appends_missing_newline) {
  test_write_file(
      env,
      "textfile/metrics.prom",
      "metric{newline=\"yes\"} 1234\n"
      "metric{newline=\"no\"} 4321");
  scrape_req *req = mock_scrape_start(env);

  void *ctx = textfile_collector.init(1, (char *[]){ "dir=textfile", 0 });
  textfile_collector.collect(req, ctx);

  mock_scrape_expect_raw(req, "metric{newline=\"yes\"} 1234\nmetric{newline=\"no\"} 4321\n");
  mock_scrape_expect_no_more(req);
  mock_scrape_free(req);
}

TEST_SUITE {
  TEST_SUITE_START;
  RUN_TEST(textfile_metrics);
  TEST_SUITE_END;
}
