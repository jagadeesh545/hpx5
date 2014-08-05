/// ---------------------------------------------------------------------------
/// @file fmm-types.h
/// @author Bo Zhang <zhang416 [at] indiana.edu>
/// @brief FMM types 
/// ---------------------------------------------------------------------------
#pragma once
#ifndef FMM_TYPES_H
#define FMM_TYPES_H

#include <complex.h>
#include "hpx/hpx.h"

/// ---------------------------------------------------------------------------
/// @brief Source point type
/// ---------------------------------------------------------------------------
typedef struct source_t {
  double position[3]; ///< position of the source point
  double charge; ///< strength of the source point
  int rank; ///< original input order
} source_t; 

/// ---------------------------------------------------------------------------
/// @brief Target point type
/// ---------------------------------------------------------------------------
typedef struct target_t {
  double position[3]; ///< position of the target point
  double potential; ///< potential at the target point 
  double field[3]; ///< field at the target point
  int rank; ///< original input order
} target_t; 

/// ---------------------------------------------------------------------------
/// @brief FMM box type
/// ---------------------------------------------------------------------------
typedef struct fmm_box_t fmm_box_t; 

struct fmm_box_t {
  int level; ///< level of the box
  hpx_addr_t parent; ///< address of the parent box
  hpx_addr_t child[8]; ///< address of the child boxes
  int nchild; ///< number of child boxes
  int index[3]; ///< indices of the box in x, y, and z directions
  int npts; ///< number of points contained in the box
  int addr; ///< offset to find the first point contained in the box
  hpx_addr_t sema; ///< semaphore for reduction
  int n_reduce; ///< number of operands completing the reduction
  hpx_addr_t expan_avail; ///< lco indicating availability of the expansion
  double complex expansion[]; ///< storage for the expansion
}; 

/// ---------------------------------------------------------------------------
/// @brief FMM parameter type. 
/// @note The parameter is intended to be duplicated on each locality
/// ---------------------------------------------------------------------------
typedef struct {
  hpx_addr_t sources; ///< address for the source information
  hpx_addr_t targets; ///< address for the target information
  hpx_addr_t source_root; ///< address for the source root
  hpx_addr_t target_root; ///< address for the target root
  hpx_addr_t sema_done; ///< address for the termination detection semaphore
  hpx_addr_t fmm_done; ///< address of the lco for fmm completion detection
  double size; ///< size of the bounding boxes
  double corner[3]; ///< coordinate of the lower left corner of the bounding box
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

/// ---------------------------------------------------------------------------
/// @brief Argument passed to the _init_param action
/// ---------------------------------------------------------------------------
typedef struct {
  hpx_addr_t sources; ///< address for the source information
  hpx_addr_t targets; ///< address for the target information
  hpx_addr_t source_root; ///< address for the source root
  hpx_addr_t target_root; ///< address for the target root
  hpx_addr_t sema_done; ///< address for the termination detection semaphore
  hpx_addr_t fmm_done; ///< address of the lco for fmm completion detection
  double size; ///< size of the bounding box
  double corner[3]; ///< lower left corner of the bounding box
} init_param_action_arg_t; 

/// ---------------------------------------------------------------------------
/// @brief Argument passed to _swap action 
/// ---------------------------------------------------------------------------
typedef struct {
  char type; ///< source/target point 
  int addr; ///< address of first source/target point
  int npts; ///< number of points
  int index[3]; ///< index of the box 
  int level; ///< level of the box
} swap_action_arg_t; 

/// ---------------------------------------------------------------------------
/// @brief Argument passed to _set_box action
/// ---------------------------------------------------------------------------
typedef struct {
  char type; ///< source/target box
  int addr; ///< address of the first contained point 
  int npts; ///< number of points contained 
  int level; ///< level of the box 
  int index[3]; ///< index of the box
  hpx_addr_t parent; ///< parent of the box being set
} set_box_action_arg_t; 

/// ---------------------------------------------------------------------------
/// @brief Argument passed to _source_to_mpole action
/// ---------------------------------------------------------------------------
typedef struct {
  int addr; ///< address of the first contained point
  int npts; ///< number of source points
  int level; ///< level of the box
  int index[3]; ///< index of the box
} source_to_mpole_action_arg_t; 

/// ---------------------------------------------------------------------------
/// @brief Argument passed to the _disaggregate action
/// ---------------------------------------------------------------------------
typedef struct {
  hpx_addr_t plist1[27]; ///< list 1 of the parent box
  hpx_addr_t plist5[27]; ///< list 5 of the parent box
  int nplist1; ///< number of entries of plist1
  int nplist5; ///< number of entries of plist5
  double complex expansion[]; ///< translated local expansion from the parent
} disaggregate_action_arg_t; 

/// ---------------------------------------------------------------------------
/// @brief Argument passed to the _build_list1 action
/// ---------------------------------------------------------------------------
typedef struct {
  int index[3]; ///< index of the target box
  int level; ///< level of the target box
  hpx_addr_t box; ///< address of the target box
} build_list1_action_arg_t; 

/// ---------------------------------------------------------------------------
/// @brief Argument passed to the _source_to_local action
/// ---------------------------------------------------------------------------
typedef struct {
  int addr; ///< address for the first source point
  int npts; ///< number of source points
  int index[3]; ///< index of the target box
  int level; ///< level of the target box
  hpx_addr_t box; ///< address of the target box
  hpx_addr_t done; ///< lco tracking completion
} source_to_local_action_arg_t; 

/// ---------------------------------------------------------------------------
/// @brief Argument passed to the _merge_local action
/// ---------------------------------------------------------------------------
typedef struct {
  hpx_addr_t done; ///< lco tracking completion
  double complex expansion[]; ///< expansion
} merge_local_action_arg_t; 

/// ---------------------------------------------------------------------------
/// @brief Argument passed to the _expo_to_local action
/// ---------------------------------------------------------------------------
typedef struct {
  int index[3]; ///< index of the parent target box 
  hpx_addr_t child[8]; ///< address of the child target boxes
} expo_to_local_action_arg_t; 

typedef expo_to_local_action_arg_t merge_shift_action_arg_t; 

#endif
