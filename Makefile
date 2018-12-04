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

# enabled collectors

COLLECTORS =
COLLECTORS += cpu
COLLECTORS += diskstats
COLLECTORS += filesystem
COLLECTORS += hwmon
COLLECTORS += meminfo
COLLECTORS += network
COLLECTORS += stat
COLLECTORS += textfile
COLLECTORS += uname

# compile settings

CFLAGS = -std=c11 -Wall -Wextra -pedantic -Wno-format-truncation -Os
LDFLAGS = -Os -s

# build rules

PROG = nano-exporter
SRCS = main.c scrape.c util.c $(foreach c,$(COLLECTORS),$(c).c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

DEPDIR := .d
$(shell mkdir -p $(DEPDIR) >/dev/null)
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o : %.c
%.o : %.c $(DEPDIR)/%.d
	$(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<
	@mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

main.o: main.c $(DEPDIR)/main.d
	$(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) -DXCOLLECTORS="$(foreach c,$(COLLECTORS),X($(c)))" -c -o $@ $<
	@mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d && touch $@

# make clean

.PHONY: clean
clean:
	$(RM) $(PROG) $(OBJS)
	$(RM) -r $(DEPDIR)

# dependencies

$(DEPDIR)/%.d: ;
.PRECIOUS: $(DEPDIR)/%.d

include $(wildcard $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCS))))
