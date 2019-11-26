#!/usr/bin/make -f
#
# Developer's helper wrapper over autotools build system via GNU make, with
# extra pedantic compiler flags.
#
# Usage examples:
#
#  $ make D=1 O=2 CC=clang
#
#  $ make V=1 clean
#
# If you want to build in seperate 'build' directory, try:
#
#  $ make -C <build-dir> VPATH=$(pwd) -f $(pwd)/makefile <make-rule>
#

# User options:
# D = DEBUG (0|1)
# O = OPTLEVEL (0|1|2|3)
# V = VERBOSE (0|1)
D ?= 1
O ?= 0
V ?= 0

MKDIR_P := mkdir -p
SELF := $(lastword $(MAKEFILE_LIST))
NAME := $(notdir $(SELF))
TOP := $(realpath $(dir $(SELF)))
BUILDDIR := $(TOP)/build
PREFIXDIR := $(BUILDDIR)/local
VERSION := $(shell $(TOP)/version.sh --version)

CCS  := gcc clang
CXXS := g++ clang++
OPTS := 0 1 2 3
DBGS := 0 1
VRBS := 0 1

# Guard against environment variables
CFLAGS =
MAKE_OPTS =
CONFIGURE_OPTS =

# Vars setup
ifneq ($(CC), $(CCS))
CC := gcc
CXX := g++
endif

ifeq ($(CC), clang)
CXX := clang++
endif

ifneq ($(O), $(OPTS))
O := 0
endif

ifneq ($(D), $(DBGS))
D := 1
endif

ifneq ($(V), $(VRBS))
V := 0
endif

# Configure options
CONFIGURE_OPTS += --prefix=$(PREFIXDIR)
CONFIGURE_OPTS += --disable-shared

# Make options
ifeq ($(V), 0)
MAKE_OPTS += --silent --no-print-directory
endif


# Pedantic compilation flags
CFLAGS += -pedantic -Wall -Wextra -Winit-self -Wunused -Winline
CFLAGS += -Wshadow -Wfloat-equal -Wwrite-strings -Wpointer-arith
CFLAGS += -Wcast-align -Wsign-compare -Wredundant-decls -Wformat=2
CFLAGS += -Wmissing-include-dirs -Wmissing-declarations -Wswitch -Wswitch-enum
CFLAGS += -Wswitch-default -Wcomment -Wparentheses -Wsequence-point
CFLAGS += -Wpointer-arith -Wdisabled-optimization -Wmain -Wundef
CFLAGS += -Wunknown-pragmas -Wunused-macros -Wendif-labels -Wstrict-aliasing=2
CFLAGS += -Wvla -Waddress -Woverlength-strings -Wconversion -Wsign-conversion
CFLAGS += -Wunreachable-code -Wwrite-strings -Wlarger-than=4096
CFLAGS += -Wframe-larger-than=4096 -Wmissing-field-initializers
CFLAGS += -fstack-protector -fstack-check -fPIC -fwrapv -fstrict-aliasing

# C-Dialect compilation flags
CFLAGS += -Wbad-function-cast -Wmissing-prototypes -Waggregate-return
CFLAGS += -Wdeclaration-after-statement -Wnested-externs -Wstrict-prototypes
CFLAGS += -Wold-style-definition -Winit-self -std=gnu11

# Debug flags
CFLAGS += -g -ggdb -fno-omit-frame-pointer -DDEBUG=$(D)

# Optimization flags
ifeq ($(O), 0)
CFLAGS += -O0
ifeq ($(CC), gcc)
CFLAGS += -Wunsafe-loop-optimizations -funsafe-loop-optimizations
CFLAGS += -Wold-style-declaration
endif
else ifeq ($(O), 1)
CFLAGS += -O1
else ifeq ($(O), 2)
CFLAGS += -O2 -D_FORTIFY_SOURCE=2
else ifeq ($(O), 3)
CFLAGS += -O3
else
$(error Illegal O=$(O))
endif

# Compiler specific flags
ifeq ($(CC), gcc)
CFLAGS += -Werror -Wstack-usage=4096 -Wlogical-op
CFLAGS += -Wjump-misses-init -Wunsuffixed-float-constants
endif

ifeq ($(CC), clang)
endif


# Helper functions: report action & sub-execute make in build directory
define report
	@echo $(NAME)":" $(1)
endef

define submakeat
	@$(MAKE) $(MAKE_OPTS) V=$(V) CFLAGS="$(CFLAGS)" -C $(1) $(2)
endef

define submake
	$(call report, $@)
	$(call submakeat, $(BUILDDIR), $@)
endef


# Delegated make targtes
.PHONY: all install clean dist check

all: params configure
	$(call submake, $@)

check: params configure
	$(call submake, $@)

install: params configure
	$(call submake, $@)

clean: params
	$(call submake, $@)

dist: check
	$(call submake, $@)


# Pre-make targets:
.PHONY: configure bootstrap reset

configure: bootstrap
	$(call report, $@)
	@if [ ! -e $(BUILDDIR)/config.status ]; then \
            cd $(BUILDDIR) && $(TOP)/configure $(CONFIGURE_OPTS); fi

bootstrap:
	$(call report, $@)
	@if [ ! -d $(BUILDDIR) ]; then $(MKDIR_P) $(BUILDDIR); fi
	@if [ ! -e $(BUILDDIR)/config.status ]; then $(TOP)/bootstrap; fi

reset:
	$(call report, $@)
	$(RM) -r $(BUILDDIR)
	$(RM) -r $(TOP)/autom4te.cache
	$(RM) $(TOP)/configure
	$(RM) $(TOP)/aclocal.m4
	$(RM) $(TOP)/include/config.h.in
	find $(TOP)/ -type f -name Makefile.in -exec $(RM) {} \;

params:
	$(call report, $@)
	@echo "  VERSION="$(VERSION)
	@echo "  COMPILER="$(CC)
	@echo "  DEBUG="$(D)
	@echo "  OPTLEVEL="$(O)
	@echo "  VERBOSE="$(V)


# Help the naive user
.PHONY: help version

version:
	@$(TOP)/version.sh

help:
	@echo "Usage: "$(SELF)" [option=val] [<target>]"
	@echo ""
	@echo "Targets: "
	@echo "  all "
	@echo "  check "
	@echo "  clean "
	@echo "  dist "
	@echo "  configure "
	@echo "  reset "
	@echo "  version "
	@echo ""
	@echo "Options: "
	@echo "  CC (COMPILER): "$(CCS)
	@echo "  D (DEBUG): "$(DBGS)
	@echo "  O (OPTLEVEL): "$(OPTS)
	@echo "  V (VERBOSE): "$(VRBS)
	@echo ""



