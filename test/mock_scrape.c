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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "harness.h"
#include "../scrape.h"
#include "../util.h"

#define MAX_METRICS 128
#define MAX_RAWS 16

struct scrape_metric {
  char *metric;
  struct label *labels;
  double value;
};

struct scrape_req {
  test_env *env;

  struct scrape_metric metrics[MAX_METRICS];
  unsigned metrics_written;
  unsigned metrics_tested;

  const char *raws[MAX_RAWS];
  unsigned raws_written;
  unsigned raws_tested;
};

static void dump_metrics(scrape_req *req);
static void dump_metric(const char *metric, const struct label *labels, double value);

static struct label *copy_labels(const struct label *labels);
static void free_labels(struct label *labels);
static void compare_labels(scrape_req *req, const struct label *got, const struct label *expected);

void scrape_write(scrape_req *req, const char *metric, const struct label *labels, double value) {
  if (req->metrics_written >= MAX_METRICS)
    test_fail(req->env, "exceeded MAX_METRICS: %u metrics already written", req->metrics_written);

  struct scrape_metric *rec = &req->metrics[req->metrics_written++];
  rec->metric = must_strdup(metric);
  rec->labels = copy_labels(labels);
  rec->value = value;
}

void scrape_write_raw(scrape_req *req, const void *buf, size_t len) {
  if (req->metrics_written >= MAX_RAWS)
    test_fail(req->env, "exceeded MAX_RAWS: %u raw blocks already written", req->raws_written);

  test_fail(req->env, "TODO: implement scrape_write_raw");
}

scrape_req *mock_scrape_start(test_env *env) {
  scrape_req *req = must_malloc(sizeof *req);
  req->env = env;
  req->metrics_written = req->metrics_tested = 0;
  req->raws_written = req->raws_tested = 0;
  return req;
}

void mock_scrape_free(scrape_req *req) {
  for (unsigned i = 0; i < req->metrics_written; i++) {
    free(req->metrics[i].metric);
    free_labels(req->metrics[i].labels);
  }
  free(req);
}

static int metric_cmp(const void *a_ptr, const void *b_ptr) {
  const struct scrape_metric *a = a_ptr;
  const struct scrape_metric *b = b_ptr;
  int c;

  c = strcmp(a->metric, b->metric);
  if (c != 0)
    return c;

  unsigned a_labels = 0, b_labels = 0;
  for (const struct label *p = a->labels; p && p->key; p++) a_labels++;
  for (const struct label *p = b->labels; p && p->key; p++) b_labels++;
  if (a_labels < b_labels)
    return -1;
  else if (a_labels > b_labels)
    return 1;
  for (const struct label *pa = a->labels, *pb = b->labels; pa && pa->key; pa++, pb++) {
    c = strcmp(pa->key, pb->key);
    if (c != 0)
      return c;
    c = strcmp(pa->value, pb->value);
    if (c != 0)
      return c;
  }

  if (a->value < b->value)
    return -1;
  else if (a->value > b->value)
    return 1;
  return 0;
}

void mock_scrape_sort(scrape_req *req) {
  if (req->metrics_written > 0)
    qsort(req->metrics, req->metrics_written, sizeof *req->metrics, metric_cmp);
}

void mock_scrape_expect(scrape_req *req, const char *metric, const struct label *labels, double value) {
  if (req->metrics_tested >= req->metrics_written) {
    dump_metrics(req);
    test_fail(req->env, "got no more metrics, expected %s", metric);
  }
  struct scrape_metric *got = &req->metrics[req->metrics_tested];

  if (strcmp(got->metric, metric) != 0) {
    dump_metrics(req);
    test_fail(req->env, "got metric %s, expected %s", got->metric, metric);
  }
  compare_labels(req, got->labels, labels);
  if (fabs(got->value - value) >= 1e-6) {
    dump_metrics(req);
    test_fail(req->env, "got metric value %.16g, expected %.16g", got->value, value);
  }

  req->metrics_tested++;
}

void mock_scrape_expect_raw(scrape_req *req, const char *str) {
  test_fail(req->env, "TODO: implement");
}

void mock_scrape_expect_no_more(scrape_req *req) {
  if (req->metrics_tested < req->metrics_written) {
    dump_metrics(req);
    test_fail(req->env, "got metric %s, expected no more metrics", req->metrics[req->metrics_tested].metric);
  }
  if (req->raws_tested < req->raws_written) {
    test_fail(req->env, "TODO: raw stuff");
  }
}

static void dump_metrics(scrape_req *req) {
  if (req->metrics_tested > 0) {
    printf("expected metrics:\n");
    for (unsigned i = 0; i < req->metrics_tested; i++) {
      struct scrape_metric *m = &req->metrics[i];
      dump_metric(m->metric, m->labels, m->value);
    }
  }

  if (req->metrics_written > req->metrics_tested) {
    printf("unexpected metrics:\n");
    for (unsigned i = req->metrics_tested; i < req->metrics_written; i++) {
      struct scrape_metric *m = &req->metrics[i];
      dump_metric(m->metric, m->labels, m->value);
    }
  }

  // TODO: dump raw
}

static void dump_metric(const char *metric, const struct label *labels, double value) {
  fputs("  ", stdout);
  fputs(metric, stdout);
  if (labels && labels->key) {
    fputc('{', stdout);
    for (const struct label *l = labels; l->key; l++) {
      if (l != labels)
        fputc(',', stdout);
      printf("%s=\"%s\"", l->key, l->value);
    }
    fputc('}', stdout);
  }
  printf(" %.16g\n", value);
}


static struct label *copy_labels(const struct label *labels) {
  if (!labels)
    return 0;

  size_t count = 0;
  for (const struct label *p = labels; p->key; p++)
    count++;
  if (!count)
    return 0;

  struct label *copy = must_malloc((count + 1) * sizeof *copy);
  for (size_t i = 0; i < count; i++) {
    copy[i].key = must_strdup(labels[i].key);
    copy[i].value = must_strdup(labels[i].value);
  }
  copy[count] = LABEL_END;

  return copy;
}

static void free_labels(struct label *labels) {
  if (!labels)
    return;
  for (struct label *p = labels; p->key; p++) {
    free(p->key);
    free(p->value);
  }
  free(labels);
}

static void compare_labels(scrape_req *req, const struct label *got, const struct label *expected) {
  if (!got || !got->key) {
    if (!expected || !expected->key)
      return;
    dump_metrics(req);
    test_fail(req->env, "got no more labels, expected %s=\"%s\"", expected->key, expected->value);
  }

  while (got->key && expected->key) {
    if (strcmp(got->key, expected->key) != 0 || strcmp(got->value, expected->value) != 0) {
      dump_metrics(req);
      test_fail(req->env, "got label %s=\"%s\", expected %s=\"%s\"", got->key, got->value, expected->key, expected->value);
    }
    got++;
    expected++;
  }

  if (got->key) {
    dump_metrics(req);
    test_fail(req->env, "got label %s=\"%s\", expected no more labels", got->key, got->value);
  }
  if (expected->key) {
    dump_metrics(req);
    test_fail(req->env, "got no more labels, expected %s=\"%s\"", expected->key, expected->value);
  }
}
