#include <commons/config.h>

#include "commons/log.h"

t_log* iniciar_logger(t_config* config);
t_config* iniciar_config(void);
void paquete(int socket_km, char* valor);
void terminar_programa(int socket_km, t_log* logger, t_config* config);

typedef struct
{
  int socket_fd;
  t_log* logger;
} t_datos_hilo_escucha;