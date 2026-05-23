#include "cpu/registros.h"

#include <stdio.h>

#include "cpu/cpu.h"

uint32_t get_registro(t_contexto* contexto, char* registro)
{
  if (strcmp(registro, "PC") == 0)
    return contexto->PC;
  if (strcmp(registro, "AX") == 0)
    return contexto->AX;
  if (strcmp(registro, "BX") == 0)
    return contexto->BX;
  if (strcmp(registro, "CX") == 0)
    return contexto->CX;
  if (strcmp(registro, "DX") == 0)
    return contexto->DX;
  if (strcmp(registro, "EAX") == 0)
    return contexto->EAX;
  if (strcmp(registro, "EBX") == 0)
    return contexto->EBX;
  if (strcmp(registro, "ECX") == 0)
    return contexto->ECX;
  if (strcmp(registro, "EDX") == 0)
    return contexto->EDX;
  if (strcmp(registro, "SI") == 0)
    return contexto->SI;
  if (strcmp(registro, "DI") == 0)
    return contexto->DI;

  return 0;
}

void set_registro(t_contexto* contexto, char* registro, uint32_t valor)
{
  if (strcmp(registro, "PC") == 0)
  {
    contexto->PC = valor;
    return;
  }
  if (strcmp(registro, "AX") == 0)
  {
    contexto->AX = (uint8_t)valor;
    return;
  }
  if (strcmp(registro, "BX") == 0)
  {
    contexto->BX = (uint8_t)valor;
    return;
  }
  if (strcmp(registro, "CX") == 0)
  {
    contexto->CX = (uint8_t)valor;
    return;
  }
  if (strcmp(registro, "DX") == 0)
  {
    contexto->DX = (uint8_t)valor;
    return;
  }
  if (strcmp(registro, "EAX") == 0)
  {
    contexto->EAX = valor;
    return;
  }
  if (strcmp(registro, "EBX") == 0)
  {
    contexto->EBX = valor;
    return;
  }
  if (strcmp(registro, "ECX") == 0)
  {
    contexto->ECX = valor;
    return;
  }
  if (strcmp(registro, "EDX") == 0)
  {
    contexto->EDX = valor;
    return;
  }
  if (strcmp(registro, "SI") == 0)
  {
    contexto->SI = valor;
    return;
  }
  if (strcmp(registro, "DI") == 0)
  {
    contexto->DI = valor;
    return;
  }
}
