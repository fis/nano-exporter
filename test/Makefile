# Copyright 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

COLLECTOR_TESTS := cpu

COLLECTOR_TEST_PROGS := $(foreach c,$(COLLECTOR_TESTS),$(c)_test)
COLLECTOR_TEST_OBJS := $(foreach p,$(COLLECTOR_TEST_PROGS),$(p).o)
COLLECTOR_TEST_IMPLS := $(foreach p,$(COLLECTOR_TEST_PROGS),$(p).impl.o)

CFLAGS = -std=c11 -Wall -Wextra -pedantic -Wno-format-truncation -Os

# test execution

run_all: $(COLLECTOR_TEST_PROGS) run_tests.sh
	@./run_tests.sh $(COLLECTOR_TEST_PROGS)

.PHONY: run_all

$(COLLECTOR_TEST_OBJS): %.o: %.c harness.h mock_scrape.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(COLLECTOR_TEST_IMPLS): %_test.impl.o: ../%.c stub.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -include stub.h -c -o $@ $<

$(COLLECTOR_TEST_PROGS): %: %.o %.impl.o harness.o mock_scrape.o util.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

util.o: ../util.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

# make clean

.PHONY: clean
clean:
	$(RM) $(COLLECTOR_TEST_PROGS) $(COLLECTOR_TEST_OBJS) $(COLLECTOR_TEST_IMPLS)
	$(RM) harness.o mock_scrape.o util.o
