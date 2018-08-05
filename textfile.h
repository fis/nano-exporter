#ifndef PNANOE_TEXTFILE_H_
#define PNANOE_TEXTFILE_H_ 1

#include "collector.h"

extern const struct collector textfile_collector;

void *textfile_init(int argc, char *argv[]);
void textfile_collect(scrape_req *req, void *ctx);

#endif // PNANOE_TEXTFILE_H_
