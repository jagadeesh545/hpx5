#include <string.h>
#include <stdio.h>

#include "hpx.h"

struct pingpong_args {
  int ping_id;
  int pong_id;
  char msg[128];
};

#define BUFFER_SIZE 128

int global_count = 0;
hpx_locality_t* my_loc;
hpx_locality_t* other_loc;
hpx_future_t done_fut;
hpx_action_t* a_done;

int opt_iter_limit = 1000;
int opt_text_ping = 0;
int opt_screen_out = 0;

void _done_action(void* _args) {
  hpx_lco_future_set(&done_fut, 0);
}

void _ping_action(void* _args) {
  int success;
  hpx_parcel_t* p;
  struct pingpong_args* in_args = (struct pingpong_args*)_args;
  int ping_id = in_args->pong_id + 1;
  struct pingpong_args* out_args;

  if (opt_screen_out)
    printf("Ping acting; global_count=%d, message=%s\n", global_count, out_args->msg);

  //  if (global_count >= opt_iter_limit) {
  if (ping_id >= opt_iter_limit) {
    p = hpx_alloc(sizeof(hpx_parcel_t));
    success = hpx_new_parcel("_done_action", (void*)NULL, 0, p);
    success = hpx_send_parcel(other_loc, p);
    hpx_action_invoke(a_done, NULL, (void**)NULL);
  }
  else {
    out_args = hpx_alloc(sizeof(struct pingpong_args));
    if (out_args == NULL) {
      printf("Dieing horribly - no memory!\n");
      exit(-1);
    }
    out_args->ping_id = ping_id;
    if (opt_text_ping)
      snprintf(out_args->msg, 128, "Message %d from proc 0", ping_id);
  
    p = hpx_alloc(sizeof(hpx_parcel_t));
    success = hpx_new_parcel("_pong_action", (void*)out_args, sizeof(struct pingpong_args), p);
    success = hpx_send_parcel(other_loc, p);
  }
  
  hpx_free(in_args);
  global_count++;
}

void _pong_action(void* _args) {
  int success;
  int str_length;
  char copy_buffer[BUFFER_SIZE];
  hpx_parcel_t* p;
  struct pingpong_args* out_args;
  struct pingpong_args* in_args = (struct pingpong_args*)_args;
  int ping_id = in_args->ping_id;
  int pong_id = ping_id;

  out_args = hpx_alloc(sizeof(struct pingpong_args));
  if (out_args == NULL) {
    printf("Dieing horribly!\n");
    exit(-1);
  }
  out_args->pong_id = pong_id;

  if (opt_text_ping) {
    snprintf(copy_buffer, 50, "At %d, received from proc 0 message: '", pong_id);
    str_length = strlen(copy_buffer);
    strcpy(&copy_buffer[str_length], in_args->msg);
    str_length = strlen(copy_buffer);
    strcpy(&copy_buffer[str_length], "'");
    strcpy(out_args->msg, copy_buffer);
  }

  if (opt_screen_out)
    printf("Pong acting; global_count=%d, message=%s\n", global_count, out_args->msg);


  //  if (global_count >= opt_iter_limit - 1) {
  if (ping_id >= opt_iter_limit) {
  }
  else {
    out_args = hpx_alloc(sizeof(struct pingpong_args));
    if (out_args == NULL) {
      printf("Dieing horribly!\n");
      exit(-1);
    }
    out_args->pong_id = pong_id;
    
    if (opt_text_ping) {
      snprintf(copy_buffer, 50, "At %d, received from proc 0 message: '", pong_id);
      str_length = strlen(copy_buffer);
      strcpy(&copy_buffer[str_length], in_args->msg);
      str_length = strlen(copy_buffer);
      strcpy(&copy_buffer[str_length], "'");
      strcpy(out_args->msg, copy_buffer);
    }
    
    if (opt_screen_out)
      printf("Pong acting; global_count=%d, message=%s\n", global_count, out_args->msg);
    
    p = hpx_alloc(sizeof(hpx_parcel_t));
    success = hpx_new_parcel("_ping_action", (void*)out_args, sizeof(struct pingpong_args), p);
    success = hpx_send_parcel(other_loc, p);
  }

  global_count++;
}

void pingpong(void* _args) {
  unsigned int num_ranks;
  unsigned int my_rank;
  hpx_parcel_t* p;
  struct pingpong_args* args;
  hpx_action_t* a_ping;
  hpx_action_t* a_pong;
  int* result;

  num_ranks = hpx_get_num_localities();
  my_loc = hpx_get_my_locality();
  my_rank = my_loc->rank;

  /* register action for parcel (must be done by all ranks) */
  a_ping = hpx_alloc(sizeof(hpx_action_t));
  hpx_action_register("_ping_action", (hpx_func_t)_ping_action, a_ping);
  a_pong = hpx_alloc(sizeof(hpx_action_t));
  hpx_action_register("_pong_action", (hpx_func_t)_pong_action, a_pong);
  a_done = hpx_alloc(sizeof(hpx_action_t));
  hpx_action_register("_done_action", (hpx_func_t)_done_action, a_done);

  if (my_rank == 0)
    other_loc = hpx_get_locality(1);
  else if (my_rank == 1)
    other_loc = hpx_get_locality(0);
  else 
    {}

  hpx_lco_future_init(&done_fut, 1);

  if (my_rank == 0) {
    args = hpx_alloc(sizeof(struct pingpong_args));
    args->pong_id = -1;
    //    p = hpx_alloc(sizeof(hpx_parcel_t));
    //    success = hpx_new_parcel("_ping_action", (void*)args, sizeof(struct pingpong_args), p);
    //    success = hpx_send_parcel(other_loc, p);
    result = hpx_alloc(sizeof(int));
    hpx_action_invoke(a_ping, args, (void**)&result);
  }
  else if (my_rank ==1) {
  }
  else
    {}

  hpx_thread_wait(&done_fut);

  if (my_rank == 0)
    free(result);
  hpx_locality_destroy(other_loc);
  free(a_ping);
  free(a_pong);
  free(a_done);

  return;
}

int main(int argc, char** argv) {
  int success;
  struct timespec begin_ts;
  struct timespec end_ts;
  unsigned long elapsed;
  double avg_oneway_latency;
  hpx_thread_t* th;

  if (argc > 1)
    opt_iter_limit = atoi(argv[1]);
  if (opt_iter_limit < 0) {
    printf("Bad iteration limit\n");
    exit(-1);
  }
  if (argc > 2)
    opt_text_ping = atoi(argv[2]);
  if (argc > 3)
    opt_screen_out = atoi(argv[3]);
  printf("Running with options: {iter limit: %d}, {text_ping: %d}, {screen_out: %d}\n", opt_iter_limit, opt_text_ping, opt_screen_out);

  success = hpx_init();
  if (success != 0)
    exit(-1);

  clock_gettime(CLOCK_MONOTONIC, &begin_ts);
  th = hpx_thread_create(__hpx_global_ctx, 0, (hpx_func_t)pingpong, 0);
  hpx_thread_join(th, NULL);
  clock_gettime(CLOCK_MONOTONIC, &end_ts);
  elapsed = ((end_ts.tv_sec * 1000000000) + end_ts.tv_nsec) - ((begin_ts.tv_sec * 1000000000) + begin_ts.tv_nsec);
  avg_oneway_latency = elapsed/((double)(opt_iter_limit*2));
  printf("average oneway latency (MPI):   %f ms\n", avg_oneway_latency/1000000.0);
  
  hpx_cleanup();

  return 0;
}
