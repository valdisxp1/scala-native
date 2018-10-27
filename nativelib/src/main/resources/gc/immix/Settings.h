#ifndef IMMIX_SETTINGS_H
#define IMMIX_SETTINGS_H

#include <stddef.h>
#define STATS_FILE_SETTING "SCALANATIVE_STATS_FILE"

size_t Settings_MinHeapSize();
size_t Settings_MaxHeapSize();

char *Settings_StatsFileName();

#endif // IMMIX_SETTINGS_H