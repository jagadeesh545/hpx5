// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

/// @file libhpx/scheduler/dataflow.c
/// @brief A dataflow LCO.
#include <assert.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>

#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/memory.h"
#include "libhpx/scheduler.h"
#include "cvar.h"
#include "lco.h"

/// Generic LCO interface.
/// @{
typedef struct {
  lco_t         lco;
  cvar_t       cvar;
} _dataflow_t;

static void _reset(_dataflow_t *d) {
  dbg_assert_str(cvar_empty(&d->cvar),
                 "Reset on LCO that has waiting threads.\n");
  log_lco("resetting dataflow LCO %p\n", (void*)d);
  lco_reset_triggered(&d->lco);
  cvar_reset(&d->cvar);
}

static hpx_status_t _wait(_dataflow_t *d) {
  lco_t *lco = &d->lco;
  if (lco_get_triggered(lco)) {
    return cvar_get_error(&d->cvar);
  }
  else {
    return scheduler_wait(&lco->lock, &d->cvar);
  }
}

static bool _trigger(_dataflow_t *d) {
  if (lco_get_triggered(&d->lco)) {
    return false;
  }
  lco_set_triggered(&d->lco);
  return true;
}

static size_t _dataflow_size(lco_t *lco) {
  return sizeof(_dataflow_t);
}

/// Deletes a dataflow LCO.
static void _dataflow_fini(lco_t *lco) {
  if (!lco) {
    return;
  }

  lco_lock(lco);
  lco_fini(lco);
}

/// Handle an error condition.
static void _dataflow_error(lco_t *lco, hpx_status_t code) {
  lco_lock(lco);
  _dataflow_t *d = (_dataflow_t *)lco;
  _trigger(d);
  scheduler_signal_error(&d->cvar, code);
  lco_unlock(lco);
}

static void _dataflow_reset(lco_t *lco) {
  _dataflow_t *u = (_dataflow_t *)lco;
  lco_lock(lco);
  _reset(u);
  lco_unlock(lco);
}

static hpx_status_t _dataflow_attach(lco_t *lco, hpx_parcel_t *p) {
}

/// Invoke a set operation on the dataflow LCO.
static int _dataflow_set(lco_t *lco, int size, const void *from) {
  return HPX_SUCCESS;
}

/// Invoke a get operation on the dataflow LCO.
static hpx_status_t _dataflow_get(lco_t *lco, int size, void *out, int reset) {
}

// Invoke a wait operation on the dataflow LCO.
static hpx_status_t _dataflow_wait(lco_t *lco, int reset) {
}

// Get the reference to the reduction.
static hpx_status_t _dataflow_getref(lco_t *lco, int size, void **out, int *unpin) {
  return HPX_SUCCESS;
}

// Release the reference to the buffer.
static int _dataflow_release(lco_t *lco, void *out) {
  return 1;
}

// vtable
static const lco_class_t _dataflow_vtable = {
  .type        = LCO_DATAFLOW,
  .on_fini     = _dataflow_fini,
  .on_error    = _dataflow_error,
  .on_set      = _dataflow_set,
  .on_attach   = _dataflow_attach,
  .on_get      = _dataflow_get,
  .on_getref   = _dataflow_getref,
  .on_release  = _dataflow_release,
  .on_wait     = _dataflow_wait,
  .on_reset    = _dataflow_reset,
  .on_size     = _dataflow_size
};

static void HPX_CONSTRUCTOR _register_vtable(void) {
  lco_vtables[LCO_DATAFLOW] = &_dataflow_vtable;
}
/// @}

static int
_dataflow_init_handler(_dataflow_t *d) {
  log_lco("initializing dataflow LCO %p\n", (void*)d);
  lco_init(&d->lco, &_dataflow_vtable);
  cvar_reset(&d->cvar);
  return HPX_SUCCESS;
}
static LIBHPX_ACTION(HPX_DEFAULT, HPX_PINNED,
                     _dataflow_init_action, _dataflow_init_handler,
                     HPX_POINTER);

hpx_addr_t hpx_lco_dataflow_new(int lcos) {
  _dataflow_t *d = NULL;
  hpx_addr_t gva = lco_alloc_local(1, sizeof(*d), 0);

  if (!hpx_gas_try_pin(gva, (void**)&d)) {
    int e = hpx_call_sync(gva, _dataflow_init_action, NULL, 0);
    dbg_check(e, "could not initialize the dataflow LCO at %"PRIu64"\n", gva);
  } else {
    LCO_LOG_NEW(gva, d);
    _dataflow_init_handler(d);
    hpx_gas_unpin(gva);
  }
  return gva;
}

int _hpx_lco_dataflow_add(hpx_addr_t lco, hpx_action_t action, hpx_addr_t out, int n, ...) {
  
}
