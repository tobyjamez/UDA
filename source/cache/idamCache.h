#ifndef IDAM_CACHE_IDAMCACHE_H
#define IDAM_CACHE_IDAMCACHE_H

#include <time.h>

#include <clientserver/idamStructs.h>

#define CACHE_NOT_OPENED    1
#define CACHE_NOT_AVAILABLE 2
#define CACHE_AVAILABLE     3

#define IDAM_CACHE_HOST     "localhost"     // Override these with environment variables with the same name
#define IDAM_CACHE_PORT     11211
#define IDAM_CACHE_EXPIRY   86400           //24*3600       // Lifetime of the object in Secs

typedef struct IdamCache IDAM_CACHE;

IDAM_CACHE* idamOpenCache();

void idamFreeCache();

char* idamCacheKey(REQUEST_BLOCK* request_block);

int idamCacheWrite(IDAM_CACHE* cache, REQUEST_BLOCK* request_block, DATA_BLOCK* data_block);

DATA_BLOCK* idamCacheRead(IDAM_CACHE* cache, REQUEST_BLOCK* request_block);

#endif // IDAM_CACHE_IDAMCACHE_H
