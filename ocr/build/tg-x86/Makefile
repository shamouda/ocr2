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
$(error tg-x86 support is not available for OCR v1.0)

#DEFAULT_CONFIG=mach-1block.cfg
DEFAULT_CONFIG=mach-2block.cfg
#
# Tool-chain to be used for the build
#

CC      := gcc
CFLAGS  := -fno-stack-protector

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
SUPPORTS_SHARED=yes
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

.PHONY: all
all: static shared

.PHONY: debug
debug: debug-static debug-shared

include ../common.mk
