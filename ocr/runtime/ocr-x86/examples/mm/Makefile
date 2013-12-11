#
# written by Elkin Garcia
#

all: mm mm-power

OCR_FLAGS=-L${OCR_INSTALL}/lib -I${OCR_INSTALL}/include -locr
ifndef OCR_INSTALL
$(error OCR_INSTALL not set)
endif

OCR_CONFIG=test.cfg

OCR_RUN_FLAGS=-ocr:cfg ${OCR_CONFIG}
CPPFLAGS:=-DMEASURE
#CPPFLAGS:=$(CPPFLAGS) -DNOSIMPLIFY
#CPPFLAGS:=$(CPPFLAGS) -DNOSIMPLIFY -DCOMPARE

PCM_SRCS = msr.o cpucounters.o pci.o client_bw.o
MM_SRCS = mm.o
MM_PWR_SRCS = mm-power.o

TARGETS =	mm		\
		mm-power	

CC=g++ -Wall
OPT= -g -O3 
CC2=gcc -Wall

# uncomment if you want to rely on Linux perf support (user needs CAP_SYS_ADMIN privileges)
ifneq ($(wildcard /usr/include/linux/perf_event.h),)
#OPT+= -DPCM_USE_PERF 
endif

UNAME:=$(shell uname)

ifeq ($(UNAME), Linux)
LIB= -lpthread -lrt
endif
ifeq ($(UNAME), Darwin)
LIB= -lpthread -lPcmMsr
endif
ifeq ($(UNAME), FreeBSD)
LIB= -lpthread
endif


msr.o: msr.h msr.cpp 
	$(CC) $(OPT) -c msr.cpp

pci.o: pci.h pci.cpp
	$(CC) $(OPT) -c pci.cpp

client_bw.o: client_bw.h pci.h client_bw.cpp
	$(CC) $(OPT) -c client_bw.cpp

cpucounters.o: msr.h msr.cpp cpucounters.h cpucounters.cpp types.h width_extender.h 
	$(CC) $(OPT) -c cpucounters.cpp

mm.o:
	$(CC2) $(OPT) $(CPPFLAGS) -c $(OCR_FLAGS) -I. mm.c

mm-power.o: utils.h msr.h types.h pci.h cpucounters.h
	$(CC) $(OPT) $(CPPFLAGS) -fpermissive -c -DPOWERTRACING $(OCR_FLAGS) -I. mm.c -o mm-power.o

mm: $(MM_SRCS)
	$(CC2) $(OPT) -fpermissive $(MM_SRCS) $(OCR_FLAGS) -I. -o mm $(LIB)

mm-power: $(MM_PWR_SRCS) $(PCM_SRCS)
	$(CC) $(OPT) -fpermissive $(MM_PWR_SRCS) $(PCM_SRCS) -DPOWERTRACING $(OCR_FLAGS) -I. -o mm-power $(LIB)

run:
	./mm $(OCR_RUN_FLAGS) 4 1024 1024 128

									      
clean:
	rm -rf  *.o $(TARGETS) *~
