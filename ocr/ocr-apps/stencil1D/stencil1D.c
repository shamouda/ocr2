/*
This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed withs file is subject to the license agreement located in the file LICENSE
 *  *  * and cannot be distributed without it. This notice cannot be
 *   *   * removed or modified.


This code does a 1D (3 point) stencil computation on a set of points in pure ocr

N is the number of tasks
M is the number of points in each task
I is the number of iterations

the datapoints are initialized to zero except the boundary values which are set to 1
the particular stencil implemented is anew(i) = .5*(a(i+1) + a(i-1))
which implements an elliptic solver using the 2nd order three point finite difference.
The values converge (slowly) to all 1s.

if the stencil is wider than 3 points, then the number of shared boundary values
would have to be increased.


Control structure
  N stenciltask EDTs do the computation on M numbers each
  Each stencil task has a "real" datablock for computing values
  Each stencil has a private datablock to hold parameters
  Each stenciltask "clones" itself for each iteration
  Each stenciltask has two data blocks for sending to each neighbor
   (the blocks alternate each time step to break a race condition)
  Neighboring stencil tasks share sticky events.
    The receiver uses the sticky event as a dependence for its clone
    The sender satisfies the event with the boundary data
    THe sender creates the sticky event for the next exchange and puts it in the datablok
    The receiver is responsible for destroying the used sticky event


Structure of code

mainEDT
  creates N datablocks of length 2M
  creates 4*N-4 datablocks for exchanging halo data and control information
  N "private" datablocks for local parameters
  launches wrapup that depends on realmain finishing
  launches realmain as a finish EDT

realmain
  creates N stencilEdts
  creates 2N-2 sticky events that will be used to pass the first boundary blocks
  launches the N stencilEdt with the correct datablocks attached


Each stencil event
  if(iteration == 0) initializes data
  destroys the previous sticky events
  computes its new values using neighbor values as needed
  writes its boundary value(s) to the out datablock(s)
  creates the sticky events and puts them in the out datablocks
  enables its nephews by satisfying the previous sticky events with
    the out datablocks
  clones itself for the next iteration
  launches its clone by attaching 4 datablocks and 2 sticky events

The sticky events prevent the sons from starting until their uncle(s)
are finished writing and making the data available.

They have to be sticky events to avoid the race condition that they
may be satisfied before the nephews are created

The boundary data blocks alternate each time step to make sure that my son
doesn't overwrite my data before my nephew has read it

The blocks that stencilEdt is writing to are acquired in ITW mode
The blocks that stencilEdt is reading are acquired in RO mode.

The values after final iteration are printed out in order by a wrapupEdt.
To make sure wrapup doesn't start early, it depends on realmain
which is declared to be a finish event (which means it
doesn't satisfy its output event until ALL of its descendents are finished).


*/

#include <ocr.h>
#include <stdio.h>

#define N 3  //number of blocks
#define M 3   // size of local block
#define I 5  //number of iterations

typedef struct{
    double buffer;
    ocrGuid_t link; //sticky event for control
    } buffer_t;

typedef struct{
    u64 timestep;
    u64 mynode;
    ocrGuid_t leftevent;
    ocrGuid_t rightevent;
    ocrGuid_t leftold;
    ocrGuid_t rightold;
    } private_t;



