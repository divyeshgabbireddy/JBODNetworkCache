#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache.h"
#include "jbod.h"
#include "mdadm.h"
#include "net.h"

/*Use your mdadm code*/

static int mounted = 0;
static int write = 0;

int mdadm_mount(void) {
  // Complete your code here
  // won't mount if already mounted
  if (mounted) {
    return -1;
  }
  // need to place mount command in bits 12 - 19, so we shift left by 12
  uint32_t op = JBOD_MOUNT << 12;
  jbod_client_operation(op, NULL);
  mounted = 1;
  return 1;
}

int mdadm_unmount(void) {
  // Complete your code here
  // won't unmount if already unmounted
  if (!mounted) {
    return -1;
  }
  // need to place mount command in bits 12 - 19, so we shift left by 12
  uint32_t op = JBOD_UNMOUNT << 12;
  jbod_client_operation(op, NULL);
  mounted = 0;
  return 1;
}

int mdadm_write_permission(void){

	// YOUR CODE GOES HERE
	// won't give permission if we already have permission
	if (write) { 
		return -1;
	}
	// shift by 12 to place command in proper position
	uint32_t op = JBOD_WRITE_PERMISSION << 12;
	jbod_client_operation(op, NULL);
	write = 1;
	return 1;
}


int mdadm_revoke_write_permission(void){
	// YOUR CODE GOES HERE
	// can't revoke permission if its already revoked
	if (!write) {
		return -1;
	}
	// shift by 12 to place command in proper position
	uint32_t op = JBOD_REVOKE_WRITE_PERMISSION << 12;
	jbod_client_operation(op, NULL);
	write = 0;
	return 1;
}

