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

#ifndef NANO_EXPORTER_COLLECTOR_H_
#define NANO_EXPORTER_COLLECTOR_H_ 1

#include "stdbool.h"

#include "scrape.h"

struct collector {
  const char *name;
  void (*collect)(scrape_req *req, void *ctx);
  void *(*init)(int argc, char *argv[]);
  bool has_args;
};

#endif // NANO_EXPORTER_COLLECTOR_H_
