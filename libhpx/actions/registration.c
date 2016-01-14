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

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/padding.h>
#ifdef HAVE_PERCOLATION
#include <libhpx/percolation.h>
#endif
#include "init.h"

/// A static action table.
///
/// We currently need to be able to register actions before we call hpx_init()
/// because we use constructors inside of libhpx to do action registration. We
/// expose this action table to be used for that purpose.
static int _n = 1;
HPX_ALIGNED(HPX_PAGE_SIZE) action_t actions[LIBHPX_ACTION_MAX];

static void HPX_CONSTRUCTOR _init_null_handler(void) {
  actions[0] = (action_t){
    .handler = NULL,
    .id = NULL,
    .key = "",
    .type = 0,
    .attr = UINT32_C(0),
    .cif = NULL,
    .env = NULL
  };
}

int action_table_size(void) {
  return _n;
}

#ifdef ENABLE_DEBUG
void CHECK_ACTION(hpx_action_t id) {
  if (id == HPX_ACTION_INVALID) {
    dbg_error("action registration is not complete");
  }
  else if (id >= _n) {
    dbg_error("action id, %d, out of bounds [0,%u)\n", id, _n);
  }
}
#endif

/// Insert an action into the table.
///
/// @param           id The address of the user's id; written in _assign_ids().
/// @param          key The unique key for this action; read in _sort_entries().
/// @param            f The handler for this action.
/// @param         type The type of this action.
/// @param         attr The attributes associated with this action.
/// @param          cif FFI datatype information.
/// @param          env Action's environment.
///
/// @return             HPX_SUCCESS or an error if the push fails.
static int _push_back(hpx_action_t *id, const char *key, handler_t f,
                      hpx_action_type_t type, uint32_t attr, ffi_cif *cif,
                      void *env) {
  action_t *back = &actions[_n++];
  if (_n >= LIBHPX_ACTION_MAX) {
    dbg_error("action table overflow\n");
  }
  back->handler = f;
  back->id = id;
  back->key = key;
  back->type = type;
  back->attr = attr;
  back->cif = cif;
  back->env = env;
  action_init_handlers(back);
  return HPX_SUCCESS;
}

/// Compare two entries by their keys.
///
/// This is used to sort the action table during finalization, so that we can
/// uniformly assign ids to actions independent of which region in the local
/// address space the functions are loaded into.
///
/// @param          lhs A pointer to the left-hand entry.
/// @param          rhs A pointer tot he right-hand entry.
///
/// @return             The lexicographic comparison of the entries' keys.
static int _cmp_keys(const void *lhs, const void *rhs) {
  const action_t *el = lhs;
  const action_t *er = rhs;

  // if either the left or right entry's id is NULL, that means it is
  // our reserved action (user-registered actions can never have the
  // ID as NULL), and we always want it to be "less" than any other
  // registered action.
  if (el->id == NULL) {
    return -1;
  } else if (er->id == NULL) {
    return 1;
  } else {
    return strcmp(el->key, er->key);
  }
}

/// Sort the actions in an action table by their key.
static void _sort_entries(void) {
  qsort(&actions, _n, sizeof(action_t), _cmp_keys);
}

/// Assign all of the entry ids in the table.
static void _assign_ids(void) {
  for (int i = 1, e = _n; i < e; ++i) {
    *actions[i].id = i;
  }
}

void action_registration_finalize(void) {
  _sort_entries();
  _assign_ids();

  for (int i = 1, e = _n; i < e; ++i) {
    const char *key = actions[i].key;
    hpx_action_type_t type = actions[i].type;
    void (*f)(void) = actions[i].handler;
    (void) key, (void) type, (void) f;

#ifdef HAVE_PERCOLATION
    if (here->percolation && type == HPX_OPENCL) {
      void *env = percolation_prepare(here->percolation, key, (const char*)f);
      dbg_assert_str(env, "failed to prepare percolation kernel: %s\n", key);
      actions[i].handler = (handler_t)percolation_execute_handler;
    }
#endif

    log_action("%d: %s (%p) %s %x.\n", *actions[i].id,
               key, (void*)(uintptr_t)f, HPX_ACTION_TYPE_TO_STRING[type],
               actions[i].attr);
  }

  // this is a sanity check to ensure that the reserved "null" action
  // is still at index 0.
  dbg_assert(actions[0].id == NULL);

#ifndef HAVE_HUGETLBFS
  size_t bytes = _n * sizeof(actions[0]);
  bytes += _BYTES(HPX_PAGE_SIZE, bytes);
  int e1 = mprotect(&actions, bytes, PROT_READ);
  if (e1) {
    log_error("could not protect the action table\n");
  }
#endif
}

