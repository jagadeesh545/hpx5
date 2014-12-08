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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <inttypes.h>

#include "hpx/hpx.h"
#include "pxgl/pxgl.h"
#include "libsync/sync.h"
#include "libhpx/debug.h"

static void _usage(FILE *stream) {
  fprintf(stream, "Usage: sssp [options] <graph-file> <problem-file>\n"
          "\t-k, use and-lco-based termination detection\n"
          "\t-p, use process-based termination detection\n"
          "\t-q, limit time for SSSP executions in seconds\n"
          "\t-a, instead resetting adj list between the runs, reallocate it\n"
          "\t-h, this help display\n");
  hpx_print_help();
  fflush(stream);
}


static hpx_action_t _print_vertex_distance;
static int _print_vertex_distance_action(int *i)
{
  const hpx_addr_t target = hpx_thread_current_target();

  adj_list_vertex_t *vertex;
  if (!hpx_gas_try_pin(target, (void**)&vertex))
    return HPX_RESEND;

  printf("vertex: %d nbrs: %lu dist: %lu\n", *i, vertex->num_edges, vertex->distance);

  hpx_gas_unpin(target);
  return HPX_SUCCESS;
}


static hpx_action_t _print_vertex_distance_index;
static int _print_vertex_distance_index_action(int *i)
{
  const hpx_addr_t target = hpx_thread_current_target();

  hpx_addr_t *v;
  hpx_addr_t vertex;

  if (!hpx_gas_try_pin(target, (void**)&v))
    return HPX_RESEND;

  vertex = *v;
  hpx_gas_unpin(target);

  return hpx_call_sync(vertex, _print_vertex_distance, i, sizeof(*i), NULL, 0);
}


static int _read_dimacs_spec(char **filename, sssp_uint_t *nproblems, sssp_uint_t **problems) {
  FILE *f = fopen(*filename, "r");
  assert(f);

  char line[LINE_MAX];
  int count = 0;
  while (fgets(line, sizeof(line), f) != NULL) {
    switch (line[0]) {
      case 'c': continue;
      case 's':
        sscanf(&line[1], " %lu", &((*problems)[count++]));
        break;
      case 'p':
        sscanf(&line[1], " aux sp ss %" SSSP_UINT_PRI, nproblems);
        *problems = malloc(*nproblems * sizeof(sssp_uint_t));
        assert(*problems);
        break;
      default:
        fprintf(stderr, "invalid command specifier '%c' in problem file. skipping..\n", line[0]);
        continue;
    }
  }
  fclose(f);
  return 0;
}

// Arguments for the main SSSP action
typedef struct {
  char *filename;
  sssp_uint_t nproblems;
  sssp_uint_t *problems;
  char *prob_file;
  sssp_uint_t time_limit;
  int realloc_adj_list;
  sssp_kind_t sssp_kind;
  sssp_init_dc_args_t sssp_init_dc_args;
  size_t delta;
} _sssp_args_t;

