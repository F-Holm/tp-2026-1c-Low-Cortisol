#include "cpu/cpu.h"
#include "cpu/registros.h"
#include "cpu/handlers.h"
#include "cpu/memoria.h"

#include <commons/log.h>

#include <utils/src/utils/kernel_scheduler_cpu.h>

#include <stdio.h>

/*             INSTRUCCIONES BASICAS MANEJADAS POR CPU           */

bool handler_noop(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    return true;
}

bool handler_set(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    char* registro = instruccion->parametros[0];
    uint32_t valor = atoi(instruccion->parametros[1]);
    set_registro(contexto, registro, valor);
    return true;
}

bool handler_sum(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    char* registro_destino = instruccion->parametros[0];
    uint32_t resultado = get_registro(contexto, registro_destino) 
                         + get_registro(contexto, instruccion->parametros[1]);

    set_registro(contexto, registro_destino, resultado);
    return true;
}

bool handler_sub(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    char* registro_destino = instruccion->parametros[0];
    uint32_t resultado = get_registro(contexto, registro_destino) 
                         - get_registro(contexto, instruccion->parametros[1]);

    set_registro(contexto, registro_destino, resultado);
    return true;
}

bool handler_jnz(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{

    uint32_t valor_registro = get_registro(contexto, instruccion->parametros[0]);
    if (valor_registro != 0)
        set_registro(contexto, "PC", atoi(instruccion->parametros[1])); 
    
    return true;
}


/*             INSTRUCCIONES CON MODIFICACION DE MEMORIA           */

// INSTRUCCIONES PARA LA TERCERA ENTREGA

bool handler_mov_in(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    uint32_t dir_fisica = mmu(cpu, contexto, contexto->SI, sizeof(uint32_t));

    uint32_t valor = 0;// consegir el valor de la memoria fisica;

    set_registro(contexto, instruccion->parametros[0], valor);
    
    return true;
}

bool handler_mov_out(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    uint32_t valor = get_registro(contexto, instruccion->parametros[0]);

    uint32_t dir_fisica = mmu(cpu, contexto, contexto->DI, sizeof(uint32_t));

    //escribir el valor en la memoria fisica

    return true;
}

bool handler_copy_mem(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    uint32_t dir_SI = get_registro(contexto, contexto->SI);

    uint32_t cant_bits = atoi(instruccion->parametros[0]); // suponiendo que te pasan la cantidad de bits(revisar como es)

    return true;
}

// atoi devuelve int compatible con uint32?


/*             SYSCALLS(MANEJADAS POR SCHEDULER)           */

bool handler_mutex_create(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    if (enviar_string(OP_SYSCALL_MUTEX_CREATE, instruccion->parametros[0], cpu->socket_kernel_scheduler))
    {
        log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
        return true;
    }
    else
    {
        log_error(cpu->logger, "## fallo el envio de la syscall al kernel scheduler");
        return false;
    }
        
}

bool handler_mutex_lock(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    if (enviar_string(OP_SYSCALL_MUTEX_LOCK, instruccion->parametros[0], cpu->socket_kernel_scheduler))
    {
        log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
        return true;
    }
    else
    {
        log_error(cpu->logger, "## fallo el envio de la syscall al kernel scheduler");
        return false;
    }
}

bool handler_mutex_unlock(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    if (enviar_string(OP_SYSCALL_MUTEX_UNLOCK, instruccion->parametros[0], cpu->socket_kernel_scheduler))
    {
        log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
        return true;
    }
    else
    {
        log_error(cpu->logger, "## fallo el envio de la syscall al kernel scheduler");
        return false;
    }
}

bool handler_mem_alloc(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    t_syscall_memory* datos_syscall;
    datos_syscall = malloc(sizeof(t_syscall_memory));

    datos_syscall->pid = pid;
    datos_syscall->id_segmento = atoi(instruccion->parametros[0]);
    datos_syscall->tamaño = atoi(instruccion->parametros[1]);

    if (enviar_buffer(OP_SYSCALL_MEM_ALLOC, datos_syscall, sizeof(t_syscall_memory), cpu->socket_kernel_scheduler))
    {
        log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
        return true;
    }
    else
    {
        log_error(cpu->logger, "## fallo el envio de la syscall al kernel scheduler");
        return false;
    }   
}

bool handler_mem_free(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    t_syscall_memory* datos_syscall;
    datos_syscall = malloc(sizeof(t_syscall_memory));

    datos_syscall->pid = pid;
    datos_syscall->id_segmento = atoi(instruccion->parametros[0]);
    datos_syscall->tamaño = 0;

    if (enviar_buffer(OP_SYSCALL_MEM_FREE, datos_syscall, sizeof(t_syscall_memory), cpu->socket_kernel_scheduler))
    {
        log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
        return true;
    }
    else
    {
        log_error(cpu->logger, "## fallo el envio de la syscall al kernel scheduler");
        return false;
    }   
}

bool handler_sleep(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    t_peticion_sleep* datos_syscall;
    datos_syscall = malloc(sizeof(t_peticion_sleep));

    datos_syscall->pid = pid;
    datos_syscall->tiempo_bloqueado = atoi(instruccion->parametros[0]);

    if (enviar_buffer(OP_SYSCALL_SLEEP, datos_syscall, sizeof(t_peticion_sleep), cpu->socket_kernel_scheduler))
    {
        log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
        return true;
    }
    else
    {
        log_error(cpu->logger, "## fallo el envio de la syscall al kernel scheduler");
        return false;
    }
}

bool handler_stdout(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    t_peticion_stdout* datos_syscall;
    datos_syscall = malloc(sizeof(t_peticion_stdout));

    datos_syscall->pid = pid;
    datos_syscall->direccion_logica = atoi(instruccion->parametros[0]);
    datos_syscall->tamanio_a_escribir = atoi(instruccion->parametros[1]);

    if (enviar_buffer(OP_SYSCALL_STDOUT, datos_syscall, sizeof(t_peticion_stdout), cpu->socket_kernel_scheduler))
    {
        log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
        return true;
    }
    else
    {
        log_error(cpu->logger, "## fallo el envio de la syscall al kernel scheduler");
        return false;
    }   
}

bool handler_stdin(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    t_peticion_stdin* datos_syscall;
    datos_syscall = malloc(sizeof(t_peticion_stdin));

    datos_syscall->pid = pid;
    datos_syscall->direccion_logica = atoi(instruccion->parametros[0]);
    datos_syscall->tamanio_a_leer = atoi(instruccion->parametros[1]);

    if (enviar_buffer(OP_SYSCALL_STDIN, datos_syscall, sizeof(t_peticion_stdin), cpu->socket_kernel_scheduler))
    {
        log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
        return true;
    }
    else
    {
        log_error(cpu->logger, "## fallo el envio de la syscall al kernel scheduler");
        return false;
    }   
}

bool handler_init_proc(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    int prioridad = atoi(instruccion->parametros[1]);
    
    t_paquete* paquete_syscall = crear_paquete(OP_SYSCALL_INIT_PROC);
    agregar_string_a_paquete(paquete_syscall, instruccion->parametros[0]);
    agregar_a_paquete(paquete_syscall, &prioridad, sizeof(int));

    if (enviar_paquete(paquete_syscall, cpu->socket_kernel_scheduler))
    {
        log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
        eliminar_paquete(paquete_syscall);
        return true;
    }
    else
    {
        log_error(cpu->logger, "## fallo el envio de la syscall al kernel scheduler");
        eliminar_paquete(paquete_syscall);
        return false;
    }
}

bool handler_exit(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion, uint32_t pid)
{
    if (enviar_string(OP_SYSCALL_EXIT, "PROCESO TERMINADO", cpu->socket_kernel_scheduler))
    {
        log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
        return true;
    }
    else
    {
        log_error(cpu->logger, "## fallo el envio de la syscall al kernel scheduler");
        return false;
    }
}