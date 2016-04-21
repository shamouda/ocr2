#
# Makefile for the OCR Runtime on the x86-linux platform
#
# For OCR licensing terms, see top level LICENSE file.
#
# Author: Ivan Ganev <ivan.b.ganev@intel.com>
#

ifndef OCR_TYPE
  OCR_TYPE=tg-x86
else
  ifneq (${OCR_TYPE}, tg-x86)
    $(error OCR_TYPE is set to ${OCR_TYPE} but expected tg-x86)
  endif
endif

DEFAULT_CONFIG=mach-1block.cfg
#DEFAULT_CONFIG=mach-2block.cfg
#
# Tool-chain to be used for the build
#

TG_INSTALL ?= ../../../../tg/tg/install

FORCE_CC ?= no
ifeq ($(FORCE_CC), no)
  ifeq ($(CC), cc)
    CC = gcc
  endif
endif # End of ifeq force_cc

# TG_INSTALL/include is for rm-map.h
CFLAGS  := -fno-stack-protector -I $(TG_INSTALL)/include

RM      := rm
RMFLAGS := -rf

CP      := cp
MKDIR   := mkdir
LN      := ln

# Shared libraries specific builds
LDFLAGS := -shared -fpic -lpthread

# -lrt needed only for < glibc-2.17 in Linux systems (not needed for OS X)
UNAME := $(shell uname -s)
ifeq ($(UNAME),Linux)
  ret := $(shell echo "`ldd --version | awk '/ldd/{print $$NF}' | cut -d'.' -f1-2` < 2.17" | bc)
  ifeq ($(ret), 1)
    LDFLAGS += -lrt
  endif
endif

# CFLAGS_SHARED will be concatenated with any
# common CFLAGS options
CFLAGS_SHARED := -fpic

# Static libraries specific builds
# Same as for CFLAGS_SHARED
CFLAGS_STATIC :=
AR := ar
ARFLAGS := cru

RANLIB := ranlib

# Library supported
SUPPORTS_SHARED=yes
SUPPORTS_STATIC=yes
OCRRUNNER=ocrrun_$(OCR_TYPE)

# Valgrind compatibility for internal allocators
# x86 only
# Requires valgrind-devel package
# CFLAGS += -I/usr/include -DENABLE_VALGRIND

# Runtime overhead profiler
# x86 only
#
# Enable profiler
# CFLAGS += -DOCR_RUNTIME_PROFILER -DPROFILER_KHZ=3400000
#
# (optional) Maximum number of scope
# nesting for runtime profiler
# CFLAGS += -DMAX_PROFILER_LEVEL=512

# Enables data collection for execution timeline visualizer
# x86 only
# Requires -DOCR_ENABLE_EDT_NAMING and DEBUG_LVL_INFO
# CFLAGS += -DOCR_ENABLE_VISUALIZER -DOCR_ENABLE_EDT_NAMING

.PHONY: all
all: static shared

.PHONY: debug
debug: debug-static debug-shared

include ../common.mk
