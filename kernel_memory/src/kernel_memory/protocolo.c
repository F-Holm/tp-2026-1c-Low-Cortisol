#include "kernel_memory/protocolo.h"

void aniadir_lista_mtx(t_list* lista, pthread_mutex_t* mutex, void* elemento)
{
  pthread_mutex_lock(mutex);
  list_add(lista, elemento);
  pthread_mutex_unlock(mutex);
}

t_list* copiar_lista_mtx(pthread_mutex_t* mutex, t_list* lista)
{
  pthread_mutex_lock(mutex);
  t_list* copia = list_duplicate(lista);
  pthread_mutex_unlock(mutex);
  return copia;
}

bool recibir_id_cpu(t_datos_cpu* datos_cpu)
{
  if (recibir_operacion(datos_cpu->socket_cpu) == OP_ID_CPU)
  {
    char* id_cpu = recibir_string(datos_cpu->socket_cpu);
    logger_info(datos_cpu->logger, "## CPU %s Conectada", id_cpu);
    datos_cpu->id = atoi(id_cpu);
    free(id_cpu);
    return true;
  }
  else
  {
    logger_info(datos_cpu->logger,
                "No se pudo realizar la conexion con el CPU ya que no se "
                "envio la operacion de ID");
    terminar_comunicacion(datos_cpu->socket_cpu);
    return false;
  }
  return false;
}

bool recibir_tamanio_stick(t_datos_stick* datos_stick)
{
  if (recibir_operacion(datos_stick->socket_stick) == OP_TAMANIO_MEMORIA)
  {
    char* tamanio = recibir_string(datos_stick->socket_stick);
    logger_info(datos_stick->logger, "## Memory Stick de %s bytes Conectada",
                tamanio);
    datos_stick->tamanio_stick = atoi(tamanio);
    free(tamanio);
    return true;
  }
  else
  {
    logger_info(datos_stick->logger,
                "No se pudo realizar la conexion con la stick ya que no se "
                "envio la operacion de tamaño");
    terminar_comunicacion(datos_stick->socket_stick);
    return false;
  }
  return false;
}

bool recibir_puerto_escucha_stick(t_datos_stick* datos_stick)
{
  if (recibir_operacion(datos_stick->socket_stick) == OP_PUERTO)
  {
    char* puerto = recibir_string(datos_stick->socket_stick);
    logger_info(datos_stick->logger, "## Puerto de Memory Stick recibido %s",
                puerto);
    datos_stick->puerto_stick = atoi(puerto);
    free(puerto);
    return true;
  }
  else
  {
    logger_info(datos_stick->logger,
                "No se pudo realizar la conexion con la stick ya que no se "
                "envio la operacion puerto");
    terminar_comunicacion(datos_stick->socket_stick);
    return false;
  }
  return false;
}

void agregar_conexion_stick(t_datos_kernel_mem* datos_kernel_memory,
                            t_datos_stick* datos_stick)
{
  aniadir_lista_mtx(datos_kernel_memory->sticks_conectados,
                    datos_kernel_memory->mutex_lista_sockets, datos_stick);
  return;
}

void agregar_conexion_cpu(t_datos_kernel_mem* datos_kernel_memory,
                          t_datos_cpu* datos_cpu)
{
  aniadir_lista_mtx(datos_kernel_memory->cpus_conectados,
                    datos_kernel_memory->mutex_lista_sockets, datos_cpu);
  return;
}

void enviar_sticks_conectadas(t_list* sticks_conectados,
                              pthread_mutex_t* mutex_lista_sockets,
                              t_datos_cpu* datos_cpu)
{
  t_list* copia_sticks =
      copiar_lista_mtx(mutex_lista_sockets, sticks_conectados);

  int total_sticks = list_size(copia_sticks);

  for (int i = 0; i < total_sticks; i++)
  {
    t_paquete* paquete = crear_paquete(OP_PAQUETE);
    t_datos_stick* stick_actual = (t_datos_stick*)list_get(copia_sticks, i);

    char puerto[6];
    snprintf(puerto, sizeof(puerto), "%u", stick_actual->puerto_stick);

    agregar_a_paquete(paquete, stick_actual->ip_memory_stick, sizeof(char[16]));
    agregar_a_paquete(paquete, puerto, sizeof(puerto));

    enviar_paquete(paquete, datos_cpu->socket_cpu);
    eliminar_paquete(paquete);
  }
  list_destroy(copia_sticks);
}

