/// ----------------------------------------------------------------------------
/// @file fmm-types.h
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief FMM types 
/// ----------------------------------------------------------------------------
#pragma once
#ifndef FMM_TYPES_H
#define FMM_TYPES_H

#include <complex.h>
#include "hpx/hpx.h"

typedef struct fmm_box_t fmm_box_t; 

/// ----------------------------------------------------------------------------
/// @brief FMM box type
/// ----------------------------------------------------------------------------
struct fmm_box_t {
  int level; ///< level of the box
  hpx_addr_t parent; ///< pointer to the parent
  hpx_addr_t child[8]; ///< pointers to the children
  int nchild; ///< number of child boxes
  int idx; ///< index, x-direction
  int idy; ///< index, y-direction
  int idz; ///< index, z-direction
  int npts; ///< number of points contained in the box
  int addr; ///< offset to locate the first point contained in the box
  char type; ///< type of the box: 'S' for source, 'T' for target
  hpx_addr_t list1[27]; ///< coarser or same level list 1 boxes 
  hpx_addr_t list5[27]; ///< same-level adjacent boxes
  int nlist1; ///< number of entries in list1
  int nlist5; ///< number of entries in list5
  hpx_addr_t reduce; ///< reduce lco for the box
  double complex expansion[]; ///< storage for expansion
}; 

/// ----------------------------------------------------------------------------
/// @brief FMM dag type
/// ----------------------------------------------------------------------------
typedef struct {
  int nslev; ///< maximum level of the source tree
  int nsboxes; ///< total number of boxes on the source tree
  int ntlev; ///< maximum level of the target tree
  int ntboxes; ///< total number of bxoes on the target tree
  double size; ///< size of the bounding box
  double corner[3]; ///< coordinate of the lower left corner of the bounding box
  hpx_addr_t source_root; ///< pointer to the root of the source tree
  hpx_addr_t target_root; ///< pointer to the root of the target tree
  int *mapsrc; ///< source mapping info
  int *maptar; ///< target mapping info
  int mapping[]; ///< storage for mapping info
} fmm_dag_t; 

/// ----------------------------------------------------------------------------
/// @brief FMM configuration type
/// ----------------------------------------------------------------------------
typedef struct {
  int nsources; ///< number of source points
  int ntargets; ///< number of target points
  int datatype; ///< type of data to generate
  int accuracy; ///< accuracy of the computation
  int s;        ///< partition criterion on box
} fmm_config_t; 

/// ----------------------------------------------------------------------------
/// @brief FMM parameter type
/// @note This is intended to be duplicated on each locality
/// ----------------------------------------------------------------------------
typedef struct {
  int pterms; ///< order of the multipole/local expansion
  int nlambs; ///< number of terms in the exponential expansion
  int pgsz; ///< buffer size for holding multipole/local expansion
  int nexptot; ///< total number of exponential expansion terms
  int nthmax; ///< maximum number of Fourier terms in the exponential expansion
  int nexptotp; ///< number of exponential expansions
  int nexpmax; ///< buffer size for holding exponential expansions
  int *numphys; ///< number of modes in the exponential expansion
  int *numfour; ///< number of Fourier modes in the expansion
  double *whts; ///< weights for the exponential expansion
  double *rlams; ///< nodes for the exponential expansion
  double *rdplus; ///< rotation matrix y->z
  double *rdminus; ///< rotation matrix z->x
  double *rdsq3; ///< shift multipole/local expansion +z direction
  double *rdmsq3; ///< shift multipole/local expansion -z direction
  double *dc; ///< coefficients for local translation along the z-axis
  double *ytopc; ///< precomputed vectors for factorials
  double *ytopcs; ///< precomputed vectors for factorials
  double *ytopcsinv; ///< precomputed vectors for factorials
  double *rlsc; ///< p_n^m for different lambda_k
  double *zs; ///< E2E operator, z-direction
  double *scale; ///< scaling factor at each level
  double complex *xs; ///< E2E operator, x-direction
  double complex *ys; ///< E2E operator, y-direction
  double complex *fexpe; ///< coefficients for merging exponentials
  double complex *fexpo; ///< coefficients for merging exponentials
  double complex *fexpback; ///< coefficients for merging exponentials
} fmm_param_t; 

#endif
