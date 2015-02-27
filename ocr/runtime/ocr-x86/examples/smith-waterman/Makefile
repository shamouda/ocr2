PROG=smithwaterman
SUFFIX=.exec

all-test: compile run

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

TBB_LIBS=-ltbbmalloc_proxy -ltbbmalloc 
MIC_TBB_LIB_PATH=/opt/intel/composer_xe_2013_sp1.0.080/tbb/lib/mic/
MIC_TBB_LIBS=$(MIC_TBB_LIB_PATH)/libtbbmalloc_proxy.so $(MIC_TBB_LIB_PATH)/libtbbmalloc.so

PTHREAD_LIBS=/lib64/libpthread.so.0 
MIC_PTHREAD_LIBS=/opt/intel/mic/filesystem/base/lib64/libpthread.so.0 

instrument.o:
	$(CC) $(CFLAGS) -c -I$(PAPI_HOME)/include -I. instrument.c 

compile:
	$(CC) $(CFLAGS) $(OCR_FLAGS) -I. $(PROG).c -o $(PROG)$(SUFFIX)

compile.mic:
	icc -mmic -L${OCR_INSTALL}/lib -I${OCR_INSTALL}/include -fPIC -I. $(PROG).c -locr -o $(PROG).mmic$(SUFFIX)

run:
	./$(PROG)$(SUFFIX) 3072 3072 384 384

clean:
	-rm -Rf *.o $(PROG)$(SUFFIX)