void enviar_conexion_cpu(t_datos_stick* datos_stick, t_list* cpus_conectados)
{
  if (list_is_empty(cpus_conectados))
  {
    return;
  }
  t_paquete* paquete = crear_paquete(OP_PAQUETE);

  agregar_a_paquete(paquete, datos_stick->ip_memory_stick, sizeof(char[16]));

  char puerto[6];
  snprintf(puerto, sizeof(puerto), "%u", datos_stick->puerto_stick);
  agregar_a_paquete(paquete, puerto, sizeof(puerto));

  for (int i = 0; i < list_size(cpus_conectados); i++)
  {
    t_datos_cpu* cpu_actual = (t_datos_cpu*)list_get(cpus_conectados, i);
    enviar_paquete(paquete, cpu_actual->socket_cpu);
  }
  eliminar_paquete(paquete);
}

int calcular_memoria_total(t_list* sticks_conectados,
                                 pthread_mutex_t* mutex_lista_sockets)
{
  int total = 0;
  for (int i = 0; i < list_size(sticks_conectados); i++)
  {
    pthread_mutex_lock(mutex_lista_sockets);
    t_datos_stick* stick_actual =
        (t_datos_stick*)list_get(sticks_conectados, i);
    pthread_mutex_unlock(mutex_lista_sockets);
    total += stick_actual->tamanio_stick;
  }
  return total;
}

int calcular_espacio_libre(t_list* huecos, pthread_mutex_t* mutex_huecos)
{
  int total = 0;
  for (int i = 0; i < list_size(huecos); i++)
  {
    pthread_mutex_lock(mutex_huecos);
    t_hueco* hueco_actual = (t_hueco*)list_get(huecos, i);
    pthread_mutex_unlock(mutex_huecos);
    total += hueco_actual->size;
  }
  return total;
}

t_proceso* buscar_proceso(t_datos_cpu* datos_cpu, uint32_t pid)
{
  t_proceso* resultado = NULL;
  pthread_mutex_lock(datos_cpu->mutex_procesos);
  for (int i = 0; i < list_size(datos_cpu->procesos); i++)
  {
    t_proceso* proceso = list_get(datos_cpu->procesos, i);
    if (proceso->pid == pid)
      resultado = proceso;
  }
  pthread_mutex_unlock(datos_cpu->mutex_procesos);
  return resultado;
}

t_memoria_principal* aniadir_memoria_total(t_memoria_principal* memoria_principal, int memoria_total)
{
  pthread_mutex_lock(memoria_principal->mutex_memoria_principal);
  memoria_principal->tamanio_total += memoria_total;
  pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
  return memoria_principal;
}


bool compactar_memoria(int socket_scheduler,t_memoria_principal* memoria_principal)
{
  if (notificar_compactacion(socket_scheduler))
  {
    pthread_mutex_lock(memoria_principal->mutex_memoria_principal);
    memoria_principal->segmentos = compactar_segmentos(memoria_principal->segmentos);
    memoria_principal->huecos = compactar_huecos(memoria_principal->tamanio_total, calcular_base_final_segmento(memoria_principal->segmentos));
    pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
    return true;
}
  return false;
}

t_list* compactar_segmentos(t_list* segmentos)
{
  for (int i = 0; i < list_size(segmentos) - 1; i++)
  {
    t_segmento* segmento_actual = list_get(segmentos, i);
    t_segmento* siguiente_segmento = list_get(segmentos, i + 1);
    if (i == 0){
      segmento_actual->base = 0;
    }
    siguiente_segmento->base = segmento_actual->base + segmento_actual->size;
  }
}

int calcular_base_final_segmento(t_list* segmentos)
{
  t_segmento* ultimo_segmento = list_get(segmentos, list_size(segmentos) - 1);
  return ultimo_segmento->base + ultimo_segmento->size;
}

t_list* compactar_huecos( int memoria_total, int base_final_segmento)
{  
  t_list* huecos = list_create();
  t_hueco* hueco_final = malloc(sizeof(t_hueco));
  hueco_final->base = base_final_segmento;
  hueco_final->size = memoria_total - base_final_segmento;
  list_add(huecos, hueco_final);
}


bool notificar_compactacion(int socket_scheduler)
{
  enviar_string(OP_COMPACTACION_NECESARIA, "Es necesario compactar la memoria", socket_scheduler);
  if(recibir_operacion(socket_scheduler) == OP_PUEDE_COMPACTAR)
  {
    char* mensaje = recibir_string(socket_scheduler);
    free(mensaje);
    return true;
  }
  return false;
}


