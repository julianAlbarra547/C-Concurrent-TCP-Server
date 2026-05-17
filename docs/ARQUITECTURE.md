# Arquitectura del Sistema

## Visión General

El sistema está compuesto por dos programas independientes — un servidor y un cliente — que se comunican a través de la red mediante **sockets TCP**. El servidor gestiona búsquedas e inserciones sobre un dataset CSV de Spotify de aproximadamente 4 GB, atendiendo hasta 32 clientes de forma concurrente mediante hilos POSIX.

```
┌─────────────────┐                        ┌──────────────────────────────────────┐
│   p2-client     │ ←── TCP (puerto 8080) ──│   p2-server                          │
│                 │                        │                                      │
│  Interfaz de    │                        │  hilo principal: acepta conexiones   │
│  usuario        │                        │  hilo por cliente: atiende queries   │
└─────────────────┘                        └──────────────────────────────────────┘
                                                            │
                                        ┌───────────────────┼───────────────────┐
                                        │                   │                   │
                               spotify_idx.bin   spotify_entries.bin   spotify_data.csv
                               (tabla hash,      (nodos del índice,    (dataset, ~4 GB,
                                80 KB en RAM)     en disco)             lectura/escritura)
```

La comunicación se realiza de forma binaria mediante las funciones `send_all()` y `rcv_all()` definidas en `common.c`. El cliente envía primero un entero identificador de operación y luego la estructura de datos correspondiente. El servidor siempre responde, incluso en casos de error. Para el diseño detallado de la comunicación ver [SOCKET_DESIGN.md](SOCKET_DESIGN.md).

---

## Módulos implementados

### 1. csv_reader

**Archivos:** `csv_reader.h`, `csv_reader.c`

**Responsabilidad:** Abrir el CSV, posicionarse en un registro dado su offset en bytes y parsearlo en una estructura `Row`.

**Estructura principal:**

```c
typedef struct {
    int        id;
    char       title[512];
    short      rank;
    char       date[16];
    char       artist[2048];
    char       url[128];
    long long  streams;
    char       album[512];
    double     duration;
    char       explicito[8];
} Row;
```

**Funciones expuestas:**

| Función | Descripción |
|---|---|
| `open_csv(path)` | Abre el archivo CSV y retorna un `FILE *` |
| `skip_header(file)` | Descarta la primera línea del CSV |
| `read_csv(file, offset)` | Lee y parsea el registro en la posición `offset` |
| `print_row(row)` | Imprime todos los campos de un registro |
| `close_csv(file)` | Cierra el archivo CSV |

**Decisiones de diseño:**

- El parser maneja campos entre comillas dobles para soportar valores con comas internas, como artistas colaborativos: `"Bonny Cepeda, Peter Cruz, Ray Polanco"`.
- Los tamaños de los buffers se determinaron analizando el máximo real de cada columna con un script Python sobre el dataset completo.

---

### 2. hash

**Archivos:** `hash.h`, `hash.c`

**Responsabilidad:** Construir, persistir y consultar un índice hash sobre el campo título del dataset para localizar registros en O(1) promedio.

**Estructura principal:**

```c
typedef struct hash_node {
    char title[512];    // criterio de búsqueda primario
    char artist[2048];  // criterio de búsqueda secundario
    long offset;        // posición en bytes del registro en el CSV
    long next_entry;    // offset del siguiente nodo en entries.bin, -1 si es el último
} Hash_node;
```

Los nodos no viven en RAM sino en `spotify_entries.bin`. La tabla hash en RAM es únicamente un arreglo de `long` de 80 KB que apunta a los nodos en disco.

**Funciones expuestas:**

| Función | Descripción |
|---|---|
| `hash(title)` | Calcula el cajón usando djb2 |
| `normalize_string(in, out, size)` | Convierte a minúsculas para búsqueda insensible a mayúsculas |
| `create_table(table)` | Inicializa todos los cajones a -1 |
| `node_exists(table, entries, title, artist)` | Verifica si la combinación título+artista ya existe |
| `insert_node(table, entries, title, artist, offset)` | Inserta un nodo nuevo al inicio de la lista del cajón |
| `build_index(csv_path, idx_path, entries_path)` | Lee el CSV, deduplica por título+artista y genera los archivos de índice |
| `load_table(idx_path, table)` | Carga `spotify_idx.bin` en RAM (80 KB) |
| `search_node(table, entries, title, artist)` | Búsqueda exacta por título y artista; retorna offset en el CSV o -1 |
| `search_range_node(table, entries, title, list, size)` | Búsqueda por título sin artista; retorna hasta N coincidencias |

Para el diseño detallado del módulo hash ver [HASH_DESIGN.md](HASH_DESIGN.md).

---

### 3. p2-server

**Archivo:** `p2-server.c`

**Responsabilidad:** Proceso servidor que gestiona el índice hash, atiende búsquedas e inserciones recibidas por TCP y mantiene el CSV y el índice actualizados. Soporta hasta 32 clientes concurrentes mediante un hilo POSIX por cliente y un semáforo de conteo.

