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
| `make base` | Compila y ejecuta los módulos. |
| `make base-memcheck` | Compila y ejecuta los módulos con memcheck. |
| `make base-helgrind` | Compila y ejecuta los módulos con helgrind. |
| `make pcp` | Compila y ejecuta los módulos. |
| `make pcp-memcheck` | Compila y ejecuta los módulos con memcheck. |
| `make pcp-helgrind` | Compila y ejecuta los módulos con helgrind. |
| `make mem` | Compila y ejecuta los módulos. |
| `make mem-memcheck` | Compila y ejecuta los módulos con memcheck. |
| `make mem-helgrind` | Compila y ejecuta los módulos con helgrind. |
| `make mem` | Compila y ejecuta los módulos. |
| `make mem-memcheck` | Compila y ejecuta los módulos con memcheck. |
| `make mem-helgrind` | Compila y ejecuta los módulos con helgrind. |
| `make php` | Compila y ejecuta los módulos. |
| `make php-memcheck` | Compila y ejecuta los módulos con memcheck. |
| `make php-helgrind` | Compila y ejecuta los módulos con helgrind. |
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