ocrGuid_t stencilTask(u32 paramc, u64 * paramv, u32 depc, ocrEdtDep_t depv[]) {

/*
Parameters:
0: stencilTemplate

Dependencies:
0: My datablock
1: Leftin block (satisfied by my left neighbor)
2: Leftout block
3: Rightout block
4: Rightin block (satisfied by my right neighbor)
5: My private block
*/


    u64 i, phase, phasenext;

    ocrGuid_t stencilEdt;
    ocrGuid_t sticky;


    ocrGuid_t stencilTemplate = paramv[0];

    double * a = depv[0].ptr;
    double * atemp = a + M;

    buffer_t * leftin = depv[1].ptr;
    buffer_t * rightin = depv[2].ptr;
    private_t * private = depv[3].ptr;

    u64 mynode = private->mynode;
    u64 timestep = private->timestep;
    ocrGuid_t  leftevent = private->leftevent;
    ocrGuid_t  rightevent = private->rightevent;

    ocrGuid_t leftinEvent = NULL_GUID;
    ocrGuid_t rightinEvent = NULL_GUID;

//printf("ST%d T%d start\n", mynode, timestep);
//printf("ST%d T%d depv[0] %d \n", mynode, timestep, depv[0].guid);
//printf("ST%d T%d depv[1] %d \n", mynode, timestep, depv[1].guid);
//printf("ST%d T%d depv[2] %d \n", mynode, timestep, depv[2].guid);
//printf("ST%d T%d depv[3] %d \n", mynode, timestep, depv[3].guid);
//fflush(stdout);

    if(private->leftold != NULL_GUID) ocrEventDestroy(private->leftold);
    if(private->rightold != NULL_GUID) ocrEventDestroy(private->rightold);


    if(timestep == 0) { //initialize
        if(leftin == NULL) a[0] = 1;
          else {
            a[0] = 0;
            leftin->buffer = 0;
        }

    for(i=1;i<M-1;i++) a[i] = 0;

    if(rightin == NULL) a[M-1] = 1;
        else {
        a[M-1] = 0;
        rightin->buffer = 0;
        }
//printf("ST%d T%d after init \n", mynode, timestep);
//fflush(stdout);
    }

    if(timestep < I) {


//compute

        if(leftin != NULL) atemp[0] =  .5*a[0] + .25*(a[1] + leftin->buffer);
            else atemp[0] = a[0];

        for(i=1;i<M-1;i++) atemp[i] =  0.5*a[i] + 0.25*(a[i+1] + a[i-1]);
            if(rightin != NULL) atemp[M-1] = 0.5* a[M-1] + .25*( (rightin->buffer) + a[M-2]);
                else atemp[M - 1] = a[M-1];

//could optimized by changing a pointer rather than actually copying
        for(i=0;i<M;i++) a[i] = atemp[i];

//printf("ST%d T%d after compute %f %f %f \n", mynode, timestep, a[0], a[1], a[2]);
//fflush(stdout);




        private->timestep = timestep+1;

        if(leftin != NULL) { //leftin is NULL too
            leftinEvent = leftin->link;
            leftin->buffer = a[0];
            ocrEventCreate(&sticky, OCR_EVENT_STICKY_T, true);
            leftin->link = sticky;
            ocrEventSatisfy(private->leftevent, depv[1].guid);
            private->leftevent = sticky;
        }

//printf("ST%d T%d after send left\n", mynode, timestep);
//fflush(stdout);

        if(rightin != NULL) {
            rightinEvent = rightin->link;
            rightin->buffer = a[M-1];
            ocrEventCreate(&sticky, OCR_EVENT_STICKY_T, true);
            rightin->link = sticky;
            ocrEventSatisfy(private->rightevent, depv[2].guid);
            private->rightevent = sticky;
        }

        private->leftold = leftinEvent;
        private->rightold = rightinEvent;

//printf("ST%d T%d after send right \n", mynode, timestep);
//fflush(stdout);

//create clone

        ocrEdtCreate(&stencilEdt, stencilTemplate, EDT_PARAM_DEF, (u64 *) &stencilTemplate,
                EDT_PARAM_DEF, NULL, EDT_PROP_NONE, NULL_GUID, NULL_GUID);

        ocrDbRelease(depv[0].guid);
        ocrAddDependence(depv[0].guid, stencilEdt, 0 , DB_MODE_ITW);

        ocrAddDependence(leftinEvent, stencilEdt, 1 , DB_MODE_RO);
        ocrAddDependence(rightinEvent, stencilEdt, 2 , DB_MODE_ITW);

        ocrDbRelease(depv[3].guid);
        ocrAddDependence(depv[3].guid, stencilEdt, 3 , DB_MODE_ITW);

    }
//printf("ST%d T%d finish\n", mynode, timestep);
//fflush(stdout);

    return NULL_GUID;
}


