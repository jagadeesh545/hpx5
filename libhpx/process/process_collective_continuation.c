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
#include <hpx/hpx.h>
#include <libhpx/debug.h>
#include <libhpx/gpa.h>
#include <libhpx/network.h>
#include <libhpx/parcel.h>
#include "process_collective_continuation.h"

typedef struct {
  hpx_addr_t                 collective;
  hpx_parcel_t * volatile continuations;
  char                             data[];
} _element_t;

static int _element_init_handler(_element_t *element, hpx_addr_t gva) {
  element->collective = gva;
  element->continuations = NULL;
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, HPX_PINNED, _element_init,
                     _element_init_handler, HPX_POINTER, HPX_ADDR);

// static int _element_continue_handler(int source, uint64_t args) {
//   hpx_addr_t gva = offset_to_gpa(here->rank, args);
//   _element_t *element = NULL;
//   if (!hpx_gas_try_pin(gva, (void**)&element)) {
//     dbg_error("failed to pin a collective proxy at rank %d\n", here->rank);
//   }

//   hpx_parcel_t *p = NULL;
//   hpx_parcel_t *stack = sync_load(&element->continuations, SYNC_ACQUIRE);
//   sync_store(&element->continuations, NULL, SYNC_RELEASE);
//   while ((p = parcel_stack_pop(&stack))) {
//     dbg_assert(p->size);
//     hpx_parcel_set_data(p, element->data, p->size);
//     hpx_parcel_send(p, HPX_NULL);
//   }

//   hpx_gas_unpin(gva);
//   return HPX_SUCCESS;
// }
// static COMMAND_DEF(_element_continue, _element_continue_handler);

static int _element_continue_handler(_element_t *e, const void *b, size_t n) {
  hpx_parcel_t *p = NULL;
  hpx_parcel_t *stack = sync_load(&e->continuations, SYNC_ACQUIRE);
  sync_store(&e->continuations, NULL, SYNC_RELEASE);
  while ((p = parcel_stack_pop(&stack))) {
    hpx_parcel_set_data(p, b, n);
    hpx_parcel_send(p, HPX_NULL);
  }
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_INTERRUPT, HPX_PINNED | HPX_MARSHALLED,
                     _element_continue, _element_continue_handler, HPX_POINTER,
                     HPX_POINTER, HPX_SIZE_T);

hpx_addr_t process_collective_continuation_new(size_t size, hpx_addr_t gva) {
  size_t    bytes = sizeof(_element_t) + size;
  size_t    align = max_size_t(bytes, HPX_CACHELINE_SIZE);
  hpx_addr_t base = hpx_gas_alloc_cyclic(HPX_LOCALITIES, bytes, align);
  dbg_assert(base);

  hpx_addr_t sync = hpx_lco_and_new(HPX_LOCALITIES);
  for (int i = 0, e = HPX_LOCALITIES; i < e; ++i) {
    hpx_addr_t element = hpx_addr_add(base, i * bytes, bytes);
    dbg_check( hpx_call(element, _element_init, sync, &gva) );
  }
  hpx_lco_wait(sync);
  hpx_lco_delete(sync, HPX_NULL);
  return base;
}

hpx_addr_t process_collective_continuation_append(hpx_addr_t gva, size_t bytes,
                                                  hpx_addr_t c_action,
                                                  hpx_addr_t c_target) {
  hpx_addr_t    local = hpx_addr_add(gva, HPX_LOCALITY_ID * bytes, bytes);
  _element_t *element = NULL;
  if (!hpx_gas_try_pin(local, (void**)&element)) {
    dbg_error("could not pin local proxy for collective\n");
  }

  hpx_parcel_t *p = hpx_parcel_acquire(NULL, bytes);
  p->target = c_target;
  p->action = c_action;

  do {
    p->next = sync_load(&element->continuations, SYNC_RELAXED);
  } while (!sync_cas(&element->continuations, p->next, p, SYNC_ACQ_REL,
                     SYNC_RELAXED));
  return element->collective;
}

int process_collective_continuation_set_lsync(hpx_addr_t gva, size_t bytes,
                                              const void *buffer) {
  dbg_assert(bytes);
  dbg_assert(buffer);
  size_t bsize = sizeof(_element_t) * bytes;

  // hpx_addr_t and = hpx_lco_and_new(HPX_LOCALITIES);
  for (int i = 0, e = HPX_LOCALITIES; i < e; ++i) {
    hpx_addr_t element = hpx_addr_add(gva, i * bsize, bsize);
    hpx_call(element, _element_continue, HPX_NULL, buffer, bytes);
  }
  // int e = hpx_lco_wait(and);
  // hpx_lco_delete(and, HPX_NULL);
  // return e;
  return HPX_SUCCESS;
}
