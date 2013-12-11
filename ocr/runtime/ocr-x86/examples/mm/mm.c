/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifdef POWERTRACING

#define HACK_TO_REMOVE_DUPLICATE_ERROR
#include "cpucounters.h"
#ifdef _MSC_VER
#pragma warning(disable : 4996) // for sprintf
#include <windows.h>
#include "../PCM_Win/windriver.h"
#else
#include <unistd.h>
#include <signal.h>
#endif
#include <iostream>
#include <stdlib.h>
#include <iomanip>
#ifdef _MSC_VER
#include "freegetopt/getopt.h"
#endif
#include "utils.h"

#endif

#include <stdio.h>
#include <stdlib.h>
#include "ocr.h"
#include <sys/time.h>

#define DEFAULT_N 4
#define DEFAULT_M 4
#define DEFAULT_P 4
#define DEFAULT_TILE 4

void Print_Matrix (double *v, int M, int N);
void Transpose (double *v, int M, int N);

long SEED = 1;
struct timeval initTime, endTime;

#ifdef POWERTRACING
//SystemCounterState before_sstate[32], after_sstate[32];
JKTUncorePowerState before_sstate[32], after_sstate[32];

 PCM * m = PCM::getInstance();
int default_freq_band[3] = {12,20,40};
#endif

inline double LCGS(void)
{
        long x, y, z;
        long Alpha = 1664525,
        Beta = 1013904223,
        Sigma = 10000 * 10000 * 2;
        x = Alpha * SEED + Beta;
        z = x / Sigma;
        y = x - z * Sigma;
        SEED = y;
        return (double) y / (double) Sigma;
}

ocrGuid_t ComputeTile(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]){
	//u64 *func_args = paramv;
	int M = paramv[0];
	int Tile = paramv[1];
	ocrGuid_t* FinalCEvt = (ocrGuid_t*) paramv[2];

	int i,j,k;
	int offset;
	
	double* ABlock = (double*) (depv[0].ptr);
	double* BBlock = (double*) (depv[1].ptr);
	double* CBlock = (double*) (depv[2].ptr);
	//printf("Tile\n");
	
	//Print_Matrix (ABlock, M, Tile);
	//Print_Matrix (BBlock, M, Tile);
	for (i=0; i<M; i++){
		offset = i*Tile;
		for (j=0; j<Tile; j++){
			for (k=0; k<Tile; k++){
				CBlock[j*Tile+k]+=ABlock[offset + j]*BBlock[offset + k];
			}
		}
	}
	
	//Print_Matrix (CBlock, Tile, Tile);
	
	//printf("before Ablock[%d]=%f \n",i,*ABlock);
	//*ABlock = *ABlock*2;
	//printf("after Ablock[%d]=%f \n",i,*ABlock);
	//printf("Tile %d with address %x \n",i, ABlock);
	ocrEventSatisfy(FinalCEvt, depv[2].guid);
	return NULL_GUID;
}

