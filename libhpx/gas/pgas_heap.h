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
#ifndef LIBHPX_GAS_PGAS_HEAP_H
#define LIBHPX_GAS_PGAS_HEAP_H

/// @file libhpx/gas/pgas_heap.h
/// @brief Implementation of the global address space with a PGAS model.
///
/// This implementation is similar to, and inspired by, the PGAS heap
/// implementation for UPC.
///
/// The PGAS heap implementation allocates one large region for the symmetric
/// heap, as requested by the application programmer. This region is dynamically
/// divided into cyclic and acyclic regions. Each locality manages its acyclic
/// region with a combination of jemalloc and a simple, locking-bitmap-based
/// jemalloc chunk allocator. The cyclic region is managed via an sbrk at the
/// root locality. The regions start out opposite each other in the space, and
/// grow towards each other.
///
///   +------------------------
///   | cyclic
///   |
///   | ...
///   |
///   |
///   | acyclic
///   +------------------------
///
/// We do not currently have any way to detect intersection of the cyclic and
/// acyclic regions, because the cyclic allocations are not broadcast. The root
/// has no way of knowing how much acyclic allocation each locality has
/// performed, which it would need to know to do the check.
///
/// @todo Implement a debugging mode where cyclic allocation is broadcast, so we
///       can detect intersections and report meaningful errors. Without this,
///       intersections lead to untraceable errors.
///

#include <stddef.h>
#include "bitmap_alloc.h"

typedef struct {
  volatile size_t       csbrk;
  size_t      bytes_per_chunk;
  size_t              nchunks;
  lhpx_bitmap_alloc_t *chunks;
  size_t               nbytes;
  void                 *bytes;
} lhpx_pgas_heap_t;

int lhpx_pgas_heap_init(lhpx_pgas_heap_t *heap, size_t size);
void lhpx_pgas_heap_fini(lhpx_pgas_heap_t *heap);

#endif
