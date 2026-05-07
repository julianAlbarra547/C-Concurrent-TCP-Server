# Contributing Guide

Este documento define las reglas básicas para trabajar en el repositorio del proyecto.

## 1. Estructura de ramas

El repositorio utiliza el siguiente modelo de ramas:

* **main**
  Contiene únicamente versiones estables del proyecto.

* **develop**
  Rama de integración donde se incorporan todos los cambios antes de pasar a `main`.

## 2. Flujo de trabajo

El flujo de trabajo es el siguiente:

1. Crear una rama a partir de `develop`.
2. Implementar los cambios en esa rama.
3. Subir la rama al repositorio.
4. Crear un Pull Request hacia `develop`.
5. Una vez aprobado, hacer merge en `develop`.
6. Cuando el proyecto esté estable, se realizará un Pull Request de `develop` hacia `main`.

## 3. Convención de nombres para ramas

Las ramas deben seguir la siguiente convención:

* `feature/<nombre>`
  Nueva funcionalidad.

* `fix/<nombre>`
  Corrección de errores.

* `docs/<nombre>`
  Cambios en documentación.

* `refactor/<nombre>`
  Mejoras internas del código sin cambiar funcionalidad.

Ejemplos:

feature/hash-table
fix/ipc-bug
docs/update-readme

## 4. Pull Requests

Para mantener el orden del repositorio:

* No se deben hacer commits directos a `main`.
* Todos los cambios deben pasar por **Pull Request**.
* El Pull Request debe describir brevemente:

  * Qué cambio se realizó.
  * Qué problema resuelve.

## 5. Commits

Se recomienda usar mensajes de commit claros.

Ejemplos:

* `feat: implement hash table structure`
* `fix: correct fifo communication bug`
* `docs: update dataset documentation`

## 6. Buenas prácticas

* Mantener los commits pequeños y claros.
* Probar el código antes de subirlo.
* No subir archivos binarios o datasets grandes al repositorio.
* Seguir la estructura de carpetas definida en el proyecto.
