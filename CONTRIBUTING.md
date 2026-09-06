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

### Compilación

| Comando | Descripción |
| :--- | :--- |
| `make` o `make all` | Es lo mismo que hacer `make debug`. |
| `make debug` | Compila el proyecto completo en debug. |
| `make release` | Compila el proyecto completo en release. |
| `make clean` | Elimina todos los archivos objeto (`.o`) y los binarios generados. |
| `make logs` | Elimina todos los logs generados. |
| `make test` | Compila y ejecuta los tests unitarios de cada módulo. |
| `make format` | Ejecuta clang-format para darle formato estándar a todo el código. |

### Pruebas end-to-end

Cada directorio dentro de `tests/` (menos `pseudocode/`) es un escenario: contiene
sus seis archivos `<modulo>.conf` y un `test.mk` con sus parámetros.

| Comando | Descripción |
| :--- | :--- |
| `make <escenario>` | Compila y lanza el escenario indicado. |
| `make <escenario> MODE=memcheck` | Igual, con cada proceso bajo Valgrind memcheck. |
| `make <escenario> MODE=helgrind` | Igual, con cada proceso bajo Valgrind helgrind. |
| `make run` | Alias de `make full`. |
| `make kill` | Detiene todos los procesos del sistema. |

Escenarios disponibles: `base`, `base2`, `pcp`, `mem-best`, `mem-worst`, `pmp`,
`pmp-det`, `pmp-v2`, `pmp-det-v2`, `php`, `php-v2`, `es3-1`, `es3-2`, `es3-3`,
`es3-4`, `full`.

Los logs de cada proceso quedan en `./output/`.

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
