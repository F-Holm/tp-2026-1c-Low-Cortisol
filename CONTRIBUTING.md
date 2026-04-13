# Guía de Contribución (CONTRIBUTING.md)

## 1. Normas de Estilo de Código (C)

| Elemento | Ejemplo |
| :--- | :--- |
| **Constantes y Macros** | `NOMBRE_ELEMENTO` |
| **Variables y Funciones** | `nombre_elemento` |
| **Structs y Typedefs** | `nombre_elemento_t` |

---

## 2. Comandos de Make

| Comando | Descripción |
| :--- | :--- |
| `make` o `make all` | Es lo mismo que hacer make debug. |
| `make debug` | Compila el proyecto completo en debug. |
| `make release` | Compila el proyecto completo en release. |
| `make clean` | Elimina todos los archivos objeto (`.o`) y los binarios generados. |
| `make test` | Compila y ejecuta los tests. |
| `make format` | Ejecuta clang-format para darle formato estándar a todo el código. |

---

## 3. Comentarios

Cada función debe estar declarada en un header con el mismo nombre que el archivo de código donde se implementa.

Agregar comentarios con el formato que tiene el ejemplo de abajo antes de la declaración de cada función en los headers.

```c
/**
* @brief Imprime un saludo por consola
* @param quien Módulo desde donde se llama a la función
* @return No devuelve nada
*/
void saludar(char* quien);
```
