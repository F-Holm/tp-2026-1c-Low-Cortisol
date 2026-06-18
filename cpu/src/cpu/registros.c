#include "cpu/registros.h"

#include <stdio.h>

#include "cpu/cpu.h"

uint32_t get_registro(t_registros* registros, char* registro)
{
  if (strcmp(registro, "PC") == 0)
    return registros->PC;
  if (strcmp(registro, "AX") == 0)
    return registros->AX;
  if (strcmp(registro, "BX") == 0)
    return registros->BX;
  if (strcmp(registro, "CX") == 0)
    return registros->CX;
  if (strcmp(registro, "DX") == 0)
    return registros->DX;
  if (strcmp(registro, "EAX") == 0)
    return registros->EAX;
  if (strcmp(registro, "EBX") == 0)
    return registros->EBX;
  if (strcmp(registro, "ECX") == 0)
    return registros->ECX;
  if (strcmp(registro, "EDX") == 0)
    return registros->EDX;
  if (strcmp(registro, "SI") == 0)
    return registros->SI;
  if (strcmp(registro, "DI") == 0)
    return registros->DI;

  return 0;
}

void set_registro(t_registros* registros, char* registro, uint32_t valor)
{
  if (strcmp(registro, "PC") == 0)
  {
    registros->PC = valor;
    return;
  }
  if (strcmp(registro, "AX") == 0)
  {
    registros->AX = (uint8_t)valor;
    return;
  }
  if (strcmp(registro, "BX") == 0)
  {
    registros->BX = (uint8_t)valor;
    return;
  }
  if (strcmp(registro, "CX") == 0)
  {
    registros->CX = (uint8_t)valor;
    return;
  }
  if (strcmp(registro, "DX") == 0)
  {
    registros->DX = (uint8_t)valor;
    return;
  }
  if (strcmp(registro, "EAX") == 0)
  {
    registros->EAX = valor;
    return;
  }
  if (strcmp(registro, "EBX") == 0)
  {
    registros->EBX = valor;
    return;
  }
  if (strcmp(registro, "ECX") == 0)
  {
    registros->ECX = valor;
    return;
  }
  if (strcmp(registro, "EDX") == 0)
  {
    registros->EDX = valor;
    return;
  }
  if (strcmp(registro, "SI") == 0)
  {
    registros->SI = valor;
    return;
  }
  if (strcmp(registro, "DI") == 0)
  {
    registros->DI = valor;
    return;
  }
}