int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf) {
  // system is not mounted
  if (mounted == 0) {
    return -3;
  }
   // read_len cannot exceed 1024
  if (read_len > 1024) {
    return -2;
  }
  // reading beyond valid address space, bytes range from 0 to 1,048,575 (inclusive)
  if (start_addr + read_len > JBOD_NUM_DISKS * JBOD_DISK_SIZE) {
    return -1;
  }
  // 0-length read should succeed
  if (read_len == 0) {
    return 0;
  }
  // check if buf is null
  if (read_buf == NULL) {
    return -4;
  }
    
  uint32_t curr = start_addr;
  uint8_t buffer[256];
  // keep looping while we havent hit end address
  while (curr < (read_len + start_addr)) {
    // calculate which new disk we are at with integer division
    uint32_t disk_num = curr / JBOD_DISK_SIZE;
    // calculate which block we are at by getting position in disk, then integer division by 256 to see which block we are in
    uint32_t block_num = (curr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    // shows us which byte we are in within the current block
    uint32_t block_offset = curr % 256;
    uint8_t *dest_addr = (read_buf - start_addr) + curr;
    // how many bytes that need to be read to finish our read operation
    uint32_t bytes_to_end = (read_len + start_addr) - curr;
    // seek to disk op, shift accordingly
    uint32_t disk_seek_op = (JBOD_SEEK_TO_DISK << 12) | disk_num;
    // seek to block op, shift accordingly
    uint32_t block_seek_op = (JBOD_SEEK_TO_BLOCK << 12) | (block_num << 4);
    // now we define read op into block_buffer
    uint32_t read_op = JBOD_READ_BLOCK << 12;
    // we need to track num of bytes that we copy
    uint32_t bytes_to_copy;
    // we need to see if the amount of data we still need to read based on read_len and start_addr as well as where we are is < 256
    if ((read_len + start_addr) - curr < 256) {
      // we need to copy all the remaining bytes 
      bytes_to_copy = bytes_to_end;
    } else {
      // we dont want to copy beyond the boundary
      bytes_to_copy = 256 - block_offset;
    }

    // we first need to check if cache is not Null 
    if (cache_enabled()) {
      // see if the block at the disk is in the cache
      if (cache_lookup(disk_num, block_num, buffer) == 1) {
        // copy from the cache to the right dest with proper num of bytes
        memcpy(dest_addr, buffer + block_offset, bytes_to_copy);
        // we need to make sure that we work with the next full block
        curr = (curr - (curr % 256)) + 256;
        continue;
      }
    }

    // seek to disk
    jbod_client_operation(disk_seek_op, NULL);
    // seek to block
    jbod_client_operation(block_seek_op, NULL);
    // read into buffer
    jbod_client_operation(read_op, buffer);
    // cache should be not null
    if (cache_enabled()) {
      // if the same data is needed for later, we can get it from the cache so we insert
      cache_insert(disk_num, block_num, buffer);
    }
    // copy from the cache to the right dest with proper num of bytes
    memcpy(dest_addr, buffer + block_offset, bytes_to_copy);
    // we need to make sure that we work with the next full block
    curr = (curr - (curr % 256)) + 256;
  }
  return read_len;
}

int mdadm_write(uint32_t start_addr, uint32_t write_len, const uint8_t *write_buf) {
  // system is not mounted
  if (!mounted) {
    return -3;
  }
  // write permission not enabled
  if (!write) {
    return -5;
  }
  // if we writing beyond valid address space, bytes range from 0 to 1,048,575 (inclusive)
  if (start_addr + write_len > JBOD_NUM_DISKS * JBOD_DISK_SIZE) {
    return -1;
  }
  // write_len cannot exceed 1024
  if (write_len > 1024) {
    return -2;
  }
  // 0-length write should succeed
  if (write_len == 0) {
    return 0;
  }
  // check if buf is null
  if (write_buf == NULL) {
    return -4;
  }

  uint32_t curr = start_addr;
  uint8_t buffer[256];
  // keep track of where we are in the write buffer
  uint32_t buf_offset = 0;
  
  // keep looping while we haven't hit end address
  while (curr < (write_len + start_addr)) {
    // calculate which new disk we are at with integer division
    uint32_t disk_num = curr / JBOD_DISK_SIZE;
    // calculate which block we are at by getting position in disk, then integer division by 256 to see which block we are in
    uint32_t block_num = (curr % JBOD_DISK_SIZE) / JBOD_BLOCK_SIZE;
    // shows us which byte we are in within the current block
    uint32_t block_offset = curr % 256;
    // how many bytes that need to be written to finish our write operation
    uint32_t bytes_to_end = (write_len + start_addr) - curr;
    // seek to disk op, shift accordingly
    uint32_t disk_seek_op = (JBOD_SEEK_TO_DISK << 12) | disk_num;
    // seek to block op, shift accordingly
    uint32_t block_seek_op = (JBOD_SEEK_TO_BLOCK << 12) | (block_num << 4);
    // define read and write ops, shift accordingly
    uint32_t read_op = JBOD_READ_BLOCK << 12;
    uint32_t write_op = JBOD_WRITE_BLOCK << 12;
    // we need to track num of bytes that we copy
    uint32_t bytes_to_copy;
    // we need to see if the amount of data we still need to write based on write_len and start_addr as well as where we are is < 256
    if (bytes_to_end < (256 - block_offset)) {
      // we need to copy all the remaining bytes
      bytes_to_copy = bytes_to_end;
    } else {
      // we don't want to copy beyond the boundary
      bytes_to_copy = 256 - block_offset;
    }

    // we first need to check if cache is not Null (has to be on)
    if (cache_enabled()) {
      // see if the block at the disk is in the cache
      if (cache_lookup(disk_num, block_num, buffer) == 1) {
        // copy from write buffer to buffer at the right offset
        memcpy(buffer + block_offset, write_buf + buf_offset, bytes_to_copy);
        // update the cache with modified data
        cache_update(disk_num, block_num, buffer);
        // write the modified block back to disk
        jbod_client_operation(disk_seek_op, NULL);
        jbod_client_operation(block_seek_op, NULL);
        jbod_client_operation(write_op, buffer);
        // if we've written all requested bytes, then we are all completed
        if (bytes_to_copy == bytes_to_end) {
          return write_len;
        }
        // we need to make sure that we work with the next full block
        curr = (curr - block_offset) + 256;
        continue;
      }
    }
    // now read existing block into our buffer
    jbod_client_operation(disk_seek_op, NULL);
    jbod_client_operation(block_seek_op, NULL);
    jbod_client_operation(read_op, buffer);
    // now we modify our block with our new data
    memcpy(buffer + block_offset, write_buf + buf_offset, bytes_to_copy);
    // write modified block back to disk
    jbod_client_operation(disk_seek_op, NULL);
    jbod_client_operation(block_seek_op, NULL);
    jbod_client_operation(write_op, buffer);
    
    // if our cache is enabled, insert the modified block for next time
    if (cache_enabled()) {
      cache_insert(disk_num, block_num, buffer);
    }
    // update our position in both the device and write buffer for next loop
    curr += bytes_to_copy;
    buf_offset += bytes_to_copy;
  }

  return write_len;
}

