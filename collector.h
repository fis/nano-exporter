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
