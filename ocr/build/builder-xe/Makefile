#
# Makefile for the OCR Runtime on the x86-linux platform
#
# For OCR lixensing terms, see top level LICENSE file.
#
# Author: Ivan Ganev <ivan.b.ganev@intel.com>
#

ifndef OCR_TYPE
  OCR_TYPE=builder-xe
else
  ifneq (${OCR_TYPE}, builder-xe)
    $(error OCR_TYPE is set to ${OCR_TYPE} but expected builder-xe)
  endif
endif
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

CFLAGS  := -fno-builtin -fno-stack-protector -I ${TG_INSTALL}/include

RM      := rm
RMFLAGS := -rf

CP      := cp
MKDIR   := mkdir
LN      := ln

EXEFLAGS := -lelf -lpthread

# Shared libraries specific builds
LDFLAGS := -shared -fpic

# CFLAGS_SHARED will be concatenated with any
# common CFLAGS options
CFLAGS_SHARED := -fpic

# Static libraries specific builds
# Same as for CFLAGS_SHARED
CFLAGS_STATIC :=
AR := ar
ARFLAGS := cru

CFLAGS_EXEC := $(CFLAGS)

RANLIB := ranlib

# Library supported
SUPPORTS_SHARED=no
SUPPORTS_STATIC=no
SUPPORTS_EXEC=yes


# For Valgrind, uncomment this line
#CFLAGS += -I/usr/include -DENABLE_VALGRIND


.PHONY: all
all: exec

.PHONY: debug
debug: debug-exec

include ../common.mk
