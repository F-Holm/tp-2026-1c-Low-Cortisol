#include "cpu/cpu.h"

void iniciar_hilo(void* arg)
{
    t_cpu* cpu = (t_cpu*) arg;
    pthread_mutex_init(&cpu->hilos.mutex_memory_sticks, NULL);

    pthread_create(
        &cpu->hilos.kernel_memory_hilo,
        NULL,
        escuchar_kernel_memory,
        cpu
    );
}

t_memory_stick crear_nodo(char* ip, int puerto, int socket)
{
    t_memory_stick memory_stick;
    memory_stick.ip = ip;
    memory_stick.puerto = puerto;
    memory_stick.socket = socket;

    return memory_stick;
}

void* escuchar_kernel_memory(void* arg)
{
    t_cpu* cpu = (t_cpu*) arg;
    while (1)
    {
        // ver como recivo la ip y el puerto para luego crear la conexion.
        char* ip_stick = "127.0.0.1";
        int puerto_stick = 22342;

        int nuevo_socket = crear_conexion(ip_stick, puerto_stick);

            // Handshake con memory stick
        enviar_handshake(MID_CPU, nuevo_socket);
        
        int id_modulo = recibir_handshake(nuevo_socket);
        if (id_modulo != MID_MEMORY_STICK)
        {
            log_error(cpu->logger, "## Error en el Handshake con Memory stick,");
            close(nuevo_socket);
            log_destroy(cpu->logger);
            config_destroy(cpu->config);
            return EXIT_FAILURE;
        }
        log_info(cpu->logger, "## Handshake exitoso con Memory stick");

        pthread_mutex_lock(&cpu->hilos.mutex_memory_sticks);

        insertar_nodo_ultimo(&cpu->hilos.kernel_memory_hilo, crear_nodo(ip_stick, puerto_stick, nuevo_socket));

        pthread_mutex_unlock(&cpu->hilos.mutex_memory_sticks);

        free(ip_stick);
        free(puerto_stick);
    }

    return NULL;
}

void insertar_nodo_ultimo(t_nodo_lista_memory_stick **lista, t_memory_stick elemento)
{
    t_nodo_lista_memory_stick *nuevo;
    nuevo->memory_stick = elemento;
    nuevo->sgte = NULL;
    t_nodo_lista_memory_stick *aux = lista;

    if(lista)
    {
        while(aux->sgte)
            aux = aux->sgte;

        aux->sgte = nuevo;
    }
    else
    {
        lista=nuevo;
    }
}
