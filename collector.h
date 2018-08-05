#ifndef PNANOE_COLLECTOR_H_
#define PNANOE_COLLECTOR_H_ 1

#include "stdbool.h"

#include "scrape.h"

struct collector {
  const char *name;
  void (*collect)(scrape_req *req, void *ctx);
  void *(*init)(int argc, char *argv[]);
  bool has_args;
};

#endif // PNANOE_COLLECTOR_H_
