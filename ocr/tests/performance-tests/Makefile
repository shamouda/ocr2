include defaults.mk
TYPE=x86

EXECUTABLE := $(wordlist 2, 2, $(MAKECMDGOALS))
GOAL := $(wordlist 1, 1, $(MAKECMDGOALS))
ifeq (run, $(GOAL))
 ifeq (, $(EXECUTABLE))
  $(error Executable name missing following keyword run)
 endif
endif

ifeq (benchmark, $(GOAL))
 ifeq (, $(EXECUTABLE))
  $(error Executable name missing following keyword benchmark)
 endif
 ifeq (, $(PROG))
  $(error PROG undefined)
 endif
endif

ifndef OCR_INSTALL
OCR_INSTALL?=../../install/x86
$(warning OCR_INSTALL not set, defaulting to ${OCR_INSTALL})
endif

ifndef OCR_CONFIG
OCR_CONFIG=${OCR_INSTALL}/config/default.cfg
$(warning OCR_CONFIG not set, defaulting to ${OCR_CONFIG})
endif

ifndef CC
CC=gcc
$(warning CC not set, defaulting to ${CC})
endif

ifndef BIN_GEN_DIR
BIN_GEN_DIR=build
endif

INCS=-I./inc
CFLAGS=-O3 -g -Wp,-w $(INCS) -c
OCR_FLAGS=-L${OCR_INSTALL}/lib -I${OCR_INSTALL}/include -locr -lpthread -lm ${OCR_LDFLAGS}

OCR_RUN_FLAGS=-ocr:cfg ${OCR_CONFIG}

DEPS=platform/$(TYPE).c

all: build

.PHONY: run

C_FILES=$(wildcard ocr/*.c)
OUT_FILES=$(subst ocr,$(BIN_GEN_DIR),$(C_FILES))
BIN_FILES=$(patsubst %.c,%,$(OUT_FILES))

RUN_FILES=$(subst ocr,run,$(C_FILES))
RUNBIN_FILES=$(patsubst %.c,%,$(RUN_FILES))

benchmark: $(PROG) $(BIN_GEN_DIR)/$(TYPE).o
	$(CC) -o $(EXECUTABLE) $(PROG) $(INCS) $(C_DEFINES) $(BIN_GEN_DIR)/$(TYPE).o $(OCR_FLAGS)

run: $(EXECUTABLE)
	LD_LIBRARY_PATH=$(OCR_INSTALL)/lib $(OCR_INSTALL)/bin/ocrrun $(EXECUTABLE) $(OCR_RUN_FLAGS)

$(BIN_GEN_DIR)/$(TYPE).o: $(DEPS)
	echo "Build $(CC) $(INCS) -g -c $< -o $@"
	$(CC) $(INCS) -g -c $< -o $@

build: $(BIN_FILES)
VPATH=ocr/
$(BIN_GEN_DIR)/%: %.c $(BIN_GEN_DIR)/$(TYPE).o
	$(CC) -o $@ $< $(INCS) $(C_DEFINES) $(BIN_GEN_DIR)/$(TYPE).o $(OCR_FLAGS)

clean:
	-rm -Rf build/*