static hpx_action_t _main;
static int _main_action(_sssp_args_t *args) {
  const int realloc_adj_list = args->realloc_adj_list;

  // Create an edge list structure from the given filename
  edge_list_t el;
  printf("Allocating edge-list from file %s.\n", args->filename);
  const edge_list_from_file_args_t edge_list_from_file_args = {
    .locality_readers = HPX_LOCALITIES,
    .thread_readers = 1,
    .filename = args->filename
  };
  hpx_call_sync(HPX_HERE, edge_list_from_file, &edge_list_from_file_args,
		sizeof(edge_list_from_file_args), &el, sizeof(el));
  printf("Edge List: #v = %lu, #e = %lu\n",
         el.num_vertices, el.num_edges);

  // Open the results file and write the basic info out
  FILE *results_file = fopen("sample.ss.chk", "w");
  fprintf(results_file, "%s\n","p chk sp ss sssp");
  fprintf(results_file, "%s %s %s\n","f", args->filename,args->prob_file);
  fprintf(results_file, "%s %lu %lu %lu %lu\n","g", el.num_vertices, el.num_edges, 0L, 0L);
  // min and max edge weight needs to be reimplemented
  // el.min_edge_weight, el.max_edge_weight);

  call_sssp_args_t sargs;

  double total_elapsed_time = 0.0;

  size_t *edge_traversed =(size_t *) calloc(args->nproblems, sizeof(size_t));
  double *elapsed_time = (double *) calloc(args->nproblems, sizeof(double));

  if (!realloc_adj_list) {
    // Construct the graph as an adjacency list
    hpx_call_sync(HPX_HERE, adj_list_from_edge_list, &el, sizeof(el), &sargs.graph, sizeof(sargs.graph));
  }

  hpx_addr_t kind_bcast_lco = hpx_lco_future_new(0), dc_bcast_lco = hpx_lco_future_new(0);
  hpx_bcast(initialize_sssp_kind, &args->sssp_kind, sizeof(args->sssp_kind), kind_bcast_lco);
  if(args->sssp_init_dc_args.num_pq == 0) args->sssp_init_dc_args.num_pq = HPX_THREADS;
  printf("# priority  queues: %zd\n",args->sssp_init_dc_args.num_pq);
  hpx_bcast(sssp_init_dc, &args->sssp_init_dc_args, sizeof(args->sssp_init_dc_args), dc_bcast_lco);
  if(args->delta > 0) {
    hpx_addr_t delta_bcast_lco = hpx_lco_future_new(0);
    hpx_bcast(sssp_run_delta_stepping, NULL, 0, delta_bcast_lco);
    hpx_lco_wait(delta_bcast_lco);
    hpx_lco_delete(delta_bcast_lco, HPX_NULL);
    sargs.delta = args->delta;
  }
  hpx_lco_wait(kind_bcast_lco);
  hpx_lco_wait(dc_bcast_lco);
  hpx_lco_delete(kind_bcast_lco, HPX_NULL);
  hpx_lco_delete(dc_bcast_lco, HPX_NULL);

  printf("About to enter problem loop.\n");

  for (int i = 0; i < args->nproblems; ++i) {
    if(total_elapsed_time > args->time_limit) {
      printf("Time limit of %" SSSP_UINT_PRI " seconds reached. Stopping further SSSP runs.\n", args->time_limit);
      args->nproblems = i;
      break;
    }

    if (realloc_adj_list) {
      // Construct the graph as an adjacency list
      hpx_call_sync(HPX_HERE, adj_list_from_edge_list, &el, sizeof(el), &sargs.graph, sizeof(sargs.graph));
    }

    sargs.source = args->problems[i];

    hpx_time_t now = hpx_time_now();

    // Call the SSSP algorithm
    hpx_addr_t sssp_lco = hpx_lco_future_new(0);
    sargs.termination_lco = sssp_lco;
    if (sargs.delta == 0) {
      printf("Calling SSSP.\n");
      hpx_call(HPX_HERE, call_sssp, &sargs, sizeof(sargs), HPX_NULL);
    } else {
      printf("Calling delta-stepping.\n");
      hpx_call(HPX_HERE, call_delta_sssp, &sargs, sizeof(sargs), HPX_NULL);
    }
    // printf("Waiting for termination LCO at: %zu\n", sssp_lco);
    hpx_lco_wait(sssp_lco);
    // printf("Finished waiting for termination LCO at: %zu\n", sssp_lco);
    hpx_lco_delete(sssp_lco, HPX_NULL);


    double elapsed = hpx_time_elapsed_ms(now)/1e3;
    elapsed_time[i] = elapsed;
    total_elapsed_time += elapsed;

#ifdef GATHER_STAT
    _sssp_statistics *sssp_stat=(_sssp_statistics *)malloc(sizeof(_sssp_statistics));
    hpx_call_sync(sargs.sssp_stat, _print_sssp_stat,sssp_stat,sizeof(_sssp_statistics),sssp_stat,sizeof(_sssp_statistics));
    printf("\nuseful work = %lu,  useless work = %lu\n", sssp_stat->useful_work, sssp_stat->useless_work);

    total_vertex_visit += (sssp_stat->useful_work + sssp_stat->useless_work);
    total_distance_updates += sssp_stat->useful_work;
    total_edge_traversal += sssp_stat->edge_traversal_count;
#endif

#ifdef VERBOSE
    // Action to print the distances of each vertex from the source
    hpx_addr_t vertices = hpx_lco_and_new(el.num_vertices);
    for (int i = 0; i < el.num_vertices; ++i) {
      hpx_addr_t index = hpx_addr_add(sargs.graph, i * sizeof(hpx_addr_t), _index_array_block_size);
      hpx_call(index, _print_vertex_distance_index, &i, sizeof(i), vertices);
    }
    hpx_lco_wait(vertices);
    hpx_lco_delete(vertices, HPX_NULL);
#endif

    hpx_addr_t checksum_lco = HPX_NULL;
    hpx_call_sync(sargs.graph, dimacs_checksum, &el.num_vertices, sizeof(el.num_vertices),
                  &checksum_lco, sizeof(checksum_lco));
    size_t checksum = 0;
    hpx_lco_get(checksum_lco, sizeof(checksum), &checksum);
    hpx_lco_delete(checksum_lco, HPX_NULL);

    //printf("Computing GTEPS...\n");
    hpx_addr_t gteps_lco = HPX_NULL;
    hpx_call_sync(sargs.graph, gteps_calculate, &el.num_vertices, sizeof(el.num_vertices), &gteps_lco, sizeof(gteps_lco));
    size_t gteps = 0;
    hpx_lco_get(gteps_lco, sizeof(gteps), &gteps);
    hpx_lco_delete(gteps_lco, HPX_NULL);
    edge_traversed[i] = gteps;
    //printf("Gteps is %zu\n", gteps);

    printf("Finished problem %d in %.7f seconds (csum = %zu).\n", i, elapsed, checksum);
    fprintf(results_file, "d %zu\n", checksum);

    if (realloc_adj_list) {
      hpx_call_sync(sargs.graph, free_adj_list, NULL, 0, NULL, 0);
    } else {
      reset_adj_list(sargs.graph, &el);
    }
  }

  if (!realloc_adj_list) {
    hpx_call_sync(sargs.graph, free_adj_list, NULL, 0, NULL, 0);
  }

#ifdef GATHER_STAT
  double avg_time_per_source = total_elapsed_time/args->nproblems;
  double avg_vertex_visit  = total_vertex_visit/args->nproblems;
  double avg_edge_traversal = total_edge_traversal/args->nproblems;
  double avg_distance_updates = total_distance_updates/args->nproblems;

  printf("\navg_vertex_visit =  %f, avg_edge_traversal = %f, avg_distance_updates= %f\n", avg_vertex_visit, avg_edge_traversal, avg_distance_updates);

  FILE *fp;
  fp = fopen("perf.ss.res", "a+");

  fprintf(fp, "%s\n","p res sp ss sssp");
  fprintf(fp, "%s %s %s\n","f",args->filename,args->prob_file);
  fprintf(fp,"%s %lu %lu %lu %lu\n","g",el.num_vertices, el.num_edges,el.min_edge_weight, el.max_edge_weight);
  fprintf(fp,"%s %f\n","t",avg_time_per_source);
  fprintf(fp,"%s %f\n","v",avg_vertex_visit);
  fprintf(fp,"%s %f\n","e",avg_edge_traversal);
  fprintf(fp,"%s %f\n","i",avg_distance_updates);
  //fprintf(fp,"%s %s %f\n","c ", "The GTEPS measure is ",(total_edge_traversal/(total_elapsed_time*1.0E9)));

  fclose(fp);
#endif

#ifdef VERBOSE
  printf("\nElapsed time\n");
  for(int i = 0; i < args->nproblems; i++)
    printf("%f\n", elapsed_time[i]);

  printf("\nEdges traversed\n");
  for(int i = 0; i < args->nproblems; i++)
    printf("%zu\n", edge_traversed[i]);
#endif

  printf("\nTEPS statistics:\n");
  double *tm = (double*)malloc(sizeof(double)*args->nproblems);
  double *stats = (double*)malloc(sizeof(double)*9);

  for(int i = 0; i < args->nproblems; i++)
    tm[i] = edge_traversed[i]/elapsed_time[i];

  statistics (stats, tm, args->nproblems);
  PRINT_STATS("TEPS", 1);

  free(tm);
  free(stats);
  free(edge_traversed);
  free(elapsed_time);

  hpx_shutdown(HPX_SUCCESS);
  return HPX_SUCCESS;
}