t_segmento* buecar_y_eliminar_segmento(uint32_t id, uint32_t pid, t_memoria_principal* memoria_principal){
  t_segmento* segmento = NULL; 
  pthread_mutex_lock(memoria_principal->mutex_memoria_principal);
  for (int i = 0; i < list_size(memoria_principal->segmentos); i++)
  {
    t_segmento* segmento_actual = list_get(memoria_principal->segmentos, i);
    if (segmento_actual->id == id && segmento_actual->pid == pid)
    {
      segmento = segmento_actual;
      list_remove(memoria_principal->segmentos, i);
      break;
    }
  }
  pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
  return segmento;
}

void es_hueco_anterior(t_hueco* hueco_aux,t_hueco* hueco_actual, t_segmento* segmento_aux, t_memoria_principal* memoria_principal,int indice){
if(hueco_actual->base + hueco_actual->size == segmento_aux->base){
              hueco_aux ->base= hueco_actual->base;
              hueco_aux ->size = hueco_actual->size + segmento_aux->size;
              list_remove_and_destroy_element(memoria_principal->huecos,indice,free);
              return hueco_aux;
}
}

void es_hueco_posterior(t_hueco* hueco_aux,t_hueco* hueco_actual, t_segmento* segmento_aux, t_memoria_principal* memoria_principal,int indice){
  if(hueco_actual->base == segmento_aux->base + segmento_aux->size){            
      hueco_aux->size += hueco_actual->size;
      list_remove_and_destroy_element(memoria_principal->huecos, indice ,free);
}
}


void eliminar_segmento(uint32_t id, uint32_t pid, t_memoria_principal* memoria_principal)
{
  t_hueco* nuevo_hueco = malloc(sizeof(t_hueco));
  nuevo_hueco->base =0;
  nuevo_hueco->size=0;
  t_segmento* segmento_aux = buscar_y_eliminar_segmento(id,pid, memoria_principal);  
  pthread_mutex_lock(memoria_principal->mutex_memoria_principal);
  if(segmento_aux == NULL)
  {//logger_error()
  }
  if(hueco_antes_segmento(segmento_aux->base, segmento_aux->base + segmento_aux->size, memoria_principal->huecos) && hueco_despues_segmento(segmento_aux->base, segmento_aux->base + segmento_aux->size, memoria_principal->huecos)){
          // SEGMENTO EN MEDIO DE HUECOS 
          for(int i = 0 ; i < list_size(memoria_principal->huecos); i++){
            t_hueco* hueco_actual = list_get(memoria_principal->huecos, i);
            es_hueco_anterior(nuevo_hueco, hueco_actual, segmento_aux, memoria_principal, i);
            es_hueco_posterior(nuevo_hueco, hueco_actual, segmento_aux, memoria_principal, i);
          }
          memoria_principal->huecos= list_add(memoria_principal->huecos, nuevo_hueco);
    }else if(hueco_antes_segmento(segmento_aux->base, segmento_aux->base + segmento_aux->size, memoria_principal->huecos)){
      //SEGMENTO DESPUES DE HUECO
         for(int i = 0 ; i < list_size(memoria_principal->huecos); i++){
            t_hueco* hueco_actual = list_get(memoria_principal->huecos, i);
            es_hueco_anterior(nuevo_hueco, hueco_actual, segmento_aux, memoria_principal, i);
            }

      }else if(hueco_despues_segmento(segmento_aux->base, segmento_aux->base + segmento_aux->size, memoria_principal->huecos)){
        // SEGMENTO ANTES DE HUECO
        for(int i = 0 ; i < list_size(memoria_principal->huecos); i++){
        t_hueco* hueco_actual = list_get(memoria_principal->huecos, i);
        es_hueco_posterior(nuevo_hueco, hueco_actual, segmento_aux, memoria_principal, i);
        }
      }else{
      nuevo_hueco->base = segmento_aux->base;
      nuevo_hueco->size = segmento_aux->size;
      memoria_principal->huecos = list_add(memoria_principal->huecos, nuevo_hueco);
      free(segmento_aux);
    }
  pthread_mutex_unlock(memoria_principal->mutex_memoria_principal);
  free(nuevo_hueco);
}

bool hueco_antes_segmento(int base_segmento, int final_segmento, t_list* huecos){
for (int i = 0; i < list_size(huecos); i++)
  {
    t_hueco* hueco_actual = list_get(huecos, i);
    if ( (hueco_actual->base + hueco_actual->size) == base_segmento ){
      return true;
    }
  }
  return false;
} 

bool hueco_despues_segmento(int base_segmento, int final_segmento, t_list* huecos){
  for (int i = 0; i < list_size(huecos); i++)  
  {
    t_hueco* hueco_actual = list_get(huecos, i);
    if ( hueco_actual->base == final_segmento ){
      return true;
    }
  }
  return false;
}