PROG=fibonacci
SUFFIX=exec

ifdef POLICY
POLICY_CFLAGS=-D${POLICY}
endif  

CFLAGS= -g -O3 ${POLICY_CFLAGS} #-DVERIFY #-DHPCTOOLKIT
MICFLAGS= -mmic -fPIC
OCR_FLAGS=-L${OCR_INSTALL}/lib -I${OCR_INSTALL}/include -locr
OCR_STATIC_FLAGS=-I$(OCR_INSTALL)/include $(OCR_INSTALL)/lib/libocr.a
HPC_TOOLKIT_FLAGS=-L${HPCTOOLKIT_PATH}/lib/hpctoolkit -I${HPCTOOLKIT_PATH}/include -lhpctoolkit

TBB_LIBS=-ltbbmalloc_proxy -ltbbmalloc 
MIC_TBB_LIB_PATH=/opt/intel/composer_xe_2013_sp1.0.080/tbb/lib/mic/
MIC_TBB_LIBS=$(MIC_TBB_LIB_PATH)/libtbbmalloc_proxy.so $(MIC_TBB_LIB_PATH)/libtbbmalloc.so

PTHREAD_LIBS=/lib64/libpthread.so.0 
MIC_PTHREAD_LIBS=/opt/intel/mic/filesystem/base/lib64/libpthread.so.0 

OCR_INPUT=${OCR_INPUT_HOME}

all: compile.ocr run
all-test: compile.ocr run verify

PAPI_HOME=$(HOME)/HJsvn/papi/BUILD
NO_CLUE_WHY_LIBS=-L$(HC_HOME)/lib -L$(PAPI_HOME)/lib -lpthread -lhc -lxml2 -lm -lpapi

instrument.o:
	$(CC) -O3 -c -I$(PAPI_HOME)/include -I. instrument.c 

#compile.<arch>_<model>_<alloc>_<source.suffixes>
#% can not be empty it seems
.SUFFIXES:

compile.ocr.tbb.static%:
	LD_PRELOAD=libtbbmalloc_proxy.so.2
	$(CC) $(CFLAGS) $(PROG).ocr$*c $(OCR_STATIC_FLAGS) $(PTHREAD_LIBS) $(TBB_LIBS) -o $(PROG).x86.ocr.tbb.static$*$(SUFFIX)

compile.ocr.static%:
	$(CC) $(CFLAGS) $(PROG).ocr$*c $(OCR_STATIC_FLAGS) $(PTHREAD_LIBS) -o $(PROG).x86.ocr.static$*$(SUFFIX)

compile.ocr.tbb%:
	LD_PRELOAD=libtbbmalloc_proxy.so.2
	$(CC) $(CFLAGS) $(PROG).ocr$*c $(OCR_FLAGS) $(TBB_LIBS) -o $(PROG).x86.ocr.tbb$*$(SUFFIX)

compile.ocr%:
	$(CC) $(CFLAGS) $(PROG).ocr$*c $(OCR_FLAGS) -o $(PROG).x86.ocr$*$(SUFFIX)

compile.serialized.tbb%:
	LD_PRELOAD=libtbbmalloc_proxy.so.2
	$(CC) $(CFLAGS) $(PROG).serialized$*c $(TBB_LIBS) -o $(PROG).x86.serialized.tbb$*$(SUFFIX)

compile.serialized%:
	$(CC) $(CFLAGS) $(PROG).serialized$*c -o $(PROG).x86.serialized$*$(SUFFIX)

compile.phi.ocr.tbb.static%:
	LD_PRELOAD=$(MIC_TBB_LIB_PATH)/libtbbmalloc_proxy.so.2
	$(CC) $(CFLAGS) $(MICFLAGS) $(PROG).ocr$*c $(OCR_STATIC_FLAGS) $(MIC_TBB_LIBS) $(MIC_PTHREAD_LIBS) -o $(PROG).mic.ocr.tbb.static$*$(SUFFIX)

compile.phi.ocr.tbb%:
	LD_PRELOAD=$(MIC_TBB_LIB_PATH)/libtbbmalloc_proxy.so.2
	$(CC) $(CFLAGS) $(MICFLAGS) $(PROG).ocr$*c $(OCR_FLAGS) $(MIC_TBB_LIBS) -o $(PROG).mic.ocr.tbb$*$(SUFFIX)

compile.phi.ocr.static%:
	$(CC) $(CFLAGS) $(MICFLAGS) $(PROG).ocr$*c  $(OCR_STATIC_FLAGS) $(MIC_PTHREAD_LIBS) -o $(PROG).mic.ocr.static$*$(SUFFIX)

compile.phi.ocr%:
	$(CC) $(CFLAGS) $(MICFLAGS) $(PROG).ocr$*c $(OCR_FLAGS) -o $(PROG).mic.ocr$*$(SUFFIX)

compile.phi.serialized.tbb%:
	LD_PRELOAD=libtbbmalloc_proxy.so.2
	$(CC) $(CFLAGS) $(MICFLAGS) $(PROG).serialized$*c $(MIC_TBB_LIBS) $(MIC_PTHREAD_LIBS) -o $(PROG).mic.serialized.tbb$*$(SUFFIX)

compile.phi.serialized%:
	$(CC) $(CFLAGS) $(MICFLAGS) $(PROG).serialized$*c -o $(PROG).mic.serialized$*$(SUFFIX)

run:
	./$(PROG).$(SUFFIX) ${MATRIX_SIZE} ${TILE_SIZE} ${OCR_INPUT}/${INPUT_FILE}

clean:
	-rm -Rf *.o $(PROG).$(SUFFIX) cholesky.out

verify:
	diff -b ${OCR_INPUT}/$(OUTPUT_FILE) cholesky.out
