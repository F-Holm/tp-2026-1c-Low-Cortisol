#pragma once

#include <stdio.h>

#include "cpu/cpu.h"

#define INVALID_ADDRESS UINT32_MAX

uint32_t mmu(t_cpu* cpu, t_context* context, uint32_t logical_address,
             uint32_t size, uint32_t pid);
t_segmento* find_segment_by_id(t_list* segment_table, uint32_t segment_number);
bool notify_seg_fault(t_cpu* cpu, uint32_t pid);
t_memory_stick_info* find_stick(t_cpu* cpu, uint32_t physical_address);
void* read_memory(t_cpu* cpu, uint32_t physical_address, uint32_t size);
bool request_read(t_cpu* cpu, t_memory_stick_info* stick,
                  uint32_t address_in_stick, uint32_t bytes_to_read);
char* receive_read_response(t_cpu* cpu, t_memory_stick_info* stick);
bool write_memory(t_cpu* cpu, uint32_t physical_address, void* data_to_write,
                  uint32_t size);
bool request_write(t_cpu* cpu, t_memory_stick_info* stick,
                   uint32_t address_in_stick, void* data,
                   uint32_t bytes_to_write);
bool receive_write_response(t_cpu* cpu, t_memory_stick_info* stick);
