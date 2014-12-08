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

#ifndef LIBPXGL_TERMINATION_H
#define LIBPXGL_TERMINATION_H

#include "pxgl/pxgl.h"

void _increment_active_count(sssp_uint_t n);
void _increment_finished_count();
termination_t _get_termination();
void _detect_termination(const hpx_addr_t termination_lco, const hpx_addr_t internal_termination_lco);
extern hpx_action_t _initialize_termination_detection;
extern termination_t _termination;

#endif // LIBPXGL_TERMINATION_H
