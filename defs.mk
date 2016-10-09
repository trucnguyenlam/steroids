
# Copyright (C) 2010--2016  Cesar Rodriguez <cesar.rodriguez@lipn.fr>
#
# This program is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation, either version 3 of the License, or any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program.  If not, see <http://www.gnu.org/licenses/>.


LLVMVERS=3.7
#LLVMCXXFLAGS=-I$(shell llvm-config-$(LLVMVERS) --includedir)
LLVMCXXFLAGS=$(shell llvm-config-$(LLVMVERS) --cppflags)
LLVMLDFLAGS=$(shell llvm-config-$(LLVMVERS) --ldflags)
#LLVMLIBS=$(shell llvm-config-$(LLVMVERS) --libs --system-libs)
LLVMLIBS=$(shell llvm-config-$(LLVMVERS) --libs all) -lz -lpthread -lffi -lncurses -ldl -lm

# traditional variables
#CFLAGS:=-Wall -Wextra -std=c11 -pg
#CFLAGS:=-Wall -Wextra -std=c11 -g
CFLAGS:=-Wall -std=c11 -g
#CXXFLAGS:=-Wall -Wextra -std=c++11 -O3
#CXXFLAGS:=-Wall -Wextra -std=c++11 -pg
#CXXFLAGS:=-Wall -Wextra -std=c++11 -g
CXXFLAGS:=-Wall -std=c++11 -g
#CPPFLAGS:=-I src/ -D_POSIX_C_SOURCE=200809L -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS -D NDEBUG
CPPFLAGS:=-I src/ -I include/ -D_POSIX_C_SOURCE=200809L -D__STDC_LIMIT_MACROS -D__STDC_FORMAT_MACROS $(LLVMCXXFLAGS)
#LDFLAGS:=-dead_strip -static
LDFLAGS:=$(LLVMLDFLAGS) -L src
LDLIBS=-lsteroids $(LLVMLIBS)

# ### library ###
LIB_SRCS:=$(wildcard src/*.c src/*.cc src/*/*.c src/*/*.cc src/*/*/*.c src/*/*/*.cc)
LIB_MSRCS:=
LIB_OBJS:=$(patsubst %.c,%.o,$(patsubst %.cc,%.o,$(LIB_SRCS)))
LIB_MOBJS:=$(patsubst %.c,%.o,$(patsubst %.cc,%.o,$(LIB_MSRCS)))
LIB_TARGETS:=src/libsteroids.a

# ### tools/test ###
TOOLS_TEST_SRCS:=$(wildcard tools/test/*.c tools/test/*.cc)
TOOLS_TEST_MSRCS:=tools/test/main.c
TOOLS_TEST_OBJS:=$(patsubst %.c,%.o,$(patsubst %.cc,%.o,$(TOOLS_TEST_SRCS)))
TOOLS_TEST_MOBJS:=$(patsubst %.c,%.o,$(patsubst %.cc,%.o,$(TOOLS_TEST_MSRCS)))
TOOLS_TEST_TARGETS:=$(TOOLS_TEST_MOBJS:.o=)

#TOOLS_TEST_SRCS:=$(wildcard tools/test/*.c tools/test/*.cc)
#TOOLS_TEST_MSRCS:=tools/test/main.c
#TOOLS_TEST_OBJS:=$(TOOLS_TEST_SRCS:.cc=.o)
#TOOLS_TEST_OBJS:=$(TOOLS_TEST_OBJS:.c=.o)
#TOOLS_TEST_MOBJS:=$(TOOLS_TEST_MSRCS:.cc=.o)
#TOOLS_TEST_MOBJS:=$(TOOLS_TEST_MOBJS:.c=.o)
#TOOLS_TEST_TARGETS:=$(TOOLS_TEST_MOBJS:.o=)

# ### global ###
OBJS=$(LIB_OBJS) $(TOOLS_TEST_OBJS)
MOBJS=$(LIB_MOBJS) $(TOOLS_TEST_MOBJS)
TARGETS=$(LIB_TARGETS) $(TOOLS_TEST_TARGETS)
DEPS:=$(patsubst %.o,%.d,$(OBJS) $(MOBJS))

# do not remove files generated by lex or bison once you generate them
.SECONDARY: src/cna/spec_lexer.cc src/cna/spec_parser.cc

# define the toolchain
CROSS:=
VERS:=-3.7
#VERS:=

LD:=$(CROSS)ld$(VERS)
CC:=$(CROSS)clang$(VERS)
CXX:=$(CROSS)clang++$(VERS)
CPP:=$(CROSS)cpp$(VERS)
LEX:=flex
YACC:=bison

%.d : %.c
	@echo "DEP $<"
	@set -e; $(CC) -MM -MT $*.o $(CFLAGS) $(CPPFLAGS) $< | \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@;

%.d : %.cc
	@echo "DEP $<"
	@set -e; $(CXX) -MM -MT $*.o $(CXXFLAGS) $(CPPFLAGS) $< | \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' > $@;

%.cc : %.l
	@echo "LEX $<"
	@$(LEX) -o $@ $^

%.cc : %.y
	@echo "YAC $<"
	@$(YACC) -o $@ $^

# cancelling gnu make builtin rules for lex/yacc to c
# http://ftp.gnu.org/old-gnu/Manuals/make-3.79.1/html_chapter/make_10.html#SEC104
%.c : %.l
%.c : %.y

%.o : %.c
	@echo "CC  $<"
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

%.o : %.cc
	@echo "CXX $<"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c -o $@ $<

%.pdf : %.dot
	@echo "DOT $<"
	@dot -T pdf < $< > $@

%.jpg : %.dot
	@echo "DOT $<"
	@dot -T jpg < $< > $@

CFLAGS_:=-Wall -Wextra -std=c11
CXXFLAGS_:=-Wall -Wextra -std=c++11 -O3

%.ll : %.c
	$(CC) $(CFLAGS_) $(CPPFLAGS) -S -flto $< -o $@
%.bc : %.c
	$(CC) $(CFLAGS_) $(CPPFLAGS) -c -flto $< -o $@
%.ll : %.cc
	$(CXX) $(CXXFLAGS_) $(CPPFLAGS) -S -flto $< -o $@
%.bc : %.cc
	$(CXX) $(CXXFLAGS_) $(CPPFLAGS) -c -flto $< -o $@
%.bc : %.ll
	llvm-as-$(LLVMVERS) $< -o $@
%.ll : %.bc
	llvm-dis-$(LLVMVERS) $< -o $@
%.s : %.bc
	llc-$(LLVMVERS) $< -o $@

