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
#include <config.h>
#endif

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "libhpx/locality.h"
#include "libhpx/transport.h"
#include "progress.h"


/// the MPI transport
typedef struct {
  transport_class_t class;
  progress_t *progress;
} mpi_t;


/// ----------------------------------------------------------------------------
/// Get the ID for an MPI transport.
/// ----------------------------------------------------------------------------
static const char *_mpi_id(void) {
  return "MPI";
}


/// ----------------------------------------------------------------------------
/// Use MPI barrier directly.
/// ----------------------------------------------------------------------------
static void _mpi_barrier(void) {
  MPI_Barrier(MPI_COMM_WORLD);
}


/// ----------------------------------------------------------------------------
/// Return the size of an MPI request.
/// ----------------------------------------------------------------------------
static int _mpi_request_size(void) {
  return sizeof(MPI_Request);
}


static int _mpi_adjust_size(int size) {
  return size;
}


/// ----------------------------------------------------------------------------
/// Cancel an active MPI request.
/// ----------------------------------------------------------------------------
static int _mpi_request_cancel(void *request) {
  return MPI_Cancel(request);
}


/// ----------------------------------------------------------------------------
/// Shut down MPI, and delete the transport.
/// ----------------------------------------------------------------------------
static void _mpi_delete(transport_class_t *transport) {
  mpi_t *mpi = (mpi_t*)transport;
  network_progress_delete(mpi->progress);
  int finalized;
  MPI_Finalized(&finalized);
  if (!finalized)
    MPI_Finalize();
  free(transport);
}


/// ----------------------------------------------------------------------------
/// Pinning not necessary.
/// ----------------------------------------------------------------------------
static void _mpi_pin(transport_class_t *transport, const void* buffer,
                     size_t len) {
}


/// ----------------------------------------------------------------------------
/// Unpinning not necessary.
/// ----------------------------------------------------------------------------
static void _mpi_unpin(transport_class_t *transport, const void* buffer,
                       size_t len) {
}


/// ----------------------------------------------------------------------------
/// Send data via MPI.
///
/// Presumably this will be an "eager" send. Don't use "data" until it's done!
/// ----------------------------------------------------------------------------
static int _mpi_send(transport_class_t *t, int dest, const void *data, size_t n,
                 void *r)
{
  void *b = (void*)data;
  int e = MPI_Isend(b, n, MPI_BYTE, dest, here->rank, MPI_COMM_WORLD, r);
  if (e != MPI_SUCCESS)
    return dbg_error("MPI could not send %lu bytes to %i.\n", n, dest);
  return HPX_SUCCESS;
}


/// ----------------------------------------------------------------------------
/// Probe MPI to see if anything has been received.
/// ----------------------------------------------------------------------------
static size_t _mpi_probe(transport_class_t *transport, int *source) {
  if (*source != TRANSPORT_ANY_SOURCE) {
    dbg_error("mpi transport can not currently probe source %d\n", *source);
    return 0;
  }

  int flag = 0;
  MPI_Status status;
  int e = MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag,
                     &status);

  if (e != MPI_SUCCESS) {
    dbg_error("mpi failed Iprobe.\n");
    return 0;
  }

  if (!flag)
    return 0;

  int bytes = 0;
  e = MPI_Get_count(&status, MPI_BYTE, &bytes);
  if (e != MPI_SUCCESS) {
    dbg_error("could not extract bytes from mpi.\n");
    return 0;
  }

  // update the source to the actual source, and return the number of bytes
  // available
  *source = status.MPI_SOURCE;
  return bytes;
}


/// ----------------------------------------------------------------------------
/// Receive a buffer.
/// ----------------------------------------------------------------------------
static int _mpi_recv(transport_class_t *t, int src, void* buffer, size_t n, void *r)
{
  assert(src != TRANSPORT_ANY_SOURCE);
  assert(src >= 0);
  assert(src < here->ranks);

  int e = MPI_Irecv(buffer, n, MPI_BYTE, src, src, MPI_COMM_WORLD, r);
  if (e != MPI_SUCCESS)
    return dbg_error("could not receive %lu bytes from %i", n, src);

  return HPX_SUCCESS;
}


static int _mpi_test(transport_class_t *t, void *request, int *success) {
  int e = MPI_Test(request, success, MPI_STATUS_IGNORE);
  if (e != MPI_SUCCESS)
    return dbg_error("failed MPI_Test.\n");

  return HPX_SUCCESS;
}

static void _mpi_progress(transport_class_t *t, bool flush) {
  mpi_t *mpi = (mpi_t*)t;
  network_progress_poll(mpi->progress);
  if (flush)
    network_progress_flush(mpi->progress);
}

transport_class_t *transport_new_mpi(void) {
  int val = 0;
  MPI_Initialized(&val);

  if (!val) {
    int threading = 0;
    if (MPI_Init_thread(NULL, NULL, MPI_THREAD_SERIALIZED, &threading) !=
        MPI_SUCCESS)
      return NULL;

    dbg_log("thread_support_provided = %d\n", threading);
  }

  mpi_t *mpi = malloc(sizeof(*mpi));
  mpi->class.id             = _mpi_id;
  mpi->class.barrier        = _mpi_barrier;
  mpi->class.request_size   = _mpi_request_size;
  mpi->class.request_cancel = _mpi_request_cancel;
  mpi->class.adjust_size    = _mpi_adjust_size;

  mpi->class.delete         = _mpi_delete;
  mpi->class.pin            = _mpi_pin;
  mpi->class.unpin          = _mpi_unpin;
  mpi->class.send           = _mpi_send;
  mpi->class.probe          = _mpi_probe;
  mpi->class.recv           = _mpi_recv;
  mpi->class.test           = _mpi_test;
  mpi->class.progress       = _mpi_progress;

  mpi->progress             = network_progress_new();
  if (!mpi->progress) {
    dbg_error("failed to start the transport progress loop.\n");
    hpx_abort();
  }
  return &mpi->class;
}
