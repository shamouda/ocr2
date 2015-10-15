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

DEFAULT_CONFIG=mach-hc-dist-gasnet-clone-8w.cfg

#
# Tool-chain to be used for the build
#

CC      := gcc
CFLAGS  :=

RM      := rm
RMFLAGS := -rf

CP      := cp
MKDIR   := mkdir
LN      := ln

# Shared libraries specific builds
LDFLAGS := -shared -fpic -lpthread -lrt

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
OCRRUNNER=ocrrun

# Valgrind compatibility for internal allocators
# x86 only
# Requires valgrind-devel package
# CFLAGS += -I/usr/include -DENABLE_VALGRIND

# Runtime overhead profiler
# x86 only
#
# Enable profiler
# CFLAGS += -DOCR_RUNTIME_PROFILER -DPROFILE_KHZ=3400000
#
# (optional) Maximum number of scope
# nesting for runtime profiler
# CFLAGS += -DMAX_PROFILER_LEVEL=512

# Enables the collection of EDT R/W statistics
# x86 only
# Requires OCR_ENABLE_EDT_NAMING
# CFLAGS += -DOCR_ENABLE_EDT_PROFILING

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

# ocrTests will pick that up if defined
#ifndef OCR_LDFLAGS
#	$(error OCR_LDFLAGS not specified)
#endif

#
# Gasnet setup
#

## Gasnet includes

CFLAGS += ${GASNET_CFLAGS}
GASNET_TYPE=GASNET_PAR
GASNET_INCL=${GASNET_ROOT}/include/${GASNET_CONDUIT}-conduit
GASNET_CFLAGS += -D${GASNET_TYPE} -I${GASNET_ROOT}/include -I${GASNET_INCL}

## Gasnet linking

GASNET_LIB=gasnet-$(GASNET_CONDUIT)-par
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
