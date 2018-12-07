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

#ifndef NANO_EXPORTER_TEST_MOCK_SCRAPE_H_
#define NANO_EXPORTER_TEST_MOCK_SCRAPE_H_ 1

#include "harness.h"
#include "../scrape.h"

scrape_req *mock_scrape_start(test_env *env);
void mock_scrape_free(scrape_req *req);

void mock_scrape_expect(scrape_req *req, const char *metric, const struct label *labels, double value);
void mock_scrape_expect_raw(scrape_req *req, const char *str);
void mock_scrape_expect_no_more(scrape_req *req);

#endif // NANO_EXPORTER_TEST_MOCK_SCRAPE_H_