ocrGuid_t wrapupTask(u32 paramc, u64 *paramv, u32 depc, ocrEdtDep_t depv[]) {
/*
waits until realmain is done
prints out the "answer"

params: none

dependencies
0-(N-1) the datablocks in order
N: output event of realmain

*/

    u64 i, j;
    double * data[N];
    for(i=0;i<N;i++) {
        data[i] = depv[i].ptr;
        for(j=0;j<M;j++) PRINTF("%lld %lld %f \n", i, j, data[i][j]);
        }

    ocrShutdown();
    return NULL_GUID;
}


ocrGuid_t realmainTask(u32 paramc, u64 *paramv, u32 depc, ocrEdtDep_t depv[]){
/*
creates the first set of sticky events
creates N stencilEdts
passes in the main datablock and the correct set of 4 buffer blocks (or NULL_GUID)
passes in the private block

params: none
dependencies:
0 - (N-1) the N real datablocks
N - (5N-5): the 2*(2*N-2) communication datablocks
  2*N-2 of them will be passed as dependencies to the stencilEdts
  2*N-2 of them will be passed in as parameters for the next iteration

*/


    u64 i, mynode;
    ocrGuid_t stencilTemplate, stencilEdt[N];
    ocrEdtTemplateCreate(&stencilTemplate, stencilTask, 1, 4);
    u64 *dummy;
    u64 j;
    ocrGuid_t sticky;

    double * a[N];
    buffer_t * buffer[2*N-2];
    private_t * private[N];

//printf("R  start realmain\n");
//fflush(stdout);


    for(i=0;i<N;i++) a[i] = depv[i].ptr;
    for(i=N;i<3*N-2;i++) buffer[i-N] = depv[i].ptr;
    for(i=0;i<N;i++) private[i] = depv[3*N-2+i].ptr;

//printf("R  after unpack\n");
//fflush(stdout);

    u64 lin =  0;
    u64 rin =  lin + N - 1;

    private[0]->leftevent = NULL_GUID;
    private[0]->leftold = NULL_GUID;

    for(i=0;i<N-1;i++) {
//printf("R%d  start \n", i);
//fflush(stdout);
        private[i]->mynode = i;
//printf("R%d  after mynode \n", i);
//fflush(stdout);
        private[i]->timestep = 0;
//printf("R%d  after timestep\n", i);
//fflush(stdout);

        ocrEventCreate(&sticky, OCR_EVENT_STICKY_T, true);
//printf("R%d  after sticky create %d \n", i, sticky);
//fflush(stdout);
        private[i]->rightevent = sticky;
        buffer[lin++]->link = sticky;
        private[i]->rightold = NULL_GUID;
//printf("R%d  after right\n", i);
//fflush(stdout);

        ocrEventCreate(&sticky, OCR_EVENT_STICKY_T, true);
//printf("R%d  after sticky create %d \n", i, sticky);
//fflush(stdout);
        private[i+1]->leftevent = sticky;
        buffer[rin++]->link = sticky;
        private[i+1]->leftold = NULL_GUID;
//printf("R%d  after after left \n", i);
//fflush(stdout);
    }

    private[N-1]->mynode = N-1;
    private[N-1]->timestep = 0;
    private[N-1]->rightevent = NULL_GUID;
    private[N-1]->rightold = NULL_GUID;

//printf("R  after events \n");
//fflush(stdout);


//create N stencil init events, attach the data db

    for(i=0;i<4*N-2;i++) ocrDbRelease(depv[i].guid);  //release all dbs

    for(i=0;i<N;i++) ocrEdtCreate(&stencilEdt[i], stencilTemplate, EDT_PARAM_DEF, (u64 *) &stencilTemplate, EDT_PARAM_DEF, NULL, EDT_PROP_NONE, NULL_GUID, NULL_GUID);

//printf("R  after EDT create\n");
//fflush(stdout);


    lin = N;
    rin =  lin + N - 1;

    u64 pbuffer = 3*N-2;

    ocrAddDependence(NULL_GUID, stencilEdt[0], 1, DB_MODE_RO);

    for(i=0;i<N-1;i++){
//printf("R i %d lin %d rin %d pbuffer %d \n", i, lin, rin, pbuffer);
//fflush(stdout);

        ocrAddDependence(depv[i].guid, stencilEdt[i], 0, DB_MODE_ITW);
        ocrAddDependence(depv[rin++].guid, stencilEdt[i], 2, DB_MODE_ITW);
        ocrAddDependence(depv[pbuffer++].guid, stencilEdt[i], 3, DB_MODE_ITW);

        ocrAddDependence(depv[lin++].guid, stencilEdt[i+1], 1, DB_MODE_ITW);
    }

    ocrAddDependence(depv[N-1].guid, stencilEdt[N-1], 0, DB_MODE_ITW);
    ocrAddDependence(NULL_GUID, stencilEdt[N-1], 2, DB_MODE_ITW);
    ocrAddDependence(depv[pbuffer++].guid, stencilEdt[N-1], 3, DB_MODE_ITW);

//printf("R  after add dep\n");
//fflush(stdout);


    return NULL_GUID;
}



