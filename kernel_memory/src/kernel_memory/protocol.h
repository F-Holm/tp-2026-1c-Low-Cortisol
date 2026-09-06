#pragma once

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "kernel_memory/configurator.h"
#include "kernel_memory/structs.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

bool receive_cpu_id(t_cpu_data* cpu_data);
bool receive_stick_size(t_stick_data* stick_data);
bool receive_stick_listen_port(t_stick_data* stick_data);
void add_stick_connection(t_kernel_memory_data* kernel_data,
                          t_stick_data* stick_data);
void send_connected_sticks(t_list* connected_sticks,
                           pthread_mutex_t* socket_list_mutex,
                           t_cpu_data* cpu_data);
void send_cpu_connection(t_stick_data* stick_data, t_list* connected_cpus);
void add_cpu_connection(t_kernel_memory_data* kernel_data,
                        t_cpu_data* cpu_data);
void list_add_mtx(t_list* list, pthread_mutex_t* mutex, void* element);
t_process* find_process(t_list* process_list, pthread_mutex_t* processes_mutex,
                        uint32_t pid);
int compute_total_memory(t_list* connected_sticks,
                         pthread_mutex_t* socket_list_mutex);
int compute_free_space(t_list* holes, pthread_mutex_t* holes_mutex,
                       t_log* logger);
int compute_last_segment_end(t_list* segments);
t_list* filter_process_segments(int pid, t_main_memory* main_memory,
                                t_log* logger);
void add_segments_to_packet(t_list* segments, t_packet* process_segment_table);
t_main_memory* add_total_memory(t_main_memory* main_memory, int memory_total);
bool compact_memory(int socket_scheduler, t_main_memory* main_memory);
t_list* compact_holes(int memory_total, int base_final_segment);
void compact_segments(t_list* segments);
void notify_compaction(int socket_scheduler);
int compute_last_segment_end(t_list* segments);
t_segment* find_and_remove_segment(uint32_t id, uint32_t pid,
                                   t_main_memory* main_memory, t_log* logger);
bool hole_after_segment(int base_segment, int final_segment, t_list* holes);
bool hole_before_segment(int base_segment, int final_segment, t_list* holes);
void remove_segment(uint32_t id, uint32_t pid, t_main_memory* main_memory,
                    t_log* logger);
void create_segment(uint32_t id, uint32_t pid, int size,
                    t_main_memory* main_memory, int socket_scheduler,
                    t_log* logger);
int translate_logical_address(uint32_t pid, uint32_t logical_address,
                              uint32_t size, t_main_memory* main_memory,
                              t_log* logger);
char* read_from_sticks(int physical_address, int size, t_list* connected_sticks,
                       pthread_mutex_t* sticks_mutex, t_log* logger,
                       int socket_scheduler);
int find_stick(int physical_address, t_list* connected_sticks,
               pthread_mutex_t* sticks_mutex, int* stick_offset);
int compute_process_size(t_process* process, t_main_memory* main_memory);
bool write_to_sticks(int pid, int physical_address, int bytes_to_read,
                     char* write_string, t_list* connected_sticks,
                     pthread_mutex_t* socket_list_mutex, t_log* logger,
                     int socket_scheduler);
t_segment* find_segment(t_main_memory* main_memory, uint32_t pid,
                        uint32_t segment_number);
t_hole select_hole(uint32_t size, t_log* logger, t_main_memory* memory);
void update_segment_list(t_main_memory* main_memory, t_hole chosen_hole,
                         int size, uint32_t pid, uint32_t id);
