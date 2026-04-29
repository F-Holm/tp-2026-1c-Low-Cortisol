#ifndef KERNEL_SCHEDULER_IO
#define KERNEL_SCHEDULER_IO

void cerrar_io(int* sockets_io);
void agregar_io(int* sockets_io, int* socket_io, t_log* logger);

#endif /* KERNEL_SCHEDULER_IO */
