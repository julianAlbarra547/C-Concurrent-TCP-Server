# Arquitectura del Sistema

## Visión General

El sistema está compuesto por dos procesos independientes no emparentados que se comunican entre sí para gestionar búsquedas sobre un dataset CSV de Spotify de aproximadamente 4 GB.

```
┌─────────────────┐                    ┌─────────────────────┐
│   ui_process    │ ←── FIFOs (IPC) ──→│   csv_process       │
│                 │                    │                      │
│  Interfaz de    │                    │  Búsqueda en índice  │
│  usuario        │                    │  hash + lectura CSV  │
└─────────────────┘                    └─────────────────────┘
                                                │
                              ┌─────────────────┼─────────────────┐
                              │                 │                 │
                     spotify_idx.bin   spotify_entries.bin   spotify_data.csv
                     (tabla hash,      (nodos del índice,    (dataset, 4 GB,
                      80 KB en RAM)     en disco)             solo lectura)
```

La comunicación entre procesos se realiza mediante ** FIFOs nombrados**. La UI envía primero un entero identificador de operación y luego la estructura de datos correspondiente. El servidor siempre responde, incluso en casos de error. Para el diseño detallado del IPC ver [IPC_DESIGN.md](IPC_DESIGN.md).

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
    char       url[64];
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

- El parser maneja campos entre comillas dobles para soportar valores con comas internas, como artistas colaborativos: "Bonny Cepeda, Peter Cruz, Ray Polanco".
- Los tamaños de los buffers se determinaron analizando el máximo real de cada columna con un script Python (realizado con IA) sobre el dataset completo.

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
| `node_exists(table, entries, normalized_title, normalized_artist)` | Verifica si la combinación título+artista ya existe, retorna 1 de ser así y 0 si no  |
| `insert_node(table, entries, normalized_title, normalized_artist, offset)` | Inserta un nodo nuevo  |
| `normalize_string(in, out, size)` | Convierte a minúsculas para búsqueda insensible |
| `create_table(table)` | Inicializa todos los cajones a -1 |
| `build_index(csv_path, idx_path, entries_path)` | Lee el CSV, deduplica por título+artista y genera `spotify_idx.bin` y `spotify_entries.bin` |
| `load_table(idx_path, table)` | Carga `spotify_idx.bin` en RAM (80 KB) |
| `search_node(table, entries, title, artist)` | Busca por título y artista recorriendo nodos en disco, retorna offset en el CSV o -1 |
| `search_range_node(table, entries, title, list, size)` | Busca hasta N nodos por título, retorna el conteo |

Para el diseño detallado del módulo hash ver [HASH_DESIGN.md](HASH_DESIGN.md).

### 3. hash_process

**Archivo:** `hash_process.c`

**Responsabilidad:** Proceso servidor que gestiona el índice hash, atiende búsquedas e inserciones recibidas por FIFO y mantiene el CSV y el índice actualizados.

**Flujo de arranque:**

```
1. Abrir el CSV en modo r+
2. Verificar si el índice ya existe:
   - Si no existe → build_index() desde el CSV completo
   - Si existe    → omitir construcción
3. Crear los FIFOs si no existen
4. Abrir los FIFOs y esperar conexión de la UI
5. Cargar la tabla hash en RAM con load_table()
6. Abrir entries.bin
7. Entrar en loop de atención de solicitudes
```

**Operaciones que atiende:**

| identify | Operación | Entrada | Respuesta |
|---|---|---|---|
| 1 | Búsqueda | `Query` (título + artista opcional) | `int count` + `count` Rows |
| 2 | Inserción | `Row` completo | `int confirm` (1=éxito, 0=error) |

---


### 4. ui_process

**Archivo:** `ui_process.c`

**Responsabilidad:** Ser la interfaz de usuario mediante la cual se puede buscar una canción o agregar un nuevo registro al CSV. Se comunica con `csv_process` mediante FIFOs nombrados.

**Funciones expuestas:**

| Función | Descripción |
|---|---|
| `print_menu()` | Imprime el menú de opciones en pantalla |
| `option1(fdwrite, fdread)` | Solicita título y artista (opcional) al usuario, envía la consulta al servidor y muestra el resultado |
| `option2(fdwrite, fdread)` | Solicita todos los campos de un nuevo registro, los valida y los envía al servidor para inserción |

**Protocolo de comunicación:**

El proceso envía primero un entero que identifica la operación y luego la estructura correspondiente:
```c
write(fdwrite, &identify, sizeof(int));  // 1 = buscar, 2 = insertar
write(fdwrite, &query,    sizeof(Query)); // opcion 1
write(fdwrite, &new_row,  sizeof(Row));   // opcion 2
```

El servidor responde con una `Row` para búsquedas (con `id == -1` si no se encontró) o con un `int` de confirmación para inserciones.

### 5. utils

**Archivos:** `utils.h`, `utils.c`

**Responsabilidad:** Proveer funciones de uso general compartidas entre los módulos del sistema, principalmente validación de entrada del usuario y manipulación de strings.

**Funciones expuestas:**

| Función | Descripción |
|---|---|
| `trim(text)` | Elimina espacios, tabulaciones y saltos de línea al inicio y final de un string |
| `prompt_text(label, out, max_size)` | Muestra una etiqueta, lee la entrada del usuario, aplica trim y retorna 0 si el usuario ingresó "0" para volver |
| `valid_date(date)` | Valida que una fecha tenga formato YYYY-MM-DD con mes y día en rango válido |
| `valid_explicit(explicito)` | Valida que el valor sea exactamente "True" o "False" |
| `valid_positive_int(buff)` | Valida que el string represente un entero positivo |
| `valid_positive_double(buff)` | Valida que el string represente un número decimal positivo con un solo punto |

---

### 6. common

**Archivo:** `common.h`

**Responsabilidad:** Centralizar las definiciones compartidas entre `ui_process` y `csv_process` para garantizar consistencia en la comunicación IPC.

**Contenido:**
```c
#define FIFO_CLIENT_PATH "/tmp/client_fifo"
#define FIFO_SERVER_PATH "/tmp/server_fifo"

typedef struct {
    char title[512];
    char artist[1024];
} Query;
```

Las rutas de los FIFOs se definen aquí para que ambos procesos usen exactamente las mismas sin duplicar constantes. La estructura `Query` define el formato del mensaje de búsqueda enviado de la UI al servidor.
