// ==================================================================-*- C++ -*-
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

#ifndef LIBHPX_NETWORK_ISIR_MPI_TRANSPORT_H
#define LIBHPX_NETWORK_ISIR_MPI_TRANSPORT_H

#include "hpx/hpx.h"
#include "libhpx/debug.h"
#include <mpi.h>
#include <algorithm>
#include <exception>
#include <new>
#include <alloca.h>
#include <cassert>
#include <cstddef>

namespace libhpx {
namespace network {
namespace isir {
class MPITransport {
 public:
  typedef MPI_Comm Communicator;                //!< for legacy collectives
  typedef MPI_Request Request;
  typedef MPI_Status Status;

  MPITransport() : world_(MPI_COMM_NULL), finalize_(false) {
    int initialized;
    Check(MPI_Initialized(&initialized));
    if (!initialized) {
      int level;
      Check(MPI_Init_thread(nullptr, nullptr, MPI_THREAD_SERIALIZED, &level));
      assert(level >= MPI_THREAD_SERIALIZED);
      finalize_ = true;
    }
    Check(MPI_Comm_dup(MPI_COMM_WORLD, &world_));
  }

  ~MPITransport() {
    Check(MPI_Comm_free(&world_));
    if (finalize_) {
      MPI_Finalize();
    }
  }

  static bool cancel(Request& request) {
    if (request == MPI_REQUEST_NULL) {
      return true;
    }

    int cancelled;
    MPI_Status status;
    MPITransport::Check(MPI_Cancel(&request));
    MPITransport::Check(MPI_Wait(&request, &status));
    MPITransport::Check(MPI_Test_cancelled(&status, &cancelled));
    request = MPI_REQUEST_NULL;
    return cancelled;
  }

  static int source(const Status& status) {
    return status.MPI_SOURCE;
  }

  static int bytes(const Status& status) {
    int bytes;
    MPITransport::Check(MPI_Get_count(&status, MPI_BYTE, &bytes));
    return bytes;
  }

  int iprobe() {
    Status status;
    int flag;
    Check(MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, world_, &flag, &status));
    return (flag) ? status.MPI_TAG : -1;
  }

  Request isend(int to, const void *from, size_t n, int tag) {
    Request request;
    Check(MPI_Isend(from, n, MPI_BYTE, to, tag, world_, &request));
    return request;
  }

  Request irecv(void *to, size_t n, int tag) {
    Request request;
    Check(MPI_Irecv(to, n, MPI_BYTE, MPI_ANY_SOURCE, tag, world_, &request));
    return request;
  }

  static int Testsome(int n, Request* reqs, int* out) {
    if (!n) return 0;
    int ncomplete;
    MPI_Request *requests = reinterpret_cast<MPI_Request*>(reqs);
    Check(MPI_Testsome(n, requests, &ncomplete, out, MPI_STATUS_IGNORE));
    if (ncomplete == MPI_UNDEFINED)  {
      throw std::exception();
    }
    return ncomplete;
  }

  static int Testsome(int n, Request* reqs, int* out, Status* statuses) {
    if (!n) {
      return 0;
    }

    int ncomplete;
    MPI_Request *requests = reinterpret_cast<MPI_Request*>(reqs);
    Check(MPI_Testsome(n, requests, &ncomplete, out, statuses));
    if (ncomplete == MPI_UNDEFINED)  {
      throw std::exception();
    }
    return ncomplete;
  }

  void *comm() {
    return &world_;
  }

  void createComm(Communicator *out, int n, const int ranks[])
  {
    int w;
    Check(MPI_Comm_size(world_, &w));
    if (n == w) {
      Check(MPI_Comm_dup(world_, out));
    }
    else {
      MPI_Group all, active;
      Check(MPI_Comm_group(world_, &all));
      Check(MPI_Group_incl(all, n, ranks, &active));
      Check(MPI_Comm_create(world_, active, out));
    }
  }

  void allreduce(void *sendbuf, void *result, int count, void *datatype,
                 hpx_monoid_op_t *op, Communicator *comm)
  {
    int bytes = sizeof(CollectiveArg) + count;
    CollectiveArg* in = new(alloca(bytes)) CollectiveArg(*op, count, sendbuf);
    CollectiveArg* out = new(alloca(bytes)) CollectiveArg(*op, 0, nullptr);

    MPI_Op usrOp;
    Check(MPI_Op_create(CollectiveArg::Op, 1, &usrOp));
    Check(MPI_Allreduce(in, out, bytes, MPI_BYTE, usrOp, *comm));
    Check(MPI_Op_free(&usrOp));
    out->put(result);
  }

