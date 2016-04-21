#
# Makefile for the OCR Runtime on the x86-linux platform
#
# For OCR licensing terms, see top level LICENSE file.
#
# Author: Ivan Ganev <ivan.b.ganev@intel.com>
#

ifndef OCR_TYPE
OCR_TYPE=x86-newlib
else
ifneq (${OCR_TYPE}, x86-newlib)
$(error OCR_TYPE is set to ${OCR_TYPE} but expected x86-newlib)
endif
endif

DEFAULT_CONFIG = jenkins-common-8w-lockableDB.cfg
APPS_LIBS_INSTALL_ROOT ?= ../../../../apps/apps/libs/install
ARCH ?= x86
APPS_LIBS_INSTALL := ${APPS_LIBS_INSTALL_ROOT}/${ARCH}

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
LDFLAGS := -shared -fpic -lpthread

CFLAGS_STATIC := -fpic -isystem /usr/include -isystem ${APPS_LIBS_INSTALL}/include

AR := ar
ARFLAGS := cru

RANLIB := ranlib

# Library supported
SUPPORTS_SHARED=no
SUPPORTS_STATIC=yes

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

# Enables data collection for execution timeline visualizer
# x86 only
# Requires -DOCR_ENABLE_EDT_NAMING and DEBUG_LVL_INFO
# CFLAGS += -DOCR_ENABLE_VISUALIZER -DOCR_ENABLE_EDT_NAMING

.PHONY: all
all: static

.PHONY: debug
debug: debug-static

include ../common.mk
