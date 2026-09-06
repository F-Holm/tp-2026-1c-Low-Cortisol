#pragma once

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "kernel_memory/configurador.h"
#include "kernel_memory/estructuras.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

bool recibir_id_cpu(t_datos_cpu* datos_cpu);
bool recibir_tamanio_stick(t_datos_stick* datos_stick);
bool recibir_puerto_escucha_stick(t_datos_stick* datos_stick);
void agregar_conexion_stick(t_datos_kernel_mem* datos_kernel_memory,
                            t_datos_stick* datos_stick);
void enviar_sticks_conectadas(t_list* sticks_conectados,
                              pthread_mutex_t* mutex_lista_sockets,
                              t_datos_cpu* datos_cpu);
void enviar_conexion_cpu(t_datos_stick* datos_stick, t_list* cpus_conectados);
void agregar_conexion_cpu(t_datos_kernel_mem* datos_kernel_memory,
                          t_datos_cpu* datos_cpu);
void aniadir_lista_mtx(t_list* lista, pthread_mutex_t* mutex, void* elemento);
t_proceso* buscar_proceso(t_list* lista_procesos,
                          pthread_mutex_t* mutex_procesos, uint32_t pid);
int calcular_memoria_total(t_list* sticks_conectados,
                           pthread_mutex_t* mutex_lista_sockets);
int calcular_espacio_libre(t_list* huecos, pthread_mutex_t* mutex_huecos,
                           t_logger* logger);
int calcular_base_final_segmento(t_list* segmentos);
t_list* filtrar_segmentos_proceso(int pid,
                                  t_memoria_principal* memoria_principal,
                                  t_logger* logger);
void agregar_segmentos_a_paquete(t_list* segmentos,
                                 t_paquete* tabla_segmentos_proceso);
t_memoria_principal* aniadir_memoria_total(
    t_memoria_principal* memoria_principal, int memoria_total);
bool compactar_memoria(int socket_scheduler,
                       t_memoria_principal* memoria_principal);
t_list* compactar_huecos(int memoria_total, int base_final_segmento);
void compactar_segmentos(t_list* segmentos);
void notificar_compactacion(int socket_scheduler);
int calcular_base_final_segmento(t_list* segmentos);
t_segmento* buecar_y_eliminar_segmento(uint32_t id, uint32_t pid,
                                       t_memoria_principal* memoria_principal,
                                       t_logger* logger);
bool hueco_despues_segmento(int base_segmento, int final_segmento,
                            t_list* huecos);
bool hueco_antes_segmento(int base_segmento, int final_segmento,
                          t_list* huecos);
void eliminar_segmento(uint32_t id, uint32_t pid,
                       t_memoria_principal* memoria_principal,
                       t_logger* logger);
void crear_segmento(uint32_t id, uint32_t pid, int size,
                    t_memoria_principal* memoria_principal,
                    int socket_scheduler, t_logger* logger);
int traducir_direccion_logica(uint32_t pid, uint32_t direccion_logica,
                              uint32_t tamanio,
                              t_memoria_principal* memoria_principal,
                              t_logger* logger);
char* leer_de_sticks(int direccion_fisica, int tamanio,
                     t_list* sticks_conectados, pthread_mutex_t* mutex_sticks,
                     t_logger* logger, int socket_scheduler);
int encontrar_stick(int direccion_fisica, t_list* sticks_conectados,
                    pthread_mutex_t* mutex_sticks, int* offset_en_stick);
int calcular_tamanio_proceso(t_proceso* proceso,
                             t_memoria_principal* memoria_principal);
bool escribir_en_sticks(int pid, int dir_fisica, int tamanio_a_leer,
                        char* string_escribir, t_list* sticks_conectados,
                        pthread_mutex_t* mutex_lista_sockets, t_logger* logger,
                        int socket_scheduler);
t_segmento* buscar_segmento(t_memoria_principal* memoria_principal,
                            uint32_t pid, uint32_t num_segmento);
t_hueco selector_de_huecos(uint32_t tamanio, t_logger* logger,
                           t_memoria_principal* memoria);
void actualizar_lista_segmentos(t_memoria_principal* memoria_principal,
                                t_hueco hueco_elegido, int tamanio,
                                uint32_t pid, uint32_t id);
