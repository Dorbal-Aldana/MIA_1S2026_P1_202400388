# Backend — Proyecto 1 MIA

## Compilar

**Make (por defecto):**
```bash
make          # o make rebuild para forzar recompilación
./main
```

**CMake (como en CLASE7 del lab):**
```bash
mkdir -p build && cd build
cmake ..
make
./main
```

El servidor HTTP escucha en el puerto **8080** (`POST /api/command` con JSON `{"command":"..."}`).

## Parser de comandos

El análisis de cada línea sigue el enfoque de **CLASE7**: nombre del comando + parámetros `-clave=valor` extraídos con **regex** (`Analyzer::parseLine`).  
Las rutas con espacios deben ir entre **comillas dobles** en el valor, p. ej. `-path="/home/user/mi disco.dsk"`.

## Discos (como en el lab)

- **`Utilities::CreateFile` / `Utilities::OpenFile`**: mismos nombres que en CLASE7; crean carpetas padre sin tirar excepción y abren el `.dsk` en binario.
- **`namespace DiskManagement`**: la API pública son **funciones** (`mkdisk`, `fdisk`, …), no hace falta instanciar una clase en `main` (igual idea que `DiskManagement::Mkdisk` del lab).
- Implementación interna: `DiskManagementImpl` en `DiskManagementImpl.h` + `.cpp`.

**Corrección importante:** `fdisk` ya **no** vuelve a abrir el disco para calcular espacio libre a mitad de comando (eso cerraba el archivo y dejaba el flujo inconsistente). El espacio libre se calcula con el MBR en memoria o con el mismo `fstream` abierto.

**Nota:** En el P1 los discos usan `-path=...`; en el ejemplo del lab a veces aparece `-drive=A` — aquí se mantiene el formato del **proyecto** (`-path`, `-name`, etc.).
