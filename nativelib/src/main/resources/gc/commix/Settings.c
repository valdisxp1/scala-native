#include "Settings.h"
#include "Constants.h"
#include "metadata/BlockMeta.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>

/*
 Accepts number of bytes or number with a suffix letter for indicating the
 units. k or K for kilobytes(1024 bytes), m or M for megabytes and g or G for
 gigabytes.
*/
size_t Settings_parseSizeStr(const char *str) {
    int length = strlen(str);
    size_t size;
    sscanf(str, "%zu", &size);
    char possibleSuffix = str[length - 1];
    switch (possibleSuffix) {
    case 'k':
    case 'K':
        if (size < (1ULL << (8 * sizeof(size_t) - 10))) {
            size <<= 10;
        } else {
            size = UNLIMITED_HEAP_SIZE;
        }
        break;
    case 'm':
    case 'M':
        if (size < (1ULL << (8 * sizeof(size_t) - 20))) {
            size <<= 20;
        } else {
            size = UNLIMITED_HEAP_SIZE;
        }
        break;
    case 'g':
    case 'G':
        if (size < (1ULL << (8 * sizeof(size_t) - 30))) {
            size <<= 30;
        } else {
            size = UNLIMITED_HEAP_SIZE;
        }
    }
    return size;
}

size_t Settings_MinHeapSize() {
    char *minHeapSizeStr = getenv("SCALANATIVE_MIN_HEAP_SIZE");
    if (minHeapSizeStr != NULL) {
        return Settings_parseSizeStr(minHeapSizeStr);
    } else {
        return DEFAULT_MIN_HEAP_SIZE;
    }
}

size_t Settings_MaxHeapSize() {
    char *maxHeapSizeStr = getenv("SCALANATIVE_MAX_HEAP_SIZE");
    if (maxHeapSizeStr != NULL) {
        return Settings_parseSizeStr(maxHeapSizeStr);
    } else {
        return UNLIMITED_HEAP_SIZE;
    }
}

#ifdef ENABLE_GC_STATS
char *Settings_StatsFileName() { return getenv(STATS_FILE_SETTING); }
#endif

int Settings_GCThreadCount() {
    char *str = getenv("SCALANATIVE_GC_THREADS");
    if (str == NULL) {
        // default is number of cores - 1, but no less than 1 and no more than 8
        int defaultGThreadCount = get_nprocs() - 1;
        if (defaultGThreadCount < 1) {
            defaultGThreadCount = 1;
        } else if (defaultGThreadCount > 8) {
            defaultGThreadCount = 8;
        }
        return defaultGThreadCount;
    } else {
        int count;
        sscanf(str, "%d", &count);
        if (count < 1) {
            count = 1;
        }
        return count;
    }
}