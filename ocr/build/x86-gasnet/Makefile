#
# Makefile for the OCR Runtime on the x86-linux platform
#
# For OCR licensing terms, see top level LICENSE file.
#
# Author: Ivan Ganev <ivan.b.ganev@intel.com>
#

ifndef OCR_TYPE
  OCR_TYPE=x86-gasnet
else
  ifneq (${OCR_TYPE}, x86-gasnet)
    $(error OCR_TYPE is set to ${OCR_TYPE} but expected x86-gasnet)
  endif
endif

DEFAULT_CONFIG=mach-x86-gasnet-affinity-8w-lockableDB.cfg

#
# Tool-chain to be used for the build
#

FORCE_CC ?= no
ifeq ($(FORCE_CC), no)
  ifeq ($(CC), cc)
    CC = gcc
  endif
endif # End of ifeq force_cc

CFLAGS  :=

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
SUPPORTS_SHARED=no
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

ifndef GASNET_ROOT
  $(error GASNET_ROOT not specified)
endif
ifndef GASNET_CONDUIT
  $(error GASNET_CONDUIT not specified)
endif
ifndef GASNET_TYPE
  $(error GASNET_TYPE not specified)
endif

########################################################################
# Gasnet specific settings
########################################################################
# example of variable settings:
# GASNET_ROOT=/scratch/la5/pkgs/gasnet
#
# examples for UDP:
# GASNET_CONDUIT=udp
# GASNET_EXTRA_LIBS=-lamudp
#
# examples for infiniband:
# GASNET_CONDUIT=ibv
# GASNET_EXTRA_LIBS=-libverbs

# ocrTests will pick 'OCR_LDFLAGS' if defined

ifeq ($(GASNET_TYPE),seq)
  GASNET_CFLAGS += -DGASNET_SEQ
else
  ifeq ($(GASNET_TYPE),par)
    GASNET_CFLAGS += -DGASNET_PAR
  else
    GASNET_CFLAGS += GASNET_PARSYNC
  endif
endif

#
# Gasnet setup
#

## Gasnet includes

GASNET_INCL=${GASNET_ROOT}/include/${GASNET_CONDUIT}-conduit
# -isystem instead of -I to avoid 'defined but not used' warnings from gasnet includes
GASNET_CFLAGS += -D${GASNET_TYPE} -isystem ${GASNET_ROOT}/include -I${GASNET_INCL}

## Gasnet linking

GASNET_LIB=gasnet-$(GASNET_CONDUIT)-$(GASNET_TYPE)
# Gasnet dynamic linking
GASNET_LDFLAGS = -L${GASNET_ROOT}/lib -L/usr/lib64 -lrt -l${GASNET_LIB} $(GASNET_EXTRA_LIBS)
# Gasnet static linking
GASNET_STATIC_FLAGS = ${GASNET_ROOT}/lib/lib${GASNET_LIB}.a

## Updating build flags

CFLAGS += ${GASNET_CFLAGS}

LDFLAGS += ${GASNET_LDFLAGS}

.PHONY: all
all: static

include ../common.mk
