#
# Makefile for the OCR Runtime on the x86-linux platform
#
# For OCR licensing terms, see top level LICENSE file.
#
# Author: Ivan Ganev <ivan.b.ganev@intel.com>
#

ifndef OCR_TYPE
OCR_TYPE=x86_newlib
else
ifneq (${OCR_TYPE}, x86_newlib)
$(error OCR_TYPE is set to ${OCR_TYPE} but expected x86_newlib)
endif
endif

DEFAULT_CONFIG=jenkins-common-8w-lockableDB.cfg

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

# CFLAGS_SHARED will be concatenated with any
# common CFLAGS options
CFLAGS_SHARED := -fPIC -std=c99 -isystem /usr/include -isystem ../../../apps/libs/x86_native/include

# Static libraries specific builds
# Same as for CFLAGS_SHARED
CFLAGS_STATIC := -fPIC -std=c99 -isystem /usr/include -isystem ../../../apps/libs/x86_native/include
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

#LIBC = /usr/lib64/libc.a
#PTHREAD_LIB = /usr/lib64/libpthread.a
#LIB_GCC = /usr/lib/gcc/x86_64-redhat-linux/4.8.3/libgcc_eh.a
LIBC = /usr/lib/x86_64-linux-gnu/libc.a
PTHREAD_LIB = /usr/lib/x86_64-linux-gnu/libpthread.a
LIB_GCC = /usr/lib/gcc/x86_64-linux-gnu/4.9/libgcc_eh.a

.PHONY: all
all: static shared
#	ld -static -o $(OBJDIR)/libocr.a --start-group $(OBJDIR)/static/*.o $(LIBC) $(PTHREAD_LIB) $(LIB_GCC) --end-group -zmuldefs
#	ld -shared -static -o $(OBJDIR)/libocr.so $(OBJDIR)/static/*.o -lc -lpthread --unresolved-symbols=ignore-all -zmuldefs
	ld -shared -static -o $(OBJDIR)/libocr.so $(OBJDIR)/static/*.o
	#nm -u $(OBJDIR)/myocr.o | grep -E "^.*U " | sed 's/^.*U //' > syms
	#nm $(OBJDIR)/myocr.o | grep " T " | grep "ocr" | sed 's/^.*T //' >> syms
	#ld -nostartfiles -nostdlib -nodefaultlibs -static -o $(OBJDIR)/ocr.o -r -s --start-group $(OBJDIR)/static/*.o $(LIBC) $(PTHREAD_LIB) --end-group -zmuldefs
	#objcopy -S --keep-symbols=syms $(OBJDIR)/myocr.o

.PHONY: debug
debug: debug-static debug-shared

include ../common.mk
