# Diseño de la Comunicación por Sockets TCP

## Objetivo

Permitir que múltiples clientes (`p2-client`) se comuniquen con el servidor (`p2-server`) a través de la red de forma concurrente, enviando criterios de búsqueda o registros nuevos y recibiendo los resultados correspondientes.

## Mecanismo elegido — Sockets TCP

Se utiliza la API de sockets de Berkeley con el protocolo **TCP** (`SOCK_STREAM`), que garantiza entrega ordenada y sin pérdida de datos. A diferencia de los FIFOs, los sockets TCP permiten:

- Comunicación entre procesos en **máquinas distintas** (no solo en el mismo host).
- **Múltiples clientes concurrentes** sin crear un FIFO por cliente.
- Detección automática de desconexiones mediante el retorno de `recv()`.

```
p2-client (cualquier host)                  p2-server (puerto 8080)
         │                                           │
         │──── connect(127.0.0.1:8080) ─────────────►│
         │                                           │ accept() → nuevo hilo
         │──── send(identify)  ──────────────────────►│
         │──── send(query/row) ──────────────────────►│
         │                                           │ procesa
         │◄─── recv(count)     ─────────────────────│
         │◄─── recv(Row * N)   ─────────────────────│  ← búsqueda
         │◄─── recv(confirm)   ─────────────────────│  ← inserción
         │                                           │
         │──── send(identify=0) ─────────────────────►│ cierra hilo
```

## Diagrama de comunicación completo

```
p2-client                               p2-server (hilo por cliente)
    │                                           │
    │─── send_all(&identify, sizeof(int)) ─────►│
    │─── send_all(&query,    sizeof(Query)) ────►│  (operación 1)
    │                                           │
    │◄── rcv_all(&count,   sizeof(int)) ────────│
    │◄── rcv_all(&results, sizeof(Row)*N) ──────│
    │                                           │
    │─── send_all(&identify, sizeof(int)) ─────►│
    │─── send_all(&new_row,  sizeof(Row)) ──────►│  (operación 2)
    │                                           │
    │◄── rcv_all(&confirm, sizeof(int)) ────────│
    │                                           │
    │─── send_all(&identify=0, sizeof(int)) ────►│  (desconexión)
    │                                           │ cierra socket, sem_post
```

## Protocolo de mensajes

Cada operación comienza con un entero que identifica el tipo de solicitud, seguido de la estructura de datos correspondiente. Todos los datos se transmiten como structs binarios de tamaño fijo.

### Operación 1 — Búsqueda por título (sin artista)

El cliente deja el campo `artist` vacío en la estructura `Query`. El servidor busca todas las canciones con ese título y devuelve hasta 5 resultados.

```c
// Cliente envía:
int identify = 1;
send_all(sockfd, &identify, sizeof(int));
send_all(sockfd, &query,    sizeof(Query));  // query.artist == ""

// Servidor responde:
int count;  // número de coincidencias encontradas (0..5), o -1 si hubo error
send_all(sockfd, &count, sizeof(int));

// Luego envía exactamente count Rows:
for(int i = 0; i < count; i++){
    send_all(sockfd, &results[i], sizeof(Row));
}
```

### Operación 1 — Búsqueda exacta por título y artista

```c
// Cliente envía:
int identify = 1;
send_all(sockfd, &identify, sizeof(int));
send_all(sockfd, &query,    sizeof(Query));  // query.artist != ""

// Servidor responde:
int count;  // 1 si se encontró, -1 si no existe o hubo error
send_all(sockfd, &count, sizeof(int));

// Si count == 1, envía el registro:
send_all(sockfd, row, sizeof(Row));
```

### Operación 2 — Inserción

```c
// Cliente envía:
int identify = 2;
send_all(sockfd, &identify, sizeof(int));
send_all(sockfd, &new_row,  sizeof(Row));

// Servidor responde:
int confirm;  // 1 = éxito, 0 = error (duplicado o fallo de escritura)
send_all(sockfd, &confirm, sizeof(int));
```

### Operación 0 — Desconexión limpia

```c
// Cliente envía al seleccionar "Salir":
int identify = 0;
send_all(sockfd, &identify, sizeof(int));
// El hilo del servidor sale del loop y libera recursos.
```

## Estructuras compartidas

Definidas en `common.h` para garantizar que ambos programas usen exactamente el mismo layout en memoria:

