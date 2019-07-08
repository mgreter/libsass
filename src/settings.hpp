#ifndef SASS_SETTINGS_H
#define SASS_SETTINGS_H

// Global compile time settings should go here
// These settings are meant to be customized

// Default precision to format floats
#define SassDefaultPrecision 10

// When enabled we use our custom memory pool allocator
// With intense workloads this can double the performance
// Max memory usage mostly only grows by a slight amount
#define SASS_CUSTOM_ALLOCATOR

//-------------------------------------------------------------------
// Below settings should only be changed if you know what you do!
//-------------------------------------------------------------------

// Detail settings for pool allocator
#ifdef SASS_CUSTOM_ALLOCATOR

  // How many buckets should we have for the free-list
  // Determines when allocations go directly to malloc/free
  // For maximum size of managed items multiply by alignment
  #define SassAllocatorBuckets 512

  // The size of the memory pool arenas in bytes.
  #define SassAllocatorArenaSize (1024 * 256)

#endif
// EO SASS_CUSTOM_ALLOCATOR

#endif
