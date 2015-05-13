# This file is subject to the license agreement located in the file LICENSE
# and cannot be distributed without it. This notice cannot be
# removed or modified.

# Get absolute paths. This is useful since we go in
# subfolders to do make and that screws up the relative
# paths
ifeq ($(RUN_JENKINS), run)
OCR_ROOT               :=
OCR_BUILD_ROOT        :=
OCR_BUILD             :=
WORKLOAD_BUILD_ROOT   :=
WORKLOAD_BUILD        :=
APPS_ROOT             :=
WORKLOAD_SRC          :=
else
OCR_ROOT               := $(shell cd "${OCR_ROOT}" && pwd)
OCR_BUILD_ROOT        := $(shell cd "${OCR_BUILD_ROOT}" && pwd)
OCR_BUILD             := ${OCR_BUILD_ROOT}/${OCR_TARGET}
WORKLOAD_BUILD_ROOT   := $(shell mkdir -p "${WORKLOAD_BUILD_ROOT}" && cd "${WORKLOAD_BUILD_ROOT}" && pwd)
WORKLOAD_BUILD        := ${WORKLOAD_BUILD_ROOT}/${OCR_TYPE}
APPS_ROOT             := $(shell cd "${APPS_ROOT}" && pwd)
WORKLOAD_SRC          := $(shell cd "${WORKLOAD_SRC}" && pwd)
endif

# Tool to use as a run wrapper
RUN_TOOL              ?=

APPS_LIBS_ROOT        := $(shell cd "${APPS_LIBS_ROOT}" && pwd)

HOSTNAME              := $(shell hostname)

OCR_INSTALL_ROOT      := $(shell mkdir -p "${OCR_INSTALL_ROOT}" && cd "${OCR_INSTALL_ROOT}" && pwd)
OCR_INSTALL      := ${OCR_INSTALL_ROOT}/${OCR_TARGET}

WORKLOAD_INSTALL_ROOT := $(shell mkdir -p "${WORKLOAD_INSTALL_ROOT}" && cd "${WORKLOAD_INSTALL_ROOT}" && pwd)
WORKLOAD_INSTALL := ${WORKLOAD_INSTALL_ROOT}/${OCR_TYPE}

REQ_LIBS ?=

CC ?= gcc
ifeq ($(CFLAGS_WARN_ERROR), no)
CFLAGS_BASE_WARN_ERROR =
else
CFLAGS_BASE_WARN_ERROR = -Werror
endif

CFLAGS_BASE = ${CFLAGS_BASE_WARN_ERROR} -I. -I${OCR_INSTALL}/include -I $(APPS_LIBS_ROOT)/include

LDFLAGS_BASE = -L ${OCR_INSTALL}/lib -L $(APPS_LIBS_ROOT)/lib -locr

CFLAGS  := $(CFLAGS_BASE) $(CFLAGS)
LDFLAGS := $(LDFLAGS_BASE) $(LDFLAGS)

STRIP   = strip

CUT = cut
GREP    = grep
RM  = rm
CP      = cp
MKDIR   = mkdir

OBJS := $(notdir $(SRCS:.c=.o))
OBJS := $(notdir $(OBJS:.cpp=.o))
OBJS := $(addprefix $(WORKLOAD_BUILD)/,$(OBJS))

FULL_REQ_LIBS := $(addprefix $(APPS_ROOT)/libs/src/,$(REQ_LIBS))

### Install rules ###

.PHONY: install_internal
.ONESHELL:
install_internal: preinstall | $(WORKLOAD_INSTALL)
	@echo "Installing from $(WORKLOAD_BUILD) into $(WORKLOAD_INSTALL)..."
	@echo ""
	@echo "####"
	@echo "To run, use 'OCR_TYPE=${OCR_TYPE} make run <args>' or, manually (in bash):"
	@echo "export OCR_INSTALL=${OCR_INSTALL} && export LD_LIBRARY_PATH=${OCR_INSTALL}/lib "\
	"&& export OCR_CONFIG=${OCR_INSTALL}/config/default.cfg (default one) and run "\
	"the executable $(TARGET) in $(WORKLOAD_INSTALL)"
	@echo "####"
	@$(RM) -f $(WORKLOAD_INSTALL)/$(TARGET)
	@$(CP) $(WORKLOAD_BUILD)/$(TARGET) $(WORKLOAD_INSTALL)/

postinstall: install_internal

.PHONY: install
install: postinstall

### Run rules ###

ifeq (yes, $(DEBUGMAKE))
.PHONY: debug_pre_prerun
debug_pre_prerun:
	@echo "Initial arguments are '${WORKLOAD_ARGS}' and inputs are '${WORKLOAD_INPUTS}'"

prerun: debug_pre_prerun

debug_post_prerun: prerun
	@echo "Post prerun arguments are '${WORKLOAD_ARGS}' and inputs are '${WORKLOAD_INPUTS}'"
else
debug_post_prerun: prerun

endif

