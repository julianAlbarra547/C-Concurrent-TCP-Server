## Dataset Overview

El dataset utilizado en este proyecto es una **versión optimizada y depurada** del conjunto de datos original de Spotify Charts. Se han seleccionado exclusivamente las columnas esenciales para el objetivo de este proyecto, eliminando métricas técnicas de audio para reducir el tamaño del archivo y agilizar el procesamiento.

El dataset incluye información clave como:
- Identificadores únicos de registro.
- Metadatos del track (título, artista, álbum).
- Estadísticas de rendimiento (rank, streams).
- Atributos del contenido (explicit, duration).

## Source

Este dataset es una versión derivada y procesada por el autor de este repositorio. Los datos originales fueron obtenidos de Kaggle:

**Original Dataset Source:**
Spotify Charts + Audio Data (Sunny Kakar)
https://www.kaggle.com/datasets/sunnykakar/spotify-charts-all-audio-data

**Versión Optimizada (Utilizada en este proyecto):**
https://www.kaggle.com/datasets/julianalbarra547/spotify-daily-charts-optimized-version

## Dataset Structure

El dataset se encuentra en formato CSV. Cada fila representa una entrada única en un chart diario y cada columna representa un atributo específico de la canción.

**Formato de las columnas:**
Index | Title | Rank | Date | Artist | URL | Streams | Album | Duration_ms | Explicit

## Fields Description

| Field | Description |
|------|-------------|
| Index | Identificador numérico del registro (ID original) - Type: Int |
| Title | Nombre de la canción - Type: Str |
| Rank | Posición de la canción en el ranking diario - Type: Int |
| Date | Fecha de la aparición en el chart - Type: Date (YYYY-MM-DD) |
| Artist | Nombre del artista o artistas - Type: Str |
| Url | Enlace directo a la canción en Spotify - Type: Str |
| Streams | Número total de reproducciones ese día - Type: Int |
| Album | Nombre del álbum al que pertenece el track - Type: Str |
| Duration_ms | Duración de la canción en milisegundos - Type: Int |
| Explicit | Indica si la canción contiene contenido explícito - Type: Bool (True/False) |

## Searchable Fields

El sistema permitirá realizar consultas usando los siguientes campos:

**Primary Search Field:**
- `Title` (Clave principal para la función Hash)

**Secondary Search Fields:**
- `Artist`
- `Duration_ms`

## Indexing Strategy

Para permitir búsquedas rápidas se implementará una **tabla hash**.

La clave principal será el campo **Title**. La función hash generará una posición en una estructura de índice que apunta al **offset** (posición en bytes) del registro dentro del archivo CSV.

Esto permite acceder directamente al registro mediante un `seek` en el sistema de archivos, permitiendo realizar búsquedas en milisegundos sin cargar los gigabytes del dataset en la memoria RAM.

## Dataset Storage

El dataset real se almacenará en:
`data/raw/spotify_data.csv`

Debido a su tamaño (aprox. 1.26 GB comprimido / >5 GB descomprimido), el dataset no se incluye directamente en el repositorio de GitHub. Se recomienda descargarlo desde el enlace de Kaggle proporcionado arriba.

Para pruebas de desarrollo se utiliza un dataset reducido con una muestra representativa:
`data/sample/sample.csv`

## Dataset Size

- **Registros:** El dataset contiene aproximadamente 26,000,000 de registros (estimado basado en la versión completa).
- **Tamaño del archivo:** Aproximadamente **1.26 GB** (comprimido en ZIP) / El archivo original del que se derivó pesaba **27 GB**.