ocrGuid_t JoinTask(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]){
	//u64 *func_args = paramv;
	double* C = paramv[0];
	int Tile = paramv[1];
	int numTiles = paramv[2];
	int M = paramv[3];
	int i,j,k,l;
	double* temp;
	int N = numTiles*Tile;
	int offset;
	//printf("Final Tasks\n");
	for (i=0; i<numTiles; i++){
		for (j=0; j<numTiles; j++){
			temp = (double*) (depv[i*numTiles+j].ptr);
			offset = i*N*Tile + j*Tile;
			for (k=0; k<Tile; k++){
				for (l=0; l<Tile; l++){
					C[offset + k*N + l] = temp[k*Tile + l];
				}
			}
		}
		//printf("Address of Tiles is %x ", temp);
		//A[i] = *temp;
		//printf("A[%d]=%f=%f \n",i,A[i],*temp);
	}
	#ifdef COMPARE
	double tempsum = 0.0;
	for (i=0; i<N*N; i++){
		tempsum+=C[i];
	}
	//Print_Matrix (C, N, N);
	printf("Parallel Checksum = %f\n",tempsum);
	#endif
	ocrShutdown();

	#ifdef MEASURE
	gettimeofday(&endTime, 0);
	
	//SystemCounterState after_sstate = getSystemCounterState();
#ifdef POWERTRACING	
	double TotalJoules=0;
	double TotalJoulesDRAM=0;
	for(uint32 socket=0;socket<m->getNumSockets();++socket){
		after_sstate[socket] = m->getJKTUncorePowerState(socket);
		TotalJoules+= getConsumedJoules(before_sstate[socket],after_sstate[socket]);
		TotalJoulesDRAM+= getDRAMConsumedJoules(before_sstate[socket],after_sstate[socket]);
		printf("Consumed Joules in Socket %d: %f\n",socket, getConsumedJoules(before_sstate[socket],after_sstate[socket]));
		printf("Consumed DRAM Joules in Socket %d: %f\n",socket, getDRAMConsumedJoules(before_sstate[socket],after_sstate[socket]));
	}

	printf("Total Consumed Joules: %f\n",socket, TotalJoules);
	printf("Total Consumed DRAM Joules: %f\n",socket, TotalJoulesDRAM);

#endif	
	printf("Elapsed Time: %f ms.\t", ((endTime.tv_sec - initTime.tv_sec)*1000000+(endTime.tv_usec - initTime.tv_usec))*1.0/1000);
  	printf("# Mega Floating Point Operations: %.3f\t", ((2.0*M-1)*N*N)/1000000);
  	printf("MFLOPS: %.3f\n",((2.0*M-1)*N*N)/(((endTime.tv_sec - initTime.tv_sec)*1000000+(endTime.tv_usec - initTime.tv_usec))*1.0));
	#endif

	return NULL_GUID;
}

