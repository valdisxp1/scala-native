#ifndef IMMIX_CONSTANTS_H
#define IMMIX_CONSTANTS_H

#define WORD_SIZE_BITS 3
#define WORD_SIZE (1 << WORD_SIZE_BITS)

#define WORD_INVERSE_MASK (~((word_t)WORD_SIZE - 1))

#define BLOCK_SIZE_BITS 15
#define LINE_SIZE_BITS 8

#define BLOCK_METADATA_SIZE_BITS 3
#define LINE_METADATA_SIZE_BITS 0

#define BLOCK_TOTAL_SIZE (1 << BLOCK_SIZE_BITS)
#define BLOCK_METADATA_SIZE (1 << BLOCK_METADATA_SIZE_BITS)
#define LINE_SIZE (1UL << LINE_SIZE_BITS)
#define LINE_METADATA_SIZE (1 << LINE_METADATA_SIZE_BITS)

#define LINE_SIZE_MASK (LINE_SIZE - 1)

#define OBJECT_HEADER_SIZE 8
#define WORDS_IN_OBJECT_HEADER (OBJECT_HEADER_SIZE / WORD_SIZE)

#define LINE_COUNT (BLOCK_TOTAL_SIZE / LINE_SIZE)

#define BLOCK_METADATA_ALIGNED_SIZE                                            \
    ((BLOCK_METADATA_SIZE + LINE_SIZE - 1) / LINE_SIZE * LINE_SIZE)

#define WORDS_IN_LINE (LINE_SIZE / WORD_SIZE)
#define WORDS_IN_BLOCK (BLOCK_TOTAL_SIZE / WORD_SIZE)
#define WORDS_IN_BLOCK_METADATA (BLOCK_METADATA_SIZE / WORD_SIZE)

#define BLOCK_SIZE_IN_BYTES_MASK (BLOCK_TOTAL_SIZE - 1)
#define BLOCK_SIZE_IN_BYTES_INVERSE_MASK (~BLOCK_SIZE_IN_BYTES_MASK)

#define LARGE_BLOCK_SIZE_BITS 13
#define LARGE_BLOCK_SIZE (1 << LARGE_BLOCK_SIZE_BITS)

#define LARGE_OBJECT_MIN_SIZE_BITS 13
#define LARGE_OBJECT_MAX_SIZE_BITS 34

#define MIN_BLOCK_SIZE (1UL << LARGE_OBJECT_MIN_SIZE_BITS)
#define MAX_BLOCK_SIZE (1UL << LARGE_OBJECT_MAX_SIZE_BITS)

#define LARGE_BLOCK_MASK (~((1 << LARGE_OBJECT_MIN_SIZE_BITS) - 1))

#define EARLY_GROWTH_THRESHOLD (128 * 1024 * 1024UL)
#define EARLY_GROWTH_RATE 2.0
#define GROWTH_RATE 1.414213562

#define INITIAL_SMALL_HEAP_SIZE (4 * 1024 * 1024UL)
#define INITIAL_LARGE_HEAP_SIZE (1024 * 1024UL)

#define GC_STATS_MEASUREMENTS 10

#endif // IMMIX_CONSTANTS_H