  void iallreduce(void *sendbuf, void *out, int bytes, void *datatype, void *op,
               Communicator *c, void *r)
  {

    MPI_Comm *comm = c;
    MPI_Op mpi_operation = MPI_SUM;
    MPI_Datatype mpi_datatype = MPI_INT;
    int count, mpi_int_sz ;
  
    //calculate default size
    MPI_Type_size(mpi_datatype, &mpi_int_sz);
    count = bytes/mpi_int_sz;

    MPI_Request *req = (MPI_Request*) r;

    if(op){
      to_mpi_optype(*(hpx_coll_optype_t*) op, &mpi_operation);
    }
    if(datatype){
      to_mpi_dtype(*(hpx_coll_dtype_t*) datatype, &mpi_datatype, bytes, &count);
    }

    Check(MPI_Iallreduce(sendbuf, out, count, mpi_datatype, mpi_operation, *comm, req));
    //log_net("started MPI_Iallreduce with count to %d\n", count);
  }

  static void pin(const void*, size_t, void*) {
  }

  static void unpin(const void*, size_t) {
  }

 private:
  static void Check(int e) {
    if (e != MPI_SUCCESS) {
      throw std::exception();
    }
  }

  static void to_mpi_optype(hpx_coll_optype_t optype, MPI_Op *mpi_opt){
    if(optype == HPX_COLL_SUM){
      *mpi_opt = MPI_SUM;
    } else if(optype == HPX_COLL_MIN){
      *mpi_opt = MPI_MIN;
    } else if(optype == HPX_COLL_MAX){
      *mpi_opt = MPI_MAX;
    } else if(optype == HPX_COLL_AND){
      *mpi_opt = MPI_LAND;
    } else if(optype == HPX_COLL_OR){
      *mpi_opt = MPI_LOR;
    }else if(optype == HPX_COLL_XOR){
      *mpi_opt = MPI_LXOR;
    }  else {
      log_error("failed to match a correct MPI operation, provided : %d."
		   " We are defaulting to MPI_SUM\n", optype);
      *mpi_opt = MPI_SUM;
    }
  }

  static void to_mpi_dtype(hpx_coll_dtype_t coll_type, MPI_Datatype *mpi_dt, int bytes, int *count){
    int mpi_dtype_sz;	
    if(coll_type == HPX_COLL_INT){
      *mpi_dt = MPI_INT;
    } else if(coll_type == HPX_COLL_LONG){
      *mpi_dt = MPI_LONG;
    } else if(coll_type == HPX_COLL_FLOAT){
      *mpi_dt = MPI_FLOAT;
    } else if(coll_type == HPX_COLL_SHORT){
      *mpi_dt = MPI_SHORT;
    } else if(coll_type == HPX_COLL_DOUBLE){
      *mpi_dt = MPI_DOUBLE;
    }else if(coll_type == HPX_COLL_CHAR){
      *mpi_dt = MPI_CHAR;
    } else {
      log_error("failed to match a correct MPI type , provided : %d. We are defaulting to MPI_INT type \n", 
		    coll_type);
      *mpi_dt = MPI_INT;
    }
    MPI_Type_size(*mpi_dt, &mpi_dtype_sz);
    *count = bytes/mpi_dtype_sz;
  }

  class CollectiveArg {
   public:
    CollectiveArg(hpx_monoid_op_t op, int size, void *data)
        : op_(op), size_(size)
    {
      if (data) {
        std::copy(data_, data_+size, static_cast<char*>(data));
      }
    }

    static void Op(void* lhs, void* rhs, int*, MPI_Datatype*)
    {
      auto in = static_cast<CollectiveArg*>(lhs);
      auto out = static_cast<CollectiveArg*>(rhs);
      in->op(out);
    }

    void put(void *out) const
    {
      std::move(data_, data_ + size_, static_cast<char*>(out));
    }

   private:
    void op(CollectiveArg* rhs) const {
      op_(rhs->data_, data_, size_);
    }

    hpx_monoid_op_t op_;
    int size_;
    char data_[];
  };

  MPI_Comm world_;
  bool  finalize_;
};

} // namespace isir
} // namespace network
} // namespace libhpx

#endif // LIBHPX_NETWORK_ISIR_MPI_TRANSPORT_H
