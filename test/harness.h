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

#ifndef NANO_EXPORTER_TEST_HARNESS_H_
#define NANO_EXPORTER_TEST_HARNESS_H_ 1

#include <setjmp.h>
#include <stdbool.h>
#include <stdlib.h>

typedef struct test_env test_env;

// functions for use from tests

void test_write_file(test_env *env, const char *path, const char *contents);
void test_add_link(test_env *env, const char *path, const char *target);

void test_fail(test_env *env, const char *err, ...);

// macros for defining test cases

#define TEST(name) void testcase_##name(test_env *env)

#define TEST_SUITE bool test_main(test_env *env)

#define TEST_SUITE_START volatile bool success = true

#define RUN_TEST(name) do { \
    if (!test_start(env, #name)) return false; \
    if (setjmp(*test_escape(env)) != 0) { success = false; break; } \
    testcase_##name(env); \
    if (!test_cleanup(env)) return false; \
  } while (0)

#define TEST_SUITE_END return success

// internal functions used by the macros

bool test_start(test_env *env, const char *name);
jmp_buf *test_escape(test_env *env);
bool test_cleanup(test_env *env);

#endif // NANO_EXPORTER_TEST_HARNESS_H_