int main(int argc, char *argv[argc]) {
  sssp_uint_t time_limit = 1000;
  int realloc_adj_list = 0;
  sssp_init_dc_args_t sssp_init_dc_args = { .num_pq = 0, .freq = 100, .num_elem = 100 };
  sssp_kind_t sssp_kind = DC_SSSP_KIND;
  size_t delta = 0;

  int e = hpx_init(&argc, &argv);
  if (e) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return e;
  }

  int opt = 0;
  while ((opt = getopt(argc, argv, "q:f:l:z:cdaphk?")) != -1) {
    switch (opt) {
    case 'q':
      time_limit = strtoul(optarg, NULL, 0);
      break;
    case 'a':
      realloc_adj_list = 1;
      break;
    case 'k':
      set_termination(AND_LCO_TERMINATION);
      break;
    case 'p':
      set_termination(PROCESS_TERMINATION);
      break;
    case 'd':
      sssp_kind = DC_SSSP_KIND;
      // TBD: add options to adjust dc parameters
      break;
    case 'f':
      sssp_init_dc_args.freq = strtoul(optarg,NULL,0);
      break;
    case 'l':
      sssp_init_dc_args.num_pq = strtoul(optarg,NULL,0);
      break;
    case 'c':
      sssp_kind = CHAOTIC_SSSP_KIND;
      break;
    case 'h':
      _usage(stdout);
      return 0;
    case 'z':
      delta = strtoul(optarg, NULL, 0);
      break;
    case '?':
    default:
      _usage(stderr);
      return -1;
    }
  }

  argc -= optind;
  argv += optind;

  char *graph_file;
  char *problem_file;

  switch (argc) {
   case 0:
    fprintf(stderr, "\nMissing graph (.gr) file.\n");
    _usage(stderr);
    return -1;
   case 1:
    fprintf(stderr, "\nMissing problem specification (.ss) file.\n");
    _usage(stderr);
    return -1;
   default:
    _usage(stderr);
    return -1;
   case 2:
     graph_file = argv[0];
     problem_file = argv[1];
     break;
  }

  sssp_uint_t nproblems;
  sssp_uint_t *problems;
  // Read the DIMACS problem specification file
  _read_dimacs_spec(&problem_file, &nproblems, &problems);

  _sssp_args_t args = { .filename = graph_file,
                        .nproblems = nproblems,
                        .problems = problems,
                        .prob_file = problem_file,
                        .time_limit = time_limit,
                        .realloc_adj_list = realloc_adj_list,
			.sssp_kind = sssp_kind,
			.sssp_init_dc_args = sssp_init_dc_args,
			.delta = delta
  };

  // register the actions
  HPX_REGISTER_ACTION(&_print_vertex_distance_index,
                      _print_vertex_distance_index_action);
  HPX_REGISTER_ACTION(&_print_vertex_distance, _print_vertex_distance_action);
  HPX_REGISTER_ACTION(&_main, _main_action);

  e = hpx_run(&_main, &args, sizeof(args));
  free(problems);
  return e;
}