#ifdef __cplusplus
extern "C" {
#endif
ocrGuid_t mainEdt ( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    
	
	int M, N, P, Tile;
	double *va, *vb, *vc;
	double stotal=0.0; /*sequential computation*/
	double ptotal=0.0; /*parallel computation*/
	M = DEFAULT_M;
	N = DEFAULT_N;
	P = DEFAULT_P;
	Tile = DEFAULT_TILE; 
	u64 argc;

	int i,j,k,l;
	double temp;
	void *programArg = depv[0].ptr;
	u64* dbAsU64 = (u64*)programArg;
	argc = dbAsU64[0];

	u64* offsets = (u64*) malloc (argc*sizeof(u64));
	for (i=0; i<argc; i++){
		offsets[i] = dbAsU64[i+1];
	}
	char *dbAsChar = (char*)programArg;

    	/* Checking for Input */
    	if (argc != 5) {
		printf("WARNING: Using default parameters. Usage mm <#Processors> <Matrix Size M> <Matrix Size N> <Tile Size>\n");
	}
	else {
		P = atoi(dbAsChar+offsets[1]);
		M = atoi(dbAsChar+offsets[2]);
		N = atoi(dbAsChar+offsets[3]);
		Tile = atoi(dbAsChar+offsets[4]);
	}

	if (N%Tile != 0){
		printf("ERROR: Matrix Size %d is not multiple of Tile size %d\n", N, Tile);
		ocrShutdown();
		return -1;
	}

  	int numTiles = N/Tile;
 
        printf("\nStarting the Matrix Multiplication  benchmark with the following parameters:\n");
        printf("\tNumber of processors   : %lld\n", P);
        printf("\tSize of Matrix A       : %lld x %lld\n", N, M);
        printf("\tSize of Matrix B       : %lld x %lld\n", M, N);
        printf("\tSize of Matrix C       : %lld x %lld\n", N, N);
        printf("\tSize of tiles          : %lld x %lld\n", Tile, Tile);
        printf("\tNumber of tiles        : %lld \n", numTiles*numTiles);
 
        va = (double *)malloc(N*M*sizeof(double));
        vb = (double *)malloc(N*M*sizeof(double));
        vc = (double *)malloc(N*N*sizeof(double));
        if(va == NULL || vb == NULL || vc == NULL){
                printf("Error: Out of Memory allocating matrices.\n");
                ocrShutdown();
                return -1;
        }

  	printf ("Initializing Matrices ...\n");
  	/* Disable for saving time in simulation. does not change the measure */
	#ifdef NOSIMPLIFY	
  	for(i=0; i<N*M; i++) {
    		va[i] = LCGS();
    		vb[i] = LCGS();
  	}
  	for(i=0; i<N*N; i++) {
    		vc[i] = 0;
  	}
	#endif
	
	#ifdef COMPARE
	double tempsum=0.0;
  	printf ("Beginning  serial processing ...\n ");
        for (i=0; i<N; i++){
                for (j=0 ; j<N; j++){
                        temp=0;
                        for (k=0; k<M; k++)
                                temp += va[i*M+k]*vb[k*N+j];
                        vc[i*N+j]=temp;
                }
        }
 	for (i=0; i<N*N; i++){
		tempsum+=vc[i];
	}
	printf ("Serial checksum = %f\n",tempsum);	
        //Print_Matrix(vc,N,N);
        printf("Initializing  matrix C again ...\n");
        for (i=0; i<N*N; i++) {
                stotal += vc[i];
                vc[i] = 0;
        }
	#endif

 	/* Disable for saving time in simulation. does not change the measure */
        #ifdef NOSIMPLIFY
	//Print_Matrix (va, N, M);
        Transpose(va, N, M);
	//Print_Matrix (va, M, N);
        #endif

	ocrGuid_t *ABlockEvt;
	ABlockEvt = (ocrGuid_t *) malloc (numTiles*sizeof(ocrGuid_t));
	for (i=0; i<numTiles; i++){
		ocrEventCreate(/*GUID returned value*/ &(ABlockEvt[i]),/*Event Type*/ OCR_EVENT_STICKY_T,/*Carry Data?*/ true);
	}
	ocrGuid_t *BBlockEvt;
	BBlockEvt = (ocrGuid_t *) malloc (numTiles*sizeof(ocrGuid_t));
	for (i=0; i<numTiles; i++){
		ocrEventCreate(/*GUID returned value*/ &(BBlockEvt[i]),/*Event Type*/ OCR_EVENT_STICKY_T,/*Carry Data?*/ true);
	}
	ocrGuid_t *CBlockEvt;
	CBlockEvt = (ocrGuid_t *) malloc (numTiles*numTiles*sizeof(ocrGuid_t));
	for (i=0; i<numTiles*numTiles; i++){
		ocrEventCreate(/*GUID returned value*/ &(CBlockEvt[i]),/*Event Type*/ OCR_EVENT_STICKY_T,/*Carry Data?*/ true);
	}
	ocrGuid_t *FinalBlockEvt;
	FinalBlockEvt = (ocrGuid_t *) malloc (numTiles*numTiles*sizeof(ocrGuid_t));
	for (i=0; i<numTiles*numTiles; i++){
		ocrEventCreate(/*GUID returned value*/ &(FinalBlockEvt[i]),/*Event Type*/ OCR_EVENT_STICKY_T,/*Carry Data?*/ true);
	}
#ifdef POWERTRACING
 	//PCM * m = PCM::getInstance();
	// program counters, and on a failure just exit
	//if (m->program() != PCM::Success){
	if(PCM::Success != m->programSNB_EP_PowerMetrics(0,0,default_freq_band)){
		printf("Error: Not able to read power counters or architecture not supported");
		ocrShutdown();
		return -1;
	}
	
	
	for(uint32 socket=0;socket<m->getNumSockets();++socket){
		//after_sstate[socket] = getSystemCounterState(socket);
		//before_sstate = getSystemCounterState(socket);
		before_sstate[socket] = m->getJKTUncorePowerState(socket);
	//cout << "Consumed Joules:" << getConsumedJoules(before_sstate,af    ter_sstate) << endl;
		//printf("Consumed Joules: %f\n",getConsumedJoules(before_sstate[socket],after_sstate[socket]));
	}
#endif

	gettimeofday(&initTime,0);	
	//printf("creating datablocks\n");
	double* DataBlock;
	int offset;
	for (i=0; i<numTiles ; i++){
		ocrGuid_t ADbGuid;
		void* ADbGuidData;
		u8 flag;
		flag = ocrDbCreate(/*GUID new DB*/ &ADbGuid,/*Address*/ &ADbGuidData,/*block size*/ M*Tile*sizeof(double),/*flags*/ 0,/*location*/ NULL_GUID,/*allocator*/ NO_ALLOC);
	#ifdef DEBUG
		printf("Creating data block  A[%d] with flag %d\n",i,flag);
		if (flag != (u8)0){
			printf("ERROR: Out of memory allocating DataBlock A[%d]\n",i);
			DataBlock = (double*)malloc(M*Tile*sizeof(double));
			if (DataBlock == NULL){
				printf("Fail creation with malloc\n");
			}
			else{
				printf("Success creation with malloc\n");
				offset = i*Tile;
				for (j=0; j<M; j++){
					for (k=0; k<Tile; k++){
						DataBlock[j*Tile + k] = va[offset + j*N + k];
					}
				}
				printf("Success copy\n");
			}
			ocrShutdown();
			return -1;
		}
	#endif
		DataBlock = (double*)ADbGuidData;
		offset = i*Tile;
		for (j=0; j<M; j++){
			for (k=0; k<Tile; k++){
				DataBlock[j*Tile + k] = va[offset + j*N + k];
			}
		}
		ocrDbRelease(ADbGuid);
		ocrEventSatisfy(ABlockEvt[i], ADbGuid);
	}
	for (i=0; i<numTiles ; i++){
		ocrGuid_t BDbGuid;
		void* BDbGuidData;
		u8 flag;
		flag = ocrDbCreate(/*GUID new DB*/ &BDbGuid,/*Address*/ &BDbGuidData,/*block size*/ M*Tile*sizeof(double),/*flags*/ 0,/*location*/ NULL_GUID,/*allocator*/ NO_ALLOC);
	#ifdef DEBUG
		printf("Creating data block  B[%d] with flag %d\n",i,flag);
		if (flag != (u8)0){
			printf("ERROR: Out of memory allocating DataBlock B[%d]\n",i);
			ocrShutdown();
			return -1;
		}
	#endif
		DataBlock = (double*)BDbGuidData;
		offset = i*Tile;
		for (j=0; j<M; j++){
			for (k=0; k<Tile; k++){
				DataBlock[j*Tile + k] = vb[offset + j*N + k];
			}
		}
		ocrDbRelease(BDbGuid);
		ocrEventSatisfy(BBlockEvt[i], BDbGuid);
	}
	for (i=0; i<numTiles ; i++){
		for (j=0; j<numTiles; j++){
			ocrGuid_t CDbGuid;
			void* CDbGuidData;
			u8 flag;
			flag = ocrDbCreate(/*GUID new DB*/ &CDbGuid,/*Address*/ &CDbGuidData,/*block size*/ Tile*Tile*sizeof(double),/*flags*/ 0,/*location*/ NULL_GUID,/*allocator*/ NO_ALLOC);
	#ifdef DEBUG
			printf("Creating data block  C[%d,%d] with flag %d\n",i,j,flag);
			if (flag != (u8)0){
				printf("ERROR: Out of memory allocating DataBlock C[%d,%d]\n",i,j);
				ocrShutdown();
				return -1;
			}
	#endif
			DataBlock = (double*)CDbGuidData;
			offset = i*Tile*N + j*Tile;
			for (k=0; k<Tile; k++){
				for (l=0; l<Tile; l++){
					DataBlock[k*Tile + l] = vc[offset + k*N + l];
				}
			}
			ocrDbRelease(CDbGuid);
			ocrEventSatisfy(CBlockEvt[i*numTiles+j], CDbGuid);
		}
	}

	ocrGuid_t TileEdtTemplateGuid;
	ocrEdtTemplateCreate(/*GUID returned value*/ &TileEdtTemplateGuid,/*function of EDT*/ ComputeTile,/*Num No-DB Dependencies*/ 3,/*Num DB Dependencies*/ 3);
	for (i=0; i<numTiles; i++){
		for (j=0; j<numTiles; j++){
			u64 *func_args = (u64*) malloc(3*sizeof(u64));
			func_args[0] = M;
			func_args[1] = Tile;
			func_args[2] = (ocrGuid_t*) FinalBlockEvt[i*numTiles + j];	
			ocrGuid_t TileEdt;
			ocrEdtCreate(/*GUID returned value*/ &TileEdt,/*GUID of the template*/ TileEdtTemplateGuid,/*Num No-DB Dependencies*/ EDT_PARAM_DEF,
			/*Values No-DB Dependencies*/ func_args,/*Num DB Dependencies*/ EDT_PARAM_DEF,/*Values DB Dependencies*/ NULL,
			/*Properties*/ 0,/*Affinity*/ NULL_GUID,/*Output Event*/ NULL );
			//ocrAddDependence(ABlockEvt[i], TileEdt, 0, DB_MODE_RO);
			ocrAddDependence(ABlockEvt[i], TileEdt, 0, DB_MODE_ITW);
			ocrAddDependence(BBlockEvt[j], TileEdt, 1, DB_MODE_ITW);
			ocrAddDependence(CBlockEvt[i*numTiles+j], TileEdt, 2, DB_MODE_ITW);
		}
	}
	ocrGuid_t FinalEdtTemplateGuid;
	ocrGuid_t FinalEdt;
	ocrEdtTemplateCreate(/*GUID returned value*/ &FinalEdtTemplateGuid,/*function of EDT*/ JoinTask,/*Num No-DB Dependencies*/ 4,/*Num DB Dependencies*/ numTiles*numTiles);
	u64 *func_args = (u64*) malloc(4*sizeof(u64));
	func_args[0] = vc;
	func_args[1] = Tile;
	func_args[2] = numTiles;
	func_args[3] = M;

	ocrEdtCreate(/*GUID returned value*/ &FinalEdt,/*GUID of the template*/ FinalEdtTemplateGuid,/*Num No-DB Dependencies*/ EDT_PARAM_DEF,
		/*Values No-DB Dependencies*/ func_args,/*Num DB Dependencies*/ EDT_PARAM_DEF,/*Values DB Dependencies*/ NULL,/*Properties*/ 0,
		/*Affinity*/ NULL_GUID,/*Output Event*/ NULL );
	for (i=0; i<numTiles*numTiles; i++){
		ocrAddDependence(FinalBlockEvt[i], FinalEdt, i, DB_MODE_ITW);
	}

	//printf("End of Main !\n");

    	return 0;
}
#ifdef __cplusplus
}
#endif

void Print_Matrix (double *v, int M, int N)
{
 
  	int i,j;
  	for (i=0;i<M;i++){
    		for (j=0;j<N;j++)
      			printf("%.2f,",v[i*N+j]);
    		printf("\n");
  	}	
  	return;
}

void Transpose (double *v, int  M, int N)
{
  	double *v2;
  	int i,j;
  	v2 = (double *)malloc(M*N*sizeof(double));
  	for (i=0; i<M; i++)
    		for (j=0; j<N; j++)
      			v2[j*M+i]=v[i*N+j];
  	for (i=0;i<M*N;i++)
    		v[i]=v2[i];
  	free(v2);
  	return;
}
             
