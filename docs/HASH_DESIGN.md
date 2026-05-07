# Diseño de la tabla Hash

## Objetivo

Construir un índice en memoria que permita buscar registros del dataset de Spotify por título y artista en menos de 2 segundos, sin cargar el CSV completo en RAM.

## Estructura del nodo

Cada entrada del índice almacena únicamente los campos necesarios para buscar y localizar un registro:

```c
typedef struct hash_node {
    char title[512];         // criterio de búsqueda primario
    char artist[2048];       // criterio de búsqueda secundario
    long offset_csv;         // posición en bytes del registro en el CSV
    long next_entry; 	     // offset del siguiente nodo en el archivo de entries, -1 si es el ultimo
} Hash_node;
```

Los nodos no viven en RAM, sino que se escriben en el archivo bianrio `spotify_entries.bin`, mediante el campo `next_entry` se salta a la posición del siguiente nodo en el archivo de entradas, lo que permite encadenar nodos sin consumir en RAM. El campo `offset_csv` permite saltar directamente al registro en el CSV con `fseek()` sin necesidad de cargarlo completo en memoria. El resto de los campos del registro (álbum, duración, streams, etc.) se leen del CSV únicamente cuando se encuentra una coincidencia.

## Dimensionamiento de la tabla

Se eligieron 9.973 cajones (número primo de 4 cifras) por recomendación del enunciado. Los números primos reducen colisiones al tener menos factores comunes con los valores hasheados.

## Función hash — algoritmo djb2

```c
unsigned int hash(char *title) {
    unsigned int h = 5381;
    for (int i = 0; title[i] != '\0'; i++) {
        h = h * 33 + title[i];
    }
    return h % HASH_TABLE_SIZE;
}
```

Se utiliza el algoritmo djb2 por las siguientes razones:

- Considera el orden de los caracteres, evitando que anagramas colisionen.
- El valor inicial 5381 y el multiplicador 33 fueron determinados empíricamente para producir distribuciones uniformes sobre texto

Antes de hashear, el título se normaliza a minúsculas con `normalize_string()`para garantizar búsquedas insensibles a mayúsculas.

## Manejo de colisiones — encadenamiento

Cuando dos títulos distintos producen el mismo cajón, los nodos se encadenan en una lista enlazada persistida en `spotify_entries.bin`. La inserción se realiza al inicio de la lista por eficiencia: el nuevo nodo apunta al anterior primero del cajón y la tabla se actualiza para apuntar al nuevo.

Al buscar, se recorre la lista leyendo nodos de disco uno por uno con `fseek` + `fread` hasta encontrar coincidencia por título y artista, o hasta llegar a `next_entry == -1`.

## Persistencia del índice

El índice se divide en dos archivos binarios para separar la tabla de los nodos:

**`spotify_idx.bin`** — contiene exactamente `HASH_TABLE_SIZE` valores `long`. Cada posición corresponde a un cajón y guarda el offset del primer nodo de ese cajón en `spotify_entries.bin`, o `-1` si el cajón está vacío. Este archivo se carga completo en RAM al arrancar el programa.

**`spotify_entries.bin`** — contiene los nodos serializados de forma contigua. Cada nodo ocupa `sizeof(Hash_node)` bytes fijos. Los nodos nunca se cargan completos en RAM, solo se leen individualmente durante la búsqueda.
```
spotify_idx.bin:
[offset_cajon_0][offset_cajon_1]...[offset_cajon_9999]
  8 bytes          8 bytes              8 bytes

spotify_entries.bin:
[Hash_node][Hash_node][Hash_node]...
  ~2576 bytes cada uno, encadenados por next_entry
```

Con esta arquitectura el uso de RAM es independiente del tamaño del dataset:
```
RAM al arrancar:   10,000 × 8 bytes = 80 KB
RAM por búsqueda:  sizeof(Hash_node) = ~2576 bytes temporales
