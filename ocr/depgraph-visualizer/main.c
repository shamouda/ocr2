/*
 * main.c
 *
 *  Created on: Apr 26, 2014
 *      Author: rindalp
 */
#include <stdio.h>
#include "Ubigraph/include/UbigraphAPI.h"
#include <stdlib.h>

struct edt{
	int id;
	int tsCreated, tsRunnable, tsStarted, tsCompleted;

	int depCnt;
	struct edt **deps;
};

struct exeGraph{
	int edtCount;
	struct edt *edts;

};

struct exeGraph * mockExeGraph(){
	struct exeGraph *eg;
	eg = (struct exeGraph*)malloc(sizeof(struct exeGraph));
	eg->edtCount=10;
	eg->edts = (struct edt*)malloc(sizeof(struct edt)*eg->edtCount);

	eg->edts[0].depCnt = 0;
	eg->edts[0].id = 0;
	eg->edts[0].tsCreated = 0;
	eg->edts[0].tsRunnable = 0;
	eg->edts[0].tsStarted = 0;
	eg->edts[0].tsCompleted = 1;

	eg->edts[1].depCnt = 1;
	eg->edts[1].deps[0] = & eg->edts[0];
	eg->edts[1].id = 1;
	eg->edts[1].tsCreated = 0;
	eg->edts[1].tsRunnable = 1;
	eg->edts[1].tsStarted = 1;
	eg->edts[1].tsCompleted = 2;
	int i ,j;
	for( i =2; i < eg->edtCount;i++ ){
		eg->edts[i].id = i;

		eg->edts[i].depCnt = 2;
		eg->edts[i].deps = (struct edt **)malloc(sizeof(struct edt *)* eg->edts[i].depCnt );

		eg->edts[i].deps[0] = &eg->edts[i-2];
		eg->edts[i].deps[1] = &eg->edts[i-1];

		eg->edts[i].tsCreated = eg->edts[i-2].tsStarted +1;
		eg->edts[i].tsRunnable = eg->edts[i-1].tsCompleted;
		eg->edts[i].tsStarted = eg->edts[i-1].tsCompleted;
		eg->edts[i].tsCompleted = eg->edts[i].tsStarted + 1;
	}
	return eg;
}
struct exeGraph *parseStdin(){
	struct exeGraph *eg;

	eg = mockExeGraph();
}

int main(int argc, char *argv[]){

	struct exeGraph *eg = parseStdin();

	int curTime = 0;
	int i, j;
	for(i =0; i < eg->edtCount ;i++){

		if(eg->edts[i].tsStarted > curTime){
			sleep(eg->edts[i].tsStarted - curTime);
			curTime = eg->edts[i].tsStarted;
		}

		ubigraph_new_vertex_w_id(eg->edts[i].id);

		for( j =0; i < eg->edts[i].depCnt;j++){
			ubigraph_new_edge(eg->edts[i].id, eg->edts[i].deps[j]->id);
		}
	}
}






