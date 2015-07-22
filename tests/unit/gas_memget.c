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
#include <inttypes.h>
#include <stdlib.h>
#include "hpx/hpx.h"
#include "tests.h"

// Size of the data we're transferring.
enum { ELEMENTS = 32 };

static hpx_addr_t   _data = 0;
static hpx_addr_t _remote = 0;

static void HPX_NORETURN _fail(int i, uint64_t expected, uint64_t actual) {
  fprintf(stderr, "failed to set element %d correctly, "
          "expected %" PRIu64 ", got %" PRIu64 "\n", i, expected, actual);
  exit(EXIT_FAILURE);
}

static int _verify(uint64_t *local) {
  for (int i = 0, e = ELEMENTS; i < e; ++i) {
    if (local[i] != i) {
      _fail(i, i, local[i]);
    }
  }
  return HPX_SUCCESS;
}

static int _init_handler(uint64_t *local) {
  for (int i = 0; i < ELEMENTS; ++i) {
    local[i] = i;
  }
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, HPX_PINNED, _init, _init_handler, HPX_POINTER);

static int _init_globals_handler(void) {
  size_t n = ELEMENTS * sizeof(uint64_t);
  int rank = HPX_LOCALITY_ID;
  int size = HPX_LOCALITIES;
  int peer = (rank + 1) % size;
  _data = hpx_gas_alloc_cyclic(HPX_LOCALITIES, n, 0);
  assert(_data);
  _remote = hpx_addr_add(_data, peer * n, n);
  assert(_remote);
  return hpx_call_sync(_remote, _init, NULL, 0);
}
static HPX_ACTION(HPX_DEFAULT, 0, _init_globals, _init_globals_handler);

static int _fini_globals_handler(void) {
  hpx_gas_free(_data, HPX_NULL);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _fini_globals, _fini_globals_handler);

static int gas_memget_sync_stack_handler(void) {
  printf("Testing memget_sync to a stack address\n");
  uint64_t local[ELEMENTS];
  hpx_gas_memget_sync(local, _remote, sizeof(local));
  return _verify(local);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memget_sync_stack,
                  gas_memget_sync_stack_handler);


static int gas_memget_sync_registered_handler(void) {
  printf("Testing memget_sync to a registered address\n");
  size_t n = ELEMENTS * sizeof(uint64_t);
  uint64_t *local = hpx_malloc_registered(n);
  hpx_gas_memget_sync(local, _remote, n);
  _verify(local);
  hpx_free_registered(local);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memget_sync_registered,
                  gas_memget_sync_registered_handler);

static int gas_memget_sync_global_handler(void) {
  printf("Testing memget_sync to a global address\n");
  static uint64_t local[ELEMENTS];
  hpx_gas_memget_sync(local, _remote, sizeof(local));
  return _verify(local);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memget_sync_global,
                  gas_memget_sync_global_handler);

static int gas_memget_sync_malloc_handler(void) {
  printf("Testing memget_sync to a malloced address\n");
  size_t n = ELEMENTS * sizeof(uint64_t);
  uint64_t *local = malloc(n);
  hpx_gas_memget_sync(local, _remote, n);
  _verify(local);
  free(local);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memget_sync_malloc,
                  gas_memget_sync_malloc_handler);

static int gas_memget_stack_handler(void) {
  printf("Testing memget to a stack address\n");
  uint64_t local[ELEMENTS];
  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_gas_memget(local, _remote, sizeof(local), done);
  hpx_lco_wait(done);
  _verify(local);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memget_stack,
                  gas_memget_stack_handler);


static int gas_memget_registered_handler(void) {
  printf("Testing memget to a registered address\n");
  size_t n = ELEMENTS * sizeof(uint64_t);
  uint64_t *local = hpx_malloc_registered(n);
  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_gas_memget(local, _remote, n, done);
  hpx_lco_wait(done);
  _verify(local);
  hpx_free_registered(local);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memget_registered,
                  gas_memget_registered_handler);

static int gas_memget_global_handler(void) {
  printf("Testing memget to a global address\n");
  static uint64_t local[ELEMENTS];
  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_gas_memget(local, _remote, sizeof(local), done);
  hpx_lco_wait(done);
  _verify(local);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memget_global,
                  gas_memget_global_handler);

static int gas_memget_malloc_handler(void) {
  printf("Testing memget to a malloced address\n");
  size_t n = ELEMENTS * sizeof(uint64_t);
  uint64_t *local = malloc(n);

  hpx_addr_t done = hpx_lco_future_new(0);
  hpx_gas_memget(local, _remote, n, done);
  hpx_lco_wait(done);
  _verify(local);
  free(local);
  hpx_call_cc(done, hpx_lco_delete_action, NULL, NULL);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_memget_malloc,
                  gas_memget_malloc_handler);

TEST_MAIN({
    ADD_TEST(_init_globals);
    ADD_TEST(gas_memget_stack);
    ADD_TEST(gas_memget_sync_stack);
    ADD_TEST(gas_memget_registered);
    ADD_TEST(gas_memget_sync_registered);
    ADD_TEST(gas_memget_global);
    ADD_TEST(gas_memget_sync_global);
    ADD_TEST(gas_memget_malloc);
    ADD_TEST(gas_memget_sync_malloc);
    ADD_TEST(_fini_globals);
  });
