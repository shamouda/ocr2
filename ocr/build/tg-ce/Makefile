#
# Makefile for the OCR Runtime on the x86-linux platform
#
# For OCR licensing terms, see top level LICENSE file.
#
# Author: Ivan Ganev <ivan.b.ganev@intel.com>
#

ifndef OCR_TYPE
OCR_TYPE=tg-ce
else
ifneq (${OCR_TYPE}, tg-ce)
$(error OCR_TYPE is set to ${OCR_TYPE} but expected tg-ce)
endif
endif
#
# Tool-chain to be used for the build
#

TG_INSTALL ?= ../../../../intel/ss/install

DEFAULT_CONFIG = ce-default.cfg


CC      := gcc
CFLAGS  := -fno-builtin -fno-stack-protector -Wno-unused-local-typedefs -I $(TG_INSTALL)/include

RM      := rm
RMFLAGS := -rf

CP      := cp
MKDIR   := mkdir
LN      := ln

# Shared libraries specific builds
LDFLAGS :=

# CFLAGS_SHARED will be concatenated with any
# common CFLAGS options
CFLAGS_SHARED :=

# Static libraries specific builds
# Same as for CFLAGS_SHARED
CFLAGS_STATIC := ${CFLAGS}
AR := ar
ARFLAGS := cru

RANLIB := ranlib

# Library supported
SUPPORTS_SHARED=no
SUPPORTS_STATIC=yes

.PHONY: all
all: static

.PHONY: debug
debug: debug-static


include ../common.mk
