#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"
#include "jbod.h"

//Uncomment the below code before implementing cache functioncs.
static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int cache_create(int num_entries) {
  // need to see if num_entries is out of allowed values
  if (num_entries > 4096 || num_entries < 2) {
    return -1;
  }
  // our cache is already created, cant create again
  if (cache != NULL) {
    return -1;
  }

  // heap memory for our cache, initializing with val 
  cache = calloc(num_entries, sizeof(cache_entry_t));
  // need to make sure its valid memory spot
  if (cache == NULL) {
    return -1;
  }
  // set our size
  cache_size = num_entries;
  return 1;
}

int cache_destroy(void) {
  // cache already destroyed
  if (!cache) {
    return -1;
  }
  
  // free the heap memory we made for our cache
  free(cache);
  // reset size and set it to Null
  cache_size = 0;
  cache = NULL;
  return 1;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  // we need to have a buff and cache, cant be null
  if (buf == NULL || cache == NULL) {
    return -1;
  }
  
  // we made a query
  num_queries++;
  for (int i = 0; i < cache_size; i++) {
    // cache has to match our disk and block numbers, it also needs to be valid 
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num && cache[i].valid) {
      // copy cache to buffer 
      memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);
      // cache goes up by 1
      num_hits++;
      // clock goes up by 1
      clock++;
      // now its the curr time so we set it 
      cache[i].clock_accesses = clock;
      return 1;
    }
  }
  // there is a cache miss 
  return -1;
} 

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  // we need to have a buff and cache, cant be null
  if (buf == NULL || cache == NULL) {
    return;
  }
  for (int i = 0; i < cache_size; i++) {
    // cache has to match our disk and block numbers, it also needs to be valid 
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num && cache[i].valid) {
      // cope buffer to cache to update
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
      // clock goes up by 1
      clock++;
      // now its the curr time so we set it
      cache[i].clock_accesses = clock;
      return;
    }
  }
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  // need to make sure buf and cache are there and our numbers for disks and blocks wont go out of the proper range
  if (cache == NULL || buf == NULL || block_num < 0 || disk_num > 15 || disk_num < 0 || block_num > 255) {
    return -1;
  }
  // need to iterate to see if there is a cache entry with disk and block num
  for (int i = 0; i < cache_size; i ++) {
    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num && cache[i].valid) {
      // we dont need to insert if its already there
      return -1;
    }
  }
  int idx = 0, i = 0;
  int most = -1;
  while (i < cache_size) {
    // we insert at the first invalid spot
    if (!cache[i].valid) {
      // save this spot as our index
      idx = i;
      break;
    }
    // we need to keep checking for the MOST recent access time so we keep updating and checking for a bigger most
    if (cache[i].clock_accesses > most) {
      most = cache[i].clock_accesses;
      // according to most recently used policy we want to select this spot
      idx = i;
    }
    i++;
  }

  // now our cache is going to be valid and has a specific block and disk num
  cache[idx].valid = true;
  cache[idx].block_num = block_num;
  cache[idx].disk_num = disk_num;
  // move the data from the buffer into the cache, cache[idx].block is the destination
  memcpy(cache[idx].block, buf, JBOD_BLOCK_SIZE);
  // clock increemented by one
  clock ++;
  // clock access time set
  cache[idx].clock_accesses = clock;
  return 1;
}

bool cache_enabled(void) {
  // we only return true if the cache is not null (which means its true)
  return (cache != NULL);
}

void cache_print_hit_rate(void) {
	fprintf(stderr, "num_hits: %d, num_queries: %d\n", num_hits, num_queries);
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}

int cache_resize(int new_num_entries) {
  // allocate new memory on the heap
  cache_entry_t *new = calloc(new_num_entries, sizeof(cache_entry_t));
  // it cant be a null address spot
  if (new == NULL) {
    return -1;
  }
  if (new_num_entries < cache_size) {
    int idx = 0, i = 0;
    int most = -1;
    // need to find number of entries to evict
    int to_evict = cache_size - new_num_entries;
    
    // evict the most recently used entries
    while (i < to_evict) {
      // we evict the most recently used entry (highest clock access)
      if (cache[i].valid && cache[i].clock_accesses > most) {
        most = cache[i].clock_accesses;
        // according to most recently used policy we want to select this spot
        idx = i;
      }
      i++;
      // mark this entry as invalid since because so we don't consider it again
      cache[idx].valid = false;
    }
    // now we copy the remaining valid entries to new cache
    memcpy(new, cache, new_num_entries * sizeof(cache_entry_t));
  } else {
    // copy all current entries to new cache
    memcpy(new, cache, cache_size * sizeof(cache_entry_t));
  }
  
  // now we can free the old caches memory
  free(cache);
  // set the cache size to the new ones
  cache_size = new_num_entries;
  // our cache is now the new cache
  cache = new;
  return 1;
}
