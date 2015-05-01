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
#ifndef LIBHPX_GAS_AGAS_BTT_H
#define LIBHPX_GAS_AGAS_BTT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hpx/hpx.h>
#include <hpx/attributes.h>
#include "gva.h"

HPX_INTERNAL void *btt_new(size_t size);
HPX_INTERNAL void btt_delete(void *btt);

HPX_INTERNAL void btt_insert(void *btt, gva_t gva, int32_t owner,
                             void *lva);
HPX_INTERNAL void btt_remove(void *btt, gva_t gva);

HPX_INTERNAL bool btt_try_pin(void *btt, gva_t gva, void **lva);
HPX_INTERNAL void btt_unpin(void *btt, gva_t gva);
HPX_INTERNAL uint32_t btt_owner_of(const void *btt, gva_t gva);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_GAS_AGAS_BTT_H
