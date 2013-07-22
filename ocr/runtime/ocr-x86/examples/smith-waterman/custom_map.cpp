#include "ocr-types.h"

#include <pthread.h>
#include <hash_map.h>
#include <assert.h>

extern "C" void initMap();
extern "C" void * getGuid(ocrGuid_t guid);
extern "C" void setGuid(ocrGuid_t guid, void * data);

static pthread_mutex_t setLock;

// equal_to and hash copied from http://codingrelic.geekhold.com/2010/04/code-snippet-hashmap.html
namespace std {
template<> struct equal_to<u64> {
  bool operator()(const u64& x, const u64& y) const {
    return (x == y);
  }
};
}  // namespace std

namespace __gnu_cxx {
template<> struct hash<u64> {
  size_t operator()(const u64& k) const {
    return (size_t) k;
  }
};
}  // namespace __gnu_cxx

static hash_map<ocrGuid_t, void *> guidStore;

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
