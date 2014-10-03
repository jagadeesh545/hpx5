// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
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

#include <string.h>
#include "libhpx/locality.h"
#include "gva.h"
#include "heap.h"
#include "pgas.h"

hpx_action_t pgas_cyclic_alloc = 0;
hpx_action_t pgas_cyclic_calloc = 0;
hpx_action_t pgas_memset = 0;

/// Allocate from the cyclic space.
///
/// This is performed at the single cyclic server node (usually rank 0), and
/// doesn't need to be broadcast because the server controls this for
/// everyone. All global cyclic allocations are rooted at rank 0.
///
/// @param        n The number of blocks to allocate.
/// @param    bsize The block size for this allocation.
///
/// @returns The base address of the global allocation.
hpx_addr_t pgas_cyclic_alloc_sync(size_t n, uint32_t bsize) {
  const uint32_t ranks = here->ranks;
  const uint64_t blocks_per_locality = pgas_n_per_locality(n, here->ranks);
  const uint32_t padded_bsize = pgas_fit_log2_32(bsize);
  const uint64_t heap_offset = heap_csbrk(global_heap, blocks_per_locality,
                                          padded_bsize);
  const uint32_t rank = here->rank;
  const pgas_gva_t gva = pgas_gva_from_heap_offset(rank, heap_offset, ranks);
  return pgas_gva_to_hpx_addr(gva);
}

/// Allocate zeroed memory from the cyclic space.
///
/// This is performed at the single cyclic server node (usually rank 0) and is
/// broadcast to all of the ranks in the system using hpx_bcast(). It waits for
/// the broadcast to finish before returning. All global cyclic allocations are
/// rooted at rank 0.
///
/// @param        n The number of blocks to allocate.
/// @param    bsize The block size for this allocation.
///
/// @returns The base address of the global allocation.
hpx_addr_t pgas_cyclic_calloc_sync(size_t n, uint32_t bsize) {
  const uint32_t ranks = here->ranks;
  const uint64_t blocks_per_locality = pgas_n_per_locality(n, ranks);
  const uint32_t padded_bsize = pgas_fit_log2_32(bsize);
  const size_t heap_offset = heap_csbrk(global_heap, blocks_per_locality,
                                        padded_bsize);

  pgas_memset_args_t args = {
    .heap_offset = heap_offset,
    .value = 0,
    .length = blocks_per_locality * padded_bsize
  };


  hpx_addr_t sync = hpx_lco_and_new(ranks);
  hpx_bcast(pgas_memset, &args, sizeof(args), sync);
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);
  const uint32_t rank = here->rank;
  const pgas_gva_t gva = pgas_gva_from_heap_offset(rank, heap_offset, ranks);
  return pgas_gva_to_hpx_addr(gva);
}


/// This is the hpx_call_* target for a cyclic allocation.
static int _pgas_cyclic_alloc_handler(pgas_alloc_args_t *args) {
  hpx_addr_t addr = pgas_cyclic_alloc_sync(args->n, args->bsize);
  HPX_THREAD_CONTINUE(addr);
}

/// This is the hpx_call_* target for cyclic zeroed allocation.
static int _pgas_cyclic_calloc_handler(pgas_alloc_args_t *args) {
  hpx_addr_t addr = pgas_cyclic_calloc_sync(args->n, args->bsize);
  HPX_THREAD_CONTINUE(addr);
}

/// This is the hpx_call_* target for memset, used in calloc broadcast.
static int _pgas_memset_handler(pgas_memset_args_t *args) {
  void *dest = heap_offset_to_local(global_heap, args->heap_offset);
  memset(dest, args->value, args->length);
  return HPX_SUCCESS;
}

void pgas_register_actions(void) {
  pgas_cyclic_alloc = HPX_REGISTER_ACTION(_pgas_cyclic_alloc_handler);
  pgas_cyclic_calloc = HPX_REGISTER_ACTION(_pgas_cyclic_calloc_handler);
  pgas_memset = HPX_REGISTER_ACTION(_pgas_memset_handler);
}

static void HPX_CONSTRUCTOR _register(void) {
  pgas_register_actions();
}