void action_table_finalize(void) {
  for (int i = 0, e = _n; i < e; ++i) {
    ffi_cif *cif = actions[i].cif;
    if (cif) {
      free(cif->arg_types);
      free(cif);
    }

#ifdef HAVE_PERCOLATION
    void *env = actions[i].env;
    if (env && actions[i].type == HPX_OPENCL) {
      percolation_destroy(here->percolation, env);
    }
#endif
  }
}

static int _register_action_va(hpx_action_type_t type, uint32_t attr,
                               const char *key, hpx_action_t *id, handler_t f,
                               int system, unsigned int nargs, va_list vargs) {
  dbg_assert(id);
  *id = HPX_ACTION_INVALID;

  if (system) {
    attr |= HPX_INTERNAL;
  }

  bool marshalled = attr & HPX_MARSHALLED;
  bool     pinned = attr & HPX_PINNED;
  bool   vectored = attr & HPX_VECTORED;

  if (!marshalled) {
    ffi_cif *cif = calloc(1, sizeof(*cif));
    dbg_assert(cif);

    hpx_type_t *args = calloc(nargs, sizeof(args[0]));
    for (int i = 0; i < nargs; ++i) {
      args[i] = va_arg(vargs, hpx_type_t);
    }

    ffi_status s = ffi_prep_cif(cif, FFI_DEFAULT_ABI, nargs, HPX_INT, args);
    if (s != FFI_OK) {
      dbg_error("failed to process type information for action id %d.\n", *id);
    }

    return _push_back(id, key, f, type, attr, cif, NULL);
  }

  if (pinned) {
    hpx_type_t translated = va_arg(vargs, hpx_type_t);
    if (translated != HPX_POINTER) {
      dbg_error("First type of a pinned action should be HPX_POINTER\n");
    }
  }

  if (vectored) {
    hpx_type_t count = va_arg(vargs, hpx_type_t);
    hpx_type_t args = va_arg(vargs, hpx_type_t);
    hpx_type_t sizes = va_arg(vargs, hpx_type_t);

    if ((count != HPX_INT && count != HPX_UINT && count != HPX_SIZE_T) ||
        (args != HPX_POINTER) ||
        (sizes != HPX_POINTER)) {
      dbg_error("Vectored registration type failure\n");
    }

    (void)count;
    (void)args;
    (void)sizes;
  } else {
    hpx_type_t addr = va_arg(vargs, hpx_type_t);
    hpx_type_t size = va_arg(vargs, hpx_type_t);

    if ((addr != HPX_POINTER) ||
        (size != HPX_INT && size != HPX_UINT && size != HPX_SIZE_T)) {
      dbg_error("Marshalled action type should be HPX_POINTER, HPX_INT\n");
    }
  }

  va_end(vargs);
  return _push_back(id, key, f, type, attr, NULL, NULL);
}

int libhpx_register_action(hpx_action_type_t type, uint32_t attr,
                           const char *key, hpx_action_t *id, handler_t f,
                           unsigned nargs, ...) {
  va_list vargs;
  va_start(vargs, nargs);
  int e = _register_action_va(type, attr, key, id, f, 1, nargs, vargs);
  va_end(vargs);
  return e;
}

int hpx_register_action(hpx_action_type_t type, uint32_t attr, const char *key,
                        hpx_action_t *id, handler_t f, unsigned nargs,
                        ...) {
  va_list vargs;
  va_start(vargs, nargs);
  int e = _register_action_va(type, attr, key, id, f, 0, nargs, vargs);
  va_end(vargs);
  return e;
}