```c
#define PORT 8080

typedef struct {
    char title[512];
    char artist[2048];
} Query;
```

La estructura `Row` se comparte desde `csv_reader.h` y contiene todos los campos del dataset.

## Funciones de red — send_all y rcv_all

TCP puede fragmentar los datos: un único `send()` o `recv()` puede transferir menos bytes de los solicitados. Para garantizar la integridad de las estructuras, se usan bucles que reintentan hasta completar la transferencia:

```c
int send_all(int sockfd, void *buffer, size_t length){
    size_t total_sent = 0;
    while(total_sent < length){
        int sent = send(sockfd, buffer + total_sent, length - total_sent, 0);
        if(sent <= 0) return -1;
        total_sent += sent;
    }
    return 0;
}

int rcv_all(int sockfd, void *buffer, size_t length){
    size_t total_received = 0;
    while(total_received < length){
        int received = recv(sockfd, buffer + total_received, length - total_received, 0);
        if(received <= 0) return -1;
        total_received += received;
    }
    return 0;
}
```

Ambas funciones retornan 0 en éxito y -1 en error o desconexión del par.

## Ciclo de vida de la conexión

```
p2-server arranca
    │
    ├── socket(AF_INET, SOCK_STREAM, 0)
    ├── bind(sockfd, INADDR_ANY, PORT 8080)
    ├── listen(sockfd, BACKLOG=32)
    └── loop:
          ├── accept(sockfd, &client_addr)     ← bloquea hasta nueva conexión
          ├── sem_wait(client_count_sem)        ← espera si ya hay 32 clientes
          ├── malloc(ThreadArgs)
          ├── pthread_create(&tid, process_query, args)
          └── pthread_detach(tid)              ← hilo se limpia solo al terminar

p2-client arranca
    │
    ├── socket(AF_INET, SOCK_STREAM, 0)
    ├── connect(sockfd, server_addr)           ← desbloquea accept() del servidor
    └── loop de mensajes hasta opción Salir
          └── send(identify=0) → close(sockfd)

hilo servidor al recibir identify=0 o error de recv:
    ├── free(ThreadArgs)
    ├── sem_post(client_count_sem)             ← libera un cupo de cliente
    └── close(client_sock)
```

## Gestión de concurrencia

Múltiples hilos comparten los mismos recursos del servidor. Se usan tres mutexes para proteger las secciones críticas:

| Mutex | Recurso protegido | Sección crítica |
|---|---|---|
| `hash_mutex` | tabla hash + entries.bin | `search_node`, `search_range_node`, `node_exists`, `insert_node`, escritura de `spotify_idx.bin` |
| `csv_mutex` | archivo CSV | `read_csv`, escritura de nuevos registros con `fprintf` |
| `log_mutex` | archivo de log | escritura de entradas de log con `fwrite` |

El semáforo `client_count_sem` se inicializa en 32 y actúa como barrera: `sem_wait()` antes de crear el hilo garantiza que nunca haya más de `MAX_CLIENTS` hilos activos simultáneamente. Al finalizar cada hilo, `sem_post()` libera el cupo.

## Consideraciones técnicas

**Persistencia de la conexión:** A diferencia de los FIFOs donde la conexión era implícita, con sockets TCP el cliente mantiene una sola conexión abierta durante toda su sesión y envía múltiples operaciones sucesivas por ella.

**Comunicación binaria:** Los datos se transmiten como structs binarios de tamaño fijo. Esto requiere que cliente y servidor compartan exactamente las mismas definiciones de struct (de ahí la importancia de `common.h` y `csv_reader.h`), y que ambos programas corran en arquitecturas con el mismo endianness.

**Detección de desconexión:** Si `recv()` retorna 0 o negativo en cualquier punto del protocolo, el hilo interpreta que el cliente se desconectó abruptamente y sale del loop, liberando recursos como si hubiera recibido `identify = 0`.

**Protocolo sincrónico:** Cada mensaje enviado por el cliente debe tener exactamente una respuesta del servidor antes de enviar el siguiente. Desincronizar envíos y recepciones deja bytes huérfanos en el buffer TCP que corrompen las operaciones siguientes.

**IP del servidor configurable:** El cliente acepta la IP del servidor como argumento de línea de comandos o mediante la variable de entorno `P2_SERVER_IP`, con valor por defecto `127.0.0.1`. Esto permite conectarse a un servidor remoto sin recompilar.
