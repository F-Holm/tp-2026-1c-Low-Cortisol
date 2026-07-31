# Guía de Contribución (CONTRIBUTING.md)

## 1. Normas de Estilo de Código (C)

| Elemento | Ejemplo |
| :--- | :--- |
| **Constantes y Macros** | `NOMBRE_ELEMENTO` |
| **Variables y Funciones** | `nombre_elemento` |
| **Structs y Typedefs** | `t_nombre_elemento` |
| **Headers** | `nombre_elemento.h` |
| **Archivos de código** | `nombre_elemento.c` |

---

## 2. Comandos de Make

| Comando | Descripción |
| :--- | :--- |
| `make` o `make all` | Es lo mismo que hacer make debug. |
| `make debug` | Compila el proyecto completo en debug. |
| `make release` | Compila el proyecto completo en release. |
| `make clean` | Elimina todos los archivos objeto (`.o`) y los binarios generados. |
| `make logs` | Elimina todos los logs generados. |
| `make test` | Compila y ejecuta los tests. |
| `make format` | Ejecuta clang-format para darle formato estándar a todo el código. |
| `make run` | Compila y ejecuta los módulos. |
| `make memcheck` | Compila y ejecuta los módulos con memcheck. |
| `make helgrind` | Compila y ejecuta los módulos con helgrind. |
| `make base` | Compila y ejecuta la Prueba Base usando PLANI_PRE_0.prc. |
| `make base-memcheck` | Compila y ejecuta la Prueba Base con memcheck usando PLANI_PRE_0.prc. |
| `make base-helgrind` | Compila y ejecuta la Prueba Base con helgrind usando PLANI_PRE_0.prc. |
| `make base2` | Compila y ejecuta la Prueba Base usando MEMORIA_PRE_0.prc. |
| `make base2-memcheck` | Compila y ejecuta la Prueba Base con memcheck usando MEMORIA_PRE_0.prc. |
| `make base2-helgrind` | Compila y ejecuta la Prueba Base con helgrind usando MEMORIA_PRE_0.prc. |
| `make pcp` | Compila y ejecuta la Prueba Planificación Corto Plazo. |
| `make pcp-memcheck` | Compila y ejecuta la Prueba Planificación Corto Plazo con memcheck. |
| `make pcp-helgrind` | Compila y ejecuta la Prueba Planificación Corto Plazo con helgrind. |
| `make mem` | Compila y ejecuta la Prueba Memoria usando Best Fit. |
| `make mem-memcheck` | Compila y ejecuta la Prueba Memoria con memcheck usando Best Fit. |
| `make mem-helgrind` | Compila y ejecuta la Prueba Memoria con helgrind usando Best Fit. |
| `make mem2` | Compila y ejecuta la Prueba Memoria usando Worst Fit. |
| `make mem2-memcheck` | Compila y ejecuta la Prueba Memoria con memcheck usando Worst Fit. |
| `make mem2-helgrind` | Compila y ejecuta la Prueba Memoria con helgrind usando Worst Fit. |
| `make pmp` | Compila y ejecuta la Prueba Planificación Mediano Plazo. |
| `make pmp-memcheck` | Compila y ejecuta la Prueba Planificación Mediano Plazo con memcheck. |
| `make pmp-helgrind` | Compila y ejecuta la Prueba Planificación Mediano Plazo con helgrind. |
| `make pmp2` | Compila y ejecuta la Prueba Planificación Mediano Plazo (más determinística). |
| `make pmp2-memcheck` | Compila y ejecuta la Prueba Planificación Mediano Plazo (más determinística) con memcheck. |
| `make pmp2-helgrind` | Compila y ejecuta la Prueba Planificación Mediano Plazo (más determinística) con helgrind. |
| `make php` | Compila y ejecuta la Prueba Herencia de Prioridades. |
| `make php-memcheck` | Compila y ejecuta la Prueba Herencia de Prioridades con memcheck. |
| `make php-helgrind` | Compila y ejecuta la Prueba Herencia de Prioridades con helgrind. |
| `make php2` | Compila y ejecuta la Prueba Herencia de Prioridades v2. |
| `make php2-memcheck` | Compila y ejecuta la Prueba Herencia de Prioridades v2 con memcheck. |
| `make php2-helgrind` | Compila y ejecuta la Prueba Herencia de Prioridades v2 con helgrind. |
| `make es31` | Compila y ejecuta la Prueba de Estabilidad 1. |
| `make es31-memcheck` | Compila y ejecuta la Prueba de Estabilidad 1 con memcheck. |
| `make es31-helgrind` | Compila y ejecuta la Prueba de Estabilidad 1 con helgrind. |
| `make es31` | Compila y ejecuta la Prueba de Estabilidad 2. |
| `make es31-memcheck` | Compila y ejecuta la Prueba de Estabilidad 2 con memcheck. |
| `make es31-helgrind` | Compila y ejecuta la Prueba de Estabilidad 2 con helgrind. |
| `make es31` | Compila y ejecuta la Prueba de Estabilidad 3. |
| `make es31-memcheck` | Compila y ejecuta la Prueba de Estabilidad 3 con memcheck. |
| `make es31-helgrind` | Compila y ejecuta la Prueba de Estabilidad 3 con helgrind. |
| `make es31` | Compila y ejecuta la Prueba de Estabilidad 4. |
| `make es31-memcheck` | Compila y ejecuta la Prueba de Estabilidad 4 con memcheck. |
| `make es31-helgrind` | Compila y ejecuta la Prueba de Estabilidad 4 con helgrind. |
| `make kill` | Detiene la ejecución del sistema. |

---

## 3. Comentarios

Cada función debe estar declarada en un header con el mismo nombre que el archivo de código donde se implementa.

Agregar comentarios con el formato que tiene el ejemplo de abajo antes de la declaración de cada función en los headers.

```c
/**
 * @brief Imprime un saludo por consola
 * @param quien Módulo desde donde se llama a la función
 * @return No devuelve nada
 * @note liberar memoria dinámica del ...
 */
void saludar(char* quien);
```
