#include "ocr-types.h"

#include <pthread.h>
#include <assert.h>
#include <unordered_map>

using namespace std;

extern "C" void initMap();
extern "C" void * getGuid(ocrGuid_t guid);
extern "C" void setGuid(ocrGuid_t guid, void * data);

static pthread_mutex_t setLock;

static unordered_map<ocrGuid_t, void *> guidStore;

void initMap() {
	int res = pthread_mutex_init(&setLock, NULL);
	assert(res == 0);
}

void * getGuid(ocrGuid_t guid) {
	// Not sure if it's safe to read when the map is being modified
	pthread_mutex_lock(&setLock);
	void * res = guidStore[guid];
	pthread_mutex_unlock(&setLock);
	return res;
}

void setGuid(ocrGuid_t guid, void * data) {
	pthread_mutex_lock(&setLock);
    guidStore[guid] = data;
    pthread_mutex_unlock(&setLock);
}
