#include "cpu/memoria.h"

#include <stdio.h>

#include "cpu/cpu.h"
#include "cpu/liberacion.h"
#include "cpu/registros.h"

uint32_t mmu(t_cpu* cpu, t_contexto* contexto, uint32_t dir_logica,
             uint32_t tamanio, uint32_t pid)
{
  uint32_t num_segmento = dir_logica / cpu->tamanio_max_segmento;
  uint32_t desplazamiento = dir_logica % cpu->tamanio_max_segmento;

  t_segmento* segmento =
      buscar_segmento_por_id(contexto->tablaDeSegmentos, num_segmento);

  if (segmento == NULL)
  {
    log_error(cpu->logger, "## Segmento %u no encontrado", num_segmento);
    return DIR_INVALIDA - 1;
  }

  if (desplazamiento + tamanio > segmento->size)
  {
    if (seg_fault_KS(cpu, pid))
      return DIR_INVALIDA;
    else
      return DIR_INVALIDA - 1;
  }

  return segmento->base + desplazamiento;
}

t_segmento* buscar_segmento_por_id(t_list* tablaSegmentos,
                                   uint32_t num_segmento)
{
  t_segmento* segmento = NULL;
  for (int i = 0; i < list_size(tablaSegmentos); i++)
  {
    segmento = list_get(tablaSegmentos, i);
    if (segmento->id == num_segmento)
    {
      return segmento;
    }
  }
  return NULL;
}

bool seg_fault_KS(t_cpu* cpu, uint32_t pid)
{
  if (!enviar_string(OP_SEG_FAULT, "SEGMENTATION FAULT",
                     cpu->socket_kernel_scheduler))
  {
    log_error(cpu->logger, "## Fallo en el envío de segmentation fault");
    return false;
  }
  log_info(cpu->logger, "Envio correcto seg fault");
  return true;
}

t_memory_stick_info* encontrar_stick(t_cpu* cpu, uint32_t dir_fisica)
{
  for (int i = 0; i < list_size(cpu->memory_sticks); i++)
  {
    t_memory_stick_info* stick = list_get(cpu->memory_sticks, i);
    if (dir_fisica >= stick->offset &&
        dir_fisica < stick->offset + stick->tamanio)
      return stick;
  }
  return NULL;
}

void* leer_memoria(t_cpu* cpu, uint32_t dir_fisica, uint32_t tamanio)
{
  void* resultado = malloc(tamanio);
  uint32_t bytes_leidos = 0;

  while (bytes_leidos < tamanio)
  {
    t_memory_stick_info* stick =
        encontrar_stick(cpu, dir_fisica + bytes_leidos);
    if (stick == NULL)
    {
      log_error(cpu->logger, "## No se encontró el Memory Stick");
      free(resultado);
      return NULL;
    }

    uint32_t dir_en_stick = (dir_fisica + bytes_leidos) - stick->offset;
    uint32_t bytes_disponibles = stick->tamanio - dir_en_stick;
    uint32_t bytes_a_leer;

    // veo si me alcanza con el stick o si necesito otro
    if (bytes_disponibles < (tamanio - bytes_leidos))
      bytes_a_leer = bytes_disponibles;
    else
      bytes_a_leer = tamanio - bytes_leidos;

    if (!solicitar_lectura_MS(cpu, stick, dir_en_stick, bytes_a_leer))
      return NULL;

    char* lectura_parcial = confirmacion_letura_MS(cpu, stick);

    if (!lectura_parcial)
    {
      free(lectura_parcial);
      free(resultado);
      return NULL;
    }
    memcpy(resultado + bytes_leidos, lectura_parcial, bytes_a_leer);
    free(lectura_parcial);

    bytes_leidos += bytes_a_leer;
  }
  return resultado;
}

bool solicitar_lectura_MS(t_cpu* cpu, t_memory_stick_info* stick,
                          uint32_t dir_en_stick, uint32_t bytes_a_leer)
{
  t_paquete* paquete = crear_paquete(OP_MEMORY_STICK_LEER);
  agregar_a_paquete(paquete, &dir_en_stick, sizeof(uint32_t));
  agregar_a_paquete(paquete, &bytes_a_leer, sizeof(uint32_t));

  if (!enviar_paquete(paquete, stick->socket_MS))
  {
    log_error(cpu->logger, "## Error en el envio de la lectura al MS");
    return false;
  }
  log_info(cpu->logger, "Lectura solicitada correctamente al MS");
  eliminar_paquete(paquete);
  return true;
}

char* confirmacion_letura_MS(t_cpu* cpu, t_memory_stick_info* stick)
{
  int codigo_op = recibir_operacion(stick->socket_MS);
  if (codigo_op == OP_MEMORY_STICK_LEIDO)
  {
    log_info(cpu->logger, "Lectura realizada");
    return recibir_string(stick->socket_MS);
  }
  else
  {
    log_error(cpu->logger,
              "## No se recibi la respuesta de lectura correctamente");
    return NULL;
  }
}

bool escribir_memoria(t_cpu* cpu, uint32_t dir_fisica, void* datos_a_escribir,
                      uint32_t tamanio)
{
  uint32_t bytes_escritos = 0;

  while (bytes_escritos < tamanio)
  {
    t_memory_stick_info* stick =
        encontrar_stick(cpu, dir_fisica + bytes_escritos);
    if (stick == NULL)
    {
      log_error(cpu->logger, "## No se encontró el Memory Stick");
      return false;
    }

    uint32_t dir_en_stick = (dir_fisica + bytes_escritos) - stick->offset;
    uint32_t bytes_disponibles = stick->tamanio - dir_en_stick;
    uint32_t bytes_a_escribir;

    if (bytes_disponibles < (tamanio - bytes_escritos))
      bytes_a_escribir = bytes_disponibles;
    else
      bytes_a_escribir = tamanio - bytes_escritos;

    if (!solicitar_escritura_MS(cpu, stick, dir_en_stick,
                                datos_a_escribir + bytes_escritos,
                                bytes_a_escribir))
      return false;

    if (!confirmacion_escritura_MS(cpu, stick))
      return false;

    bytes_escritos += bytes_a_escribir;
  }
  return true;
}

bool solicitar_escritura_MS(t_cpu* cpu, t_memory_stick_info* stick,
                            uint32_t dir_en_stick, void* datos,
                            uint32_t bytes_a_escribir)
{
  t_paquete* paquete = crear_paquete(OP_MEMORY_STICK_ESCRIBIR);
  agregar_a_paquete(paquete, &dir_en_stick, sizeof(uint32_t));
  agregar_a_paquete(paquete, datos, bytes_a_escribir);
  agregar_a_paquete(paquete, &bytes_a_escribir, sizeof(uint32_t));
  if (!enviar_paquete(paquete, stick->socket_MS))
  {
    log_error(cpu->logger, "## Error en el envio de la escritura al MS");
    eliminar_paquete(paquete);
    return false;
  }
  log_info(cpu->logger, "Escritura solicitada correctamente al MS");
  eliminar_paquete(paquete);
  return true;
}

bool confirmacion_escritura_MS(t_cpu* cpu, t_memory_stick_info* stick)
{
  int codigo_op = recibir_operacion(stick->socket_MS);
  free(recibir_string(stick->socket_MS));
  if (codigo_op == OP_MEMORY_STICK_ESCRITO)
  {
    log_info(cpu->logger, "Escritura realizada");
    return true;
  }
  log_error(cpu->logger,
            "No se recibió la respuesta de escritura correctamente");
  return false;
}