**Flujo de arranque:**

```
1. Abrir el CSV en modo r+
2. Verificar si el índice ya existe:
   - Si no existe → build_index() desde el CSV completo
   - Si existe    → omitir construcción
3. Cargar la tabla hash en RAM con load_table() (80 KB en stack)
4. Abrir spotify_entries.bin en modo rb+
5. Abrir el archivo de log en modo append
6. Crear el socket TCP, bind en puerto 8080, listen con backlog 32
7. Inicializar el semáforo (MAX_CLIENTS = 32) y los mutexes
8. Entrar en loop: accept → sem_wait → crear hilo → pthread_detach
```

**Concurrencia:**

| Mecanismo | Propósito |
|---|---|
| `pthread_t` por cliente | Cada conexión se atiende en un hilo independiente |
| `sem_t client_count_sem` | Limita el total de clientes activos a 32 |
| `pthread_mutex_t hash_mutex` | Protege lectura/escritura del índice hash y entries.bin |
| `pthread_mutex_t csv_mutex` | Protege lectura/escritura del archivo CSV |
| `pthread_mutex_t log_mutex` | Protege escritura del archivo de log |

**Operaciones que atiende:**

| identify | Operación | Entrada | Respuesta |
|---|---|---|---|
| 1 | Búsqueda | `Query` (título + artista opcional) | `int count` + `count` Rows |
| 2 | Inserción | `Row` completo | `int confirm` (1=éxito, 0=error) |
| 0 | Desconexión | — | cierra el hilo limpiamente |

**Formato de log:**

```
[Fecha YYYYMMDDTHHMMSS] Cliente [IP] [búsqueda - Titulo: X - Artista: Y]
[Fecha YYYYMMDDTHHMMSS] Cliente [IP] [inserción - Titulo: X - Artista: Y]
```

---

### 4. p2-client

**Archivo:** `p2-client.c`

**Responsabilidad:** Interfaz de usuario que permite buscar o agregar canciones. Se conecta al servidor por TCP y mantiene la conexión abierta durante toda la sesión.

La IP del servidor puede configurarse de tres maneras, en orden de prioridad:

1. Argumento de línea de comandos: `./bin/p2-client 192.168.1.10`
2. Variable de entorno: `export P2_SERVER_IP=192.168.1.10`
3. Por defecto: `127.0.0.1`

**Funciones expuestas:**

| Función | Descripción |
|---|---|
| `print_menu()` | Imprime el menú de opciones con colores ANSI |
| `option1(sockfd)` | Solicita título y artista (opcional), envía la consulta y muestra resultados |
| `option2(sockfd)` | Solicita y valida todos los campos de un nuevo registro, lo envía al servidor |

**Protocolo de comunicación:**

```c
send_all(sockfd, &identify, sizeof(int));  // 1 = buscar, 2 = insertar, 0 = salir
send_all(sockfd, &query,    sizeof(Query)); // opción 1
send_all(sockfd, &new_row,  sizeof(Row));   // opción 2
```

---

### 5. utils_ui

**Archivos:** `utils_ui.h`, `utils_ui.c`

**Responsabilidad:** Proveer funciones de validación de entrada del usuario y manipulación de strings, usadas exclusivamente por el cliente.

**Funciones expuestas:**

| Función | Descripción |
|---|---|
| `trim(text)` | Elimina espacios, tabulaciones y saltos de línea al inicio y final de un string |
| `prompt_text(label, out, max_size)` | Muestra una etiqueta, lee la entrada, aplica trim y retorna 0 si el usuario ingresó "0" |
| `valid_date(date)` | Valida formato YYYY-MM-DD con mes y día en rango válido |
| `valid_explicit(explicito)` | Valida que el valor sea exactamente "True" o "False" |
| `valid_positive_int(buff)` | Valida que el string represente un entero positivo |
| `valid_positive_double(buff)` | Valida que el string represente un decimal positivo con un solo punto |

---

### 6. common

**Archivos:** `common.h`, `common.c`

**Responsabilidad:** Centralizar las definiciones y funciones de red compartidas entre cliente y servidor.

**Contenido de `common.h`:**

```c
#define PORT 8080

typedef struct {
    char title[512];
    char artist[2048];
} Query;

int rcv_all(int sockfd, void *buffer, size_t length);
int send_all(int sockfd, void *buffer, size_t length);
```

**Funciones de `common.c`:**

| Función | Descripción |
|---|---|
| `send_all(sockfd, buffer, length)` | Envía exactamente `length` bytes, reintentando si `send()` envía parcial |
| `rcv_all(sockfd, buffer, length)` | Recibe exactamente `length` bytes, reintentando si `recv()` recibe parcial |

Estas funciones son necesarias porque TCP no garantiza que un solo `send()`/`recv()` transfiera la totalidad de los bytes solicitados.