.PHONY: run_internal
.ONESHELL:
run_internal: WORKLOAD_EXEC ?= $(WORKLOAD_INSTALL)
run_internal: OCR_CONFIG ?= $(OCR_INSTALL)/config/default.cfg
ifeq ($(RUN_JENKINS), run)
run_internal: debug_post_prerun
else
debug_post_prerun: install
run_internal: install debug_post_prerun
endif
ifeq ($(HOSTNAME), bar1)
	$(error Please refrain from running jobs on bar1!)
endif
	LD_LIBRARY_PATH=$(OCR_INSTALL)/lib:$(APPS_LIBS_ROOT) OCR_CONFIG=$(OCR_CONFIG) \
	bash -c 'cd $(WORKLOAD_EXEC) && $(RUN_TOOL) $(OCR_INSTALL)/bin/ocrrun ./$(TARGET) -ocr:cfg $(OCR_CONFIG) $(WORKLOAD_ARGS) $(WORKLOAD_INPUTS)'

postrun: run_internal

.PHONY: run
run: postrun

### Build rules ###

# Enforce order for build_internal
$(WORKLOAD_BUILD)/$(TARGET): prebuild
prebuild: reqlibs

# The next three rules force postbuild to run after build_internal
# and default to run postbuild
.PHONY: build_internal
build_internal: reqlibs prebuild $(WORKLOAD_BUILD)/$(TARGET)

postbuild: build_internal

.PHONY: default
default: postbuild

# Builds any libraries required by the workload
.PHONY: parallel-reqlibs
parallel-reqlibs: $(FULL_REQ_LIBS)

.PHONY: $(FULL_REQ_LIBS)
$(FULL_REQ_LIBS):
	lockfile $@/.lock; \
	ARCH=x86 APPS_ROOT=$(APPS_ROOT) APPS_LIBS_ROOT=$(APPS_LIBS_ROOT) \
	$(MAKE) -C $@ install; \
	rm -f $@/.lock

.PHONY: reqlibs
reqlibs: parallel-reqlibs

# Require libocr as this means all include files are up-to-date
$(WORKLOAD_BUILD)/%.o: $(WORKLOAD_SRC)/%.c | libocr $(WORKLOAD_BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(WORKLOAD_BUILD)/%.o: $(WORKLOAD_SRC)/%.cpp | libocr $(WORKLOAD_BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(WORKLOAD_BUILD)/$(TARGET).dbg: $(OBJS) | libocr $(WORKLOAD_BUILD)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(WORKLOAD_BUILD)/$(TARGET): $(WORKLOAD_BUILD)/$(TARGET).dbg | $(WORKLOAD_BUILD)
	@$(RM) -f $(WORKLOAD_BUILD)/$(TARGET)
	@$(CP) $(WORKLOAD_BUILD)/$(TARGET).dbg $(WORKLOAD_BUILD)/$(TARGET)

# libocr
# We make this a .PHONY target to force the re-evaluation of this
# rule in case source files change for OCR
.PHONY: libocr

ifeq ($(RUN_JENKINS), run)
libocr: ;
else ifeq ($(RUN_JENKINS), build)
# If we are running in Jenkins, we do not need to recheck if the file exists
libocr: $(OCR_INSTALL)/lib/libocr.so
$(OCR_INSTALL)/lib/libocr.so: $(OCR_BUILD)/Makefile
	@echo "Building libocr.so..."
	OCR_TYPE=$(OCR_TYPE) OCR_INSTALL=$(OCR_INSTALL) OCR_ROOT=$(OCR_ROOT) $(MAKE) -C $(OCR_BUILD) $(TARGETS) install
else
libocr: $(OCR_BUILD)/Makefile
	@echo "Checking on libocr.so..."
	OCR_TYPE=$(OCR_TYPE) OCR_INSTALL=$(OCR_INSTALL) OCR_ROOT=$(OCR_ROOT) $(MAKE) -C $(OCR_BUILD) $(TARGETS) install
endif

# If the visualizer is being used, add necessary CFLAGS to application's makefile
ifeq ($(ENABLE_VISUALIZER), yes)
CFLAGS += -DOCR_ENABLE_EDT_NAMING -DOCR_ENABLE_VISUALIZER
endif

.PHONY: clean
clean:
	-$(RM) -rf $(WORKLOAD_BUILD)

.PHONY: uninstall
uninstall:
	-$(RM) -rf $(WORKLOAD_INSTALL)

$(WORKLOAD_BUILD):
	mkdir -p $(WORKLOAD_BUILD)

$(WORKLOAD_INSTALL):
	mkdir -p $(WORKLOAD_INSTALL)

# Default rules. If the user defines them later, those recipes will be used
# Otherwise, the recipe does nothing by default

# The rules obviously run before/after the build, install and run phases
.PHONY: prebuild postbuild preinstall postinstall prerun postrun
prebuild:
postbuild:
preinstall:
postintall:
prerun:
postrun:
