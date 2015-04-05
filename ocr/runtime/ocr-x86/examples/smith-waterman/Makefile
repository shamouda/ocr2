CC=icc
PROG=smith.waterman.generate.ocr.repeat
SUFFIX=.exec

ifdef INSTRUMENT
PAPI_CFLAGS=-DPAPI
PAPI_HOME=$(HOME)/HJsvn/papi/BUILD
PAPI_LIB_FLAGS=-L$(PAPI_HOME)/lib -lpapi
PAPI_OBJS=instrument.o
endif

CFLAGS= -g -O3 $(PAPI_CFLAGS) #-DHPCTOOLKIT
MICFLAGS= -mmic -fPIC

OCR_FLAGS=-L${OCR_INSTALL}/lib -I${OCR_INSTALL}/include -locr $(PAPI_LIB_FLAGS) $(PAPI_OBJS)
OCR_STATIC_FLAGS=-I$(OCR_INSTALL)/include $(OCR_INSTALL)/lib/libocr.a  $(PAPI_LIB_FLAGS) $(PAPI_OBJS)
HPC_TOOLKIT_FLAGS=-L${HPCTOOLKIT_PATH}/lib/hpctoolkit -I${HPCTOOLKIT_PATH}/include -lhpctoolkit

COMPOSER_PATH=/opt/apps/intel/psxe_2015_u2/composer_xe_2015.2.164/
TBB_LIBS=-ltbbmalloc_proxy -ltbbmalloc
MIC_TBB_LIB_PATH=$(COMPOSER_PATH)/tbb/lib/mic
MIC_TBB_LIBS=$(MIC_TBB_LIB_PATH)/libtbbmalloc_proxy.so $(MIC_TBB_LIB_PATH)/libtbbmalloc.so

PTHREAD_LIBS=/lib64/libpthread.so.0 
MIC_LIB_PATH=$(HOME)/mic.libs
MIC_PTHREAD_LIBS=$(MIC_LIB_PATH)/libpthread.so.0 

instrument.o:
	$(CC) $(CFLAGS) -c -I$(PAPI_HOME)/include -I. instrument.c 

compile.ocr.tbb.static: $(PAPI_OBJS)
	LD_PRELOAD=libtbbmalloc_proxy.so.2
	$(CC) $(CFLAGS) -I. $(PROG).c $(OCR_STATIC_FLAGS) $(PTHREAD_LIBS) $(TBB_LIBS) -o $(PROG).tbb.static$(SUFFIX)

compile.ocr.tbb: $(PAPI_OBJS)
	LD_PRELOAD=libtbbmalloc_proxy.so.2
	$(CC) $(CFLAGS) $(OCR_FLAGS) -I. $(PROG).c $(TBB_LIBS) -o $(PROG).tbb$(SUFFIX)

compile.ocr.static: $(PAPI_OBJS)
	$(CC) $(CFLAGS) -I. $(PROG).c $(OCR_STATIC_FLAGS) $(PTHREAD_LIBS) -o $(PROG).static$(SUFFIX)

compile.ocr: $(PAPI_OBJS)
	$(CC) $(CFLAGS) $(OCR_FLAGS) -I. $(PROG).c -o $(PROG)$(SUFFIX)

compile.mic:
	$(CC) $(CFLAGS) $(MICFLAGS) $(OCR_FLAGS) -I. $(PROG).c -o $(PROG).mmic$(SUFFIX)

run:
	LD_LIBRARY_PATH=$(OCR_INSTALL)/lib:$(LD_LIBRARY_PATH) ./$(PROG)$(SUFFIX) 3072 3072 384 384

clean:
	-rm -Rf *.o $(PROG)$(SUFFIX)
