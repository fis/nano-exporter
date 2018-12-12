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

#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "harness.h"
#include "../util.h"

struct test_env {
  const char *current_testcase;
  jmp_buf escape_env;

  int tmpdir_fd;
  const char *tmpdir_name;

  int testdir_fd;
  char *testdir_name;
};

bool test_main(test_env *env);

static void testdir_setup(test_env *env);
static int testdir_getdir(test_env *env, const char *path);
static void testdir_closedir(test_env *env, int dir_fd);
static bool testdir_rmtree(int parent_fd, int dir_fd, const char *dir_name);

void test_write_file(test_env *env, const char *path, const char *contents) {
  if (*path == '/')
    test_fail(env, "test_write_file path is absolute: %s", path);
  const char *file = strrchr(path, '/');
  file = file ? file + 1 : path;

  testdir_setup(env);
  int dir_fd = testdir_getdir(env, path);
  int fd = openat(dir_fd, file, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd == -1)
    test_fail(env, "in %s: %s: unable to open for writing: %s", path, file, strerror(errno));
  testdir_closedir(env, dir_fd);

  if (write_all(fd, contents, strlen(contents)) == -1)
    test_fail(env, "in %s: %s: failed write: %s", path, file, strerror(errno));

  close(fd);
}

void test_add_link(test_env *env, const char *path, const char *target) {
  if (*path == '/')
    test_fail(env, "test_write_file path is absolute: %s", path);
  const char *file = strrchr(path, '/');
  file = file ? file + 1 : path;

  testdir_setup(env);
  int dir_fd = testdir_getdir(env, path);
  if (symlinkat(target, dir_fd, file) == -1)
    test_fail(env, "in %s: %s: unable to create a symlink to %s: %s", path, file, target, strerror(errno));
  testdir_closedir(env, dir_fd);
}

void test_fail(test_env *env, const char *err, ...) {
  va_list ap;

  printf("FAILED: ");
  va_start(ap, err);
  vprintf(err, ap);
  va_end(ap);
  printf("\n");

  longjmp(env->escape_env, 1);
}

bool test_start(test_env *env, const char *name) {
  printf("test: %s\n", name);
  env->current_testcase = name;

  env->testdir_fd = -1;
  env->testdir_name = 0;

  return true;
}

jmp_buf *test_escape(test_env *env) {
  return &env->escape_env;
}

bool test_cleanup(test_env *env) {
  int success = true;

  if (env->testdir_name) {
    success = success && testdir_rmtree(env->tmpdir_fd, env->testdir_fd, env->testdir_name);
    free(env->testdir_name);
  }

  if (!success)
    printf("FAILED: test cleanup failed\n");

  env->current_testcase = 0;
  return success;
}

int main(void) {
  test_env env;
  env.current_testcase = 0;
  env.tmpdir_fd = -1;

  bool success = test_main(&env);

  if (env.tmpdir_fd != -1)
    close(env.tmpdir_fd);

  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

static void testdir_setup(test_env *env) {
  char buf[64];

  if (env->testdir_name)
    return;  // already done

  if (env->tmpdir_fd == -1) {
    env->tmpdir_name = getenv("TMPDIR");
    if (!env->tmpdir_name)
      env->tmpdir_name = "/tmp";
    env->tmpdir_fd = open(env->tmpdir_name, O_RDONLY | O_DIRECTORY);
    if (env->tmpdir_fd == -1)
      test_fail(env, "failed to open temporary file directory: %s: %s", env->tmpdir_name, strerror(errno));
  }
  
  for (unsigned attempt = 0; attempt < 100; attempt++) {
    unsigned base = 0;
    for (const char *p = env->current_testcase; p && *p; p++)
      base = (base << 5) + base + (unsigned char) *p;
    snprintf(buf, sizeof buf, "nano_exporter_test_%x", base + attempt);
    if (mkdirat(env->tmpdir_fd, buf, 0700) == 0) {
      env->testdir_fd = openat(env->tmpdir_fd, buf, O_RDONLY | O_DIRECTORY);
      if (env->testdir_fd == -1)
        test_fail(env, "failed to open test work directory in %s: %s: %s", env->tmpdir_name, buf, strerror(errno));
      env->testdir_name = must_strdup(buf);
      if (fchdir(env->testdir_fd) == -1)
        test_fail(env, "unable to change to test work directory in %s: %s: %s", env->tmpdir_name, buf, strerror(errno));
      return;
    }
  }

  test_fail(env, "failed to create test work directory in %s: %s: %s", env->tmpdir_name, buf, strerror(errno));
}

static int testdir_getdir(test_env *env, const char *orig_path) {
  char buf[NAME_MAX + 1];
  struct stat st;

  int parent_fd = env->testdir_fd;

  const char *path = orig_path;
  char *slash;
  while ((slash = strchr(path, '/'))) {
    if (slash - path > (ptrdiff_t) sizeof buf - 1)
      test_fail(env, "path name component too long: %s", path);
    snprintf(buf, sizeof buf, "%.*s", (int)(slash - path), path);

    if (fstatat(parent_fd, buf, &st, AT_SYMLINK_NOFOLLOW) == 0) {
      if (!S_ISDIR(st.st_mode))
        test_fail(env, "in %s: %s: exists and not a directory", orig_path, buf);
      // already a directory, just try to open it
    } else if (errno != ENOENT) {
      test_fail(env, "in %s: %s: failed to stat: %s", orig_path, buf, strerror(errno));
    } else {
      // doesn't exist: make a new directory
      if (mkdirat(parent_fd, buf, 0700) == -1)
        test_fail(env, "in %s: %s: failed to mkdir: %s", orig_path, buf, strerror(errno));
    }

    int fd = openat(parent_fd, buf, O_RDONLY | O_DIRECTORY);
    if (fd == -1)
      test_fail(env, "in %s: %s: failed to open: %s", orig_path, buf, strerror(errno));

    if (parent_fd != env->testdir_fd)
      close(parent_fd);
    parent_fd = fd;

    path = slash + 1;
  }

  return parent_fd;
}

static void testdir_closedir(test_env *env, int dir_fd) {
  if (dir_fd != env->testdir_fd)
    close(dir_fd);
}

static bool testdir_rmtree(int parent_fd, int dir_fd, const char *dir_name) {
  DIR *d = fdopendir(dir_fd);
  if (!d) {
    close(dir_fd);
    return false;
  }

  bool success = true;

  struct dirent *dent;
  while ((dent = readdir(d))) {
    if (strcmp(dent->d_name, ".") == 0 || strcmp(dent->d_name, "..") == 0)
      continue;

    struct stat st;
    if (fstatat(dir_fd, dent->d_name, &st, AT_SYMLINK_NOFOLLOW) == -1) {
      success = false;
      continue;
    }

    if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
      if (unlinkat(dir_fd, dent->d_name, 0) == -1)
        success = false;
    } else if (S_ISDIR(st.st_mode)) {
      int fd = openat(dir_fd, dent->d_name, O_RDONLY | O_DIRECTORY);
      if (fd == -1 || !testdir_rmtree(dir_fd, fd, dent->d_name))
        success = false;
    } else {
      printf("unexpected item in bagging area: %s: not file, link or directory\n", dent->d_name);
      success = false;
    }
  }

  closedir(d);
  if (success)
    success = unlinkat(parent_fd, dir_name, AT_REMOVEDIR) == 0;

  return success;
}
