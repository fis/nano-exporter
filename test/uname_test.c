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

#include <string.h>
#include <sys/utsname.h>

#include "harness.h"
#include "mock_scrape.h"
#include "../collector.h"

extern const struct collector uname_collector;
void uname_test_override_data(void *ctx, struct utsname *name);

TEST(uname_metrics) {
  scrape_req *req = mock_scrape_start(env);
  struct utsname mock_uname;
  strcpy(mock_uname.machine, "x86_64");
  strcpy(mock_uname.nodename, "eris");
  strcpy(mock_uname.release, "4.18.0-1-amd64");
  strcpy(mock_uname.sysname, "Linux");
  strcpy(mock_uname.version, "#1 SMP Debian 4.18.6-1 (2018-09-06)");

  void *ctx = uname_collector.init(0, 0);
  uname_test_override_data(ctx, &mock_uname);
  uname_collector.collect(req, ctx);

  mock_scrape_expect(
      req,
      "node_uname_info",
      LABEL_LIST(
          {"machine", "x86_64"},
          {"nodename", "eris"},
          {"release", "4.18.0-1-amd64"},
          {"sysname", "Linux"},
          {"version", "#1 SMP Debian 4.18.6-1 (2018-09-06)"}),
      1);
  mock_scrape_expect_no_more(req);
  mock_scrape_free(req);
}

TEST_SUITE {
  TEST_SUITE_START;
  RUN_TEST(uname_metrics);
  TEST_SUITE_END;
}
