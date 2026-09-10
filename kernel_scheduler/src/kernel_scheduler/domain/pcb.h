#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#include "utils/collections/list.h"

typedef enum
{
  EST_NEW,
  EST_READY,
  EST_EXEC,
  EST_BLOCK,
  EST_SUSP_BLOCK,
  EST_SUSP_READY,
  EST_EXIT
} t_process_state;

extern const char* const STATE_NAMES[7];

typedef struct
{
  uint32_t pid;
  int priority;
  t_list* priority_list;
  pthread_mutex_t priority_mutex;
  unsigned long blocked_time;
  int state;
  pthread_mutex_t state_mutex;
  int active_instances;
  pthread_mutex_t active_instances_mutex;
  pthread_cond_t no_active_instances;
  void* blocking_mutex;
} t_pcb;

t_pcb* create_pcb(int state, int priority);
void destroy_pcb(t_pcb* pcb);

int get_state_pcb(t_pcb* pcb);
int get_priority_pcb(t_pcb* pcb);

// returns the index of the inserted element
int insert_pcb_in_orden(t_list* list, t_pcb* pcb);

void incrementar_instances_active_pcb(t_pcb* pcb);
void disminuir_instances_active_pcb(t_pcb* pcb);
void wait_0_instances_active_pcb(t_pcb* pcb);

void set_mutex_blocking(t_pcb* pcb, void* mutex);
void* get_mutex_blocking(t_pcb* pcb);
