#include <commons/config.h>
#include "commons/log.h"

t_log* iniciar_logger(void);
t_config* iniciar_config(void);
void paquete(int conexion, char* valor);
void terminar_programa(int conexion, t_log* logger, t_config* config);