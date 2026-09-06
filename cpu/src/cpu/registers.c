#include "cpu/registers.h"

#include <stdio.h>
#include <string.h>

#include "cpu/cpu.h"

uint32_t get_register(t_registers* registers, char* register_name)
{
  if (strcmp(register_name, "PC") == 0)
    return registers->PC;
  if (strcmp(register_name, "AX") == 0)
    return registers->AX;
  if (strcmp(register_name, "BX") == 0)
    return registers->BX;
  if (strcmp(register_name, "CX") == 0)
    return registers->CX;
  if (strcmp(register_name, "DX") == 0)
    return registers->DX;
  if (strcmp(register_name, "EAX") == 0)
    return registers->EAX;
  if (strcmp(register_name, "EBX") == 0)
    return registers->EBX;
  if (strcmp(register_name, "ECX") == 0)
    return registers->ECX;
  if (strcmp(register_name, "EDX") == 0)
    return registers->EDX;
  if (strcmp(register_name, "SI") == 0)
    return registers->SI;
  if (strcmp(register_name, "DI") == 0)
    return registers->DI;

  return 0;
}

void set_register(t_registers* registers, char* register_name, uint32_t value)
{
  if (strcmp(register_name, "PC") == 0)
  {
    registers->PC = value;
    return;
  }
  if (strcmp(register_name, "AX") == 0)
  {
    registers->AX = (uint8_t)value;
    return;
  }
  if (strcmp(register_name, "BX") == 0)
  {
    registers->BX = (uint8_t)value;
    return;
  }
  if (strcmp(register_name, "CX") == 0)
  {
    registers->CX = (uint8_t)value;
    return;
  }
  if (strcmp(register_name, "DX") == 0)
  {
    registers->DX = (uint8_t)value;
    return;
  }
  if (strcmp(register_name, "EAX") == 0)
  {
    registers->EAX = value;
    return;
  }
  if (strcmp(register_name, "EBX") == 0)
  {
    registers->EBX = value;
    return;
  }
  if (strcmp(register_name, "ECX") == 0)
  {
    registers->ECX = value;
    return;
  }
  if (strcmp(register_name, "EDX") == 0)
  {
    registers->EDX = value;
    return;
  }
  if (strcmp(register_name, "SI") == 0)
  {
    registers->SI = value;
    return;
  }
  if (strcmp(register_name, "DI") == 0)
  {
    registers->DI = value;
    return;
  }
}
