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

extern const struct collector network_collector;

TEST(network_metrics) {
  test_write_file(
      env,
      "proc/net/dev",
      "Inter-|   Receive                                                |  Transmit\n"
      " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
      "    lo:  123456   12345  123  234  345   456        567       678   987654   98765  987  876  765   654     543        432\n"
      "  eno1:   12345    1234   12   23   34    45         56        67    98765    9876   98   87   76    65      54         43\n");
  scrape_req *req = mock_scrape_start(env);

  void *ctx = network_collector.init(0, 0);
  network_collector.collect(req, ctx);

  struct label *labels;
  labels = LABEL_LIST({"device", "eno1"});
  mock_scrape_expect(req, "node_network_receive_bytes_total", labels, 12345);
  mock_scrape_expect(req, "node_network_receive_packets_total", labels, 1234);
  mock_scrape_expect(req, "node_network_receive_errs_total", labels, 12);
  mock_scrape_expect(req, "node_network_receive_drop_total", labels, 23);
  mock_scrape_expect(req, "node_network_receive_fifo_total", labels, 34);
  mock_scrape_expect(req, "node_network_receive_frame_total", labels, 45);
  mock_scrape_expect(req, "node_network_receive_compressed_total", labels, 56);
  mock_scrape_expect(req, "node_network_receive_multicast_total", labels, 67); 
  mock_scrape_expect(req, "node_network_transmit_bytes_total", labels, 98765);
  mock_scrape_expect(req, "node_network_transmit_packets_total", labels, 9876);
  mock_scrape_expect(req, "node_network_transmit_errs_total", labels, 98);
  mock_scrape_expect(req, "node_network_transmit_drop_total", labels, 87);
  mock_scrape_expect(req, "node_network_transmit_fifo_total", labels, 76);
  mock_scrape_expect(req, "node_network_transmit_colls_total", labels, 65);
  mock_scrape_expect(req, "node_network_transmit_carrier_total", labels, 54);
  mock_scrape_expect(req, "node_network_transmit_compressed_total", labels, 43);
  mock_scrape_expect_no_more(req);
  mock_scrape_free(req);
}

TEST_SUITE {
  TEST_SUITE_START;
  RUN_TEST(network_metrics);
  TEST_SUITE_END;
}
