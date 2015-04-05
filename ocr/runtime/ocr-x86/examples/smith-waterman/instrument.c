#include <stdio.h>
#include <papi.h>
#include <unistd.h>
#include <sys/time.h>
#include <math.h>
#include <stdlib.h>

//#define TLB 1

//#define L3
#define L2_L3

#ifdef TLB
int events[1] = {PAPI_TLB_DM};
int eventnum = 1;
long long values[1];
int eventset;
#endif

#ifdef L2_L3
int events[4] = {PAPI_L2_DCM, PAPI_L2_DCA, PAPI_L3_TCM, PAPI_L3_DCA};
int eventnum = 4;
long long values[4];
int eventset;
#endif

#ifdef L3
int events[2] = {PAPI_L3_TCM, PAPI_L3_DCA};
int eventnum = 2;
long long values[2];
int eventset;
#endif

#ifdef L1
int events[1] = {PAPI_L1_DCM};
int eventnum = 1;
long long values[1];
int eventset;
#endif

#ifdef L2
int events[2] = {PAPI_L2_DCM, PAPI_L2_DCA};
int eventnum = 2;
long long values[2];
int eventset;
#endif

void instrument_init()
{
	if(PAPI_VER_CURRENT != PAPI_library_init(PAPI_VER_CURRENT)){
        printf("Can't initiate PAPI library!\n");
        exit(-1);
	}

  	eventset = PAPI_NULL;
  	if(PAPI_create_eventset(&eventset) != PAPI_OK){
        	printf("Can't create eventset!\n");
        	exit(-3);
  	}

  	if(PAPI_OK != PAPI_add_events(eventset, events, eventnum)){
        	printf("Can't add events! %d\n", PAPI_add_events(eventset, events, eventnum));
        	exit(-4);
  	}

}

void instrument_start()
{
	PAPI_start(eventset);
}

void instrument_end()
{
	PAPI_stop(eventset, values);
}

void instrument_print()
{
#ifdef TLB
printf(" Counter 1(TLB_DM):%lld \n",values[0]);
#endif

#ifdef L2_L3
fprintf(stderr,"L2DCM:%lld L2DCA:%lld\n",values[0], values[1]);
fprintf(stderr,"L3TCM:%lld L3DCA:%lld\n",values[2], values[3]);
#endif

#ifdef L3
printf(" Counter 1(L3TCM):%lld Counter2(L3DCA):%lld\n",values[0], values[1]);
#endif

#ifdef L1
printf(" Counter 1(L1DCM):%lld \n",values[0]);
#endif

#ifdef L2
	printf(" Counter 1(L2DCM):%lld Counter2(L2DCA):%lld\n",values[0], values[1]);
#endif
}

