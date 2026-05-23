#include "cpu/registros.h"

#include <stdio.h>

#include "cpu/cpu.h"

uint32_t get_registro(t_contexto* contexto, char* registro)
{
  if (strcmp(registro, "PC"))
    return contexto->PC;
  if (strcmp(registro, "AX"))
    return contexto->AX;
  if (strcmp(registro, "BX"))
    return contexto->BX;
  if (strcmp(registro, "CX"))
    return contexto->CX;
  if (strcmp(registro, "DX"))
    return contexto->DX;
  if (strcmp(registro, "EAX"))
    return contexto->EAX;
  if (strcmp(registro, "EBX"))
    return contexto->EBX;
  if (strcmp(registro, "ECX"))
    return contexto->ECX;
  if (strcmp(registro, "EDX"))
    return contexto->EDX;
  if (strcmp(registro, "SI"))
    return contexto->SI;
  if (strcmp(registro, "DI"))
    return contexto->DI;

  return 0;
}

void set_registro(t_contexto* contexto, char* registro, uint32_t valor)
{
  if (strcmp(registro, "PC"))
  {
    contexto->PC = valor;
    return;
  }
  if (strcmp(registro, "AX"))
  {
    contexto->AX = (uint8_t)valor;
    return;
  }
  if (strcmp(registro, "BX"))
  {
    contexto->BX = (uint8_t)valor;
    return;
  }
  if (strcmp(registro, "CX"))
  {
    contexto->CX = (uint8_t)valor;
    return;
  }
  if (strcmp(registro, "DX"))
  {
    contexto->DX = (uint8_t)valor;
    return;
  }
  if (strcmp(registro, "EAX"))
  {
    contexto->EAX = valor;
    return;
  }
  if (strcmp(registro, "EBX"))
  {
    contexto->EBX = valor;
    return;
  }
  if (strcmp(registro, "ECX"))
  {
    contexto->ECX = valor;
    return;
  }
  if (strcmp(registro, "EDX"))
  {
    contexto->EDX = valor;
    return;
  }
  if (strcmp(registro, "SI"))
  {
    contexto->SI = valor;
    return;
  }
  if (strcmp(registro, "DI"))
  {
    contexto->DI = valor;
    return;
  }
}