ocrGuid_t mainEdt(){

/*
mainEdt is executed first
Creates the datablocks
creates realmain
launches realmian
*/

    u64 i;


//printf("M  start\n");
//fflush(stdout);
    PRINTF("1D stencil code: \nnumber of workers = %d \ndata on each worker = %d \nnumber of timesteps = %d \n", N, M, I);

    u64 *dummy;
    ocrGuid_t realmain, realmainOutputEvent, realmainTemplate, dataDb[N], privateDb[N], bufferDb[4*N-4];

    for(i=0;i<2*N-2;i++) {
       ocrDbCreate(&(bufferDb[i]), (void**) &dummy, sizeof(buffer_t), 0, NULL_GUID, NO_ALLOC);
   }


    for(i=0;i<N;i++) {
       ocrDbCreate(&(privateDb[i]), (void**) &dummy, sizeof(private_t), 0, NULL_GUID, NO_ALLOC);
       ocrDbCreate(&(dataDb[i]), (void**) &dummy, 2*M*sizeof(double), 0, NULL_GUID, NO_ALLOC);
   }

//printf("M  after createDB\n");
//fflush(stdout);

    ocrGuid_t wrapupTemplate;
    ocrGuid_t wrapupEdt;

    ocrEdtTemplateCreate(&wrapupTemplate, wrapupTask, 0, N+1);
    ocrEdtCreate(&wrapupEdt, wrapupTemplate, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL, EDT_PROP_NONE, NULL_GUID, NULL);
    ocrEdtTemplateCreate(&realmainTemplate, realmainTask, 0, 4*N-2);
    ocrEdtCreate(&realmain, realmainTemplate, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL,
          EDT_PROP_FINISH, NULL_GUID, &realmainOutputEvent);


//printf("M  after EDTcreate \n");
//fflush(stdout);


    for(i=0;i<N;i++)  ocrAddDependence(dataDb[i], wrapupEdt, i, DB_MODE_ITW);
        ocrAddDependence(realmainOutputEvent, wrapupEdt, N, DB_MODE_ITW);
//printf("M  after wrapup\n");
//fflush(stdout);

    for(i=0;i<N;i++) ocrAddDependence(dataDb[i], realmain, i, DB_MODE_ITW);
    for(i=0;i<2*N-2;i++) ocrAddDependence(bufferDb[i], realmain, N+i, DB_MODE_ITW);
    for(i=0;i<N;i++) ocrAddDependence(privateDb[i], realmain, (3*N-2)+i, DB_MODE_ITW);
//printf("M  after realmain\n");
//fflush(stdout);

    return NULL_GUID;
}
