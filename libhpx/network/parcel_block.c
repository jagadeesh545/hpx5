// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <libsync/sync.h>
#include <hpx/builtins.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/padding.h>
#include <libhpx/parcel_block.h>

struct parcel_block {
  size_t remaining;
  PAD_TO_CACHELINE(sizeof(size_t));
  char bytes[];
};

_HPX_ASSERT(sizeof(parcel_block_t) == HPX_CACHELINE_SIZE, block_header_size);

parcel_block_t *parcel_block_new(size_t align, size_t n, size_t *offset) {
  dbg_assert_str(align == here->config->pwc_parcelbuffersize,
                 "Parcel block alignment is currently limited to "
                 "--hpx-pwc-parcelbuffersize (%zu), %zu requested\n",
                 here->config->pwc_parcelbuffersize, align);
  size_t bytes = n - sizeof(parcel_block_t);
  dbg_assert(bytes < n);
  parcel_block_t *block = registered_memalign(align, n);
  block->remaining = bytes;
  *offset = offsetof(parcel_block_t, bytes);
  log("allocated parcel block at %p\n", (void*)block);
  return block;
}

void parcel_block_delete(parcel_block_t *block) {
  if (block->remaining != 0) {
    log_parcel("block freed with %zu bytes remaining\n", block->remaining);
  }
  log_parcel("deleting parcel block at %p\n", (void*)block);
  registered_free(block);
}

void *parcel_block_at(parcel_block_t *block, size_t offset) {
  return &block->bytes[offset - sizeof(*block)];
}

void parcel_block_deduct(parcel_block_t *block, size_t bytes) {
  dbg_assert(bytes < SIZE_MAX/2);
  int64_t n = -bytes;
  size_t r = sync_addf(&block->remaining, n, SYNC_ACQ_REL);
  log_parcel("deducting %zu bytes from parcel block %p (%zu remain)\n", bytes,
             (void*)block, r);
  if (!r) {
    parcel_block_delete(block);
  }
}
