# ExtreamFS - Frontend

Interfaz web para el sistema de archivos EXT2 (Proyecto 1 MIA).

## Requisitos

- Node.js 18+ y npm (o pnpm/yarn)

## Instalación

```bash
cd frontend
npm install
```

## Desarrollo

1. **Inicia el backend** (en otra terminal):

   ```bash
   cd backend
   make
   ./main
   ```

   El backend debe quedar escuchando en **http://localhost:8080**.

2. **Inicia el frontend**:

   ```bash
   npm run dev
   ```

   Se abrirá **http://localhost:5173**. Las peticiones a `/api` se redirigen al backend (puerto 8080).

## Uso

- **Entrada de comandos**: escribe los comandos en el área de texto (por ejemplo `mkdisk -size=10 -path=/tmp/disco.mia`).
- **Cargar script (.smia)**: carga un archivo `.smia` con varios comandos (y comentarios con `#`) en el área de entrada.
- **Ejecutar**: envía los comandos al backend y muestra la salida en el área derecha.

## Build para producción

```bash
npm run build
```

Los archivos quedan en `dist/`. Puedes servir esa carpeta con cualquier servidor estático o configurar el backend para servirla.
