#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <ctime>
#include <cstring>
#include <vector>
#include <string>

//==================== ESTRUCTURAS DE DISCO ====================

struct Partition {
    char part_status;      // '0' = desmontada, '1' = montada
    char part_type;        // 'P' = primaria, 'E' = extendida
    char part_fit;         // 'B' = best, 'F' = first, 'W' = worst
    int part_start;        // Byte donde inicia
    int part_size;         // Tamaño en bytes
    char part_name[16];    // Nombre de la partición
    int part_correlative;  // -1 si no montada, 1,2,3...
    char part_id[4];       // ID generado al montar (ej. "341")
    
    Partition();  // Declaración del constructor
};

struct MBR {
    int mbr_tamano;                 // Tamaño total del disco en bytes
    time_t mbr_fecha_creacion;      // Fecha y hora de creación
    int mbr_dsk_signature;          // Número random único
    char dsk_fit;                   // 'B', 'F', 'W' - ajuste del disco
    Partition mbr_partitions[4];    // 4 particiones
    
    MBR();
};

struct EBR {
    char part_mount;        // '0' = desmontada, '1' = montada
    char part_fit;          // 'B', 'F', 'W'
    int part_start;         // Byte donde inicia
    int part_size;          // Tamaño en bytes
    int part_next;          // Byte del próximo EBR, -1 si no hay
    char part_name[16];     // Nombre de la partición
    
    EBR();  // Declaración del constructor
};

// Definimos Inode primero
struct Inode {
    int i_uid;                  // UID del propietario
    int i_gid;                  // GID del grupo
    int i_size;                 // Tamaño del archivo en bytes
    time_t i_atime;             // Último acceso
    time_t i_ctime;             // Fecha de creación
    time_t i_mtime;             // Última modificación
    int i_block[15];            // 12 directos + simple + doble + triple
    char i_type;                // '0' = carpeta, '1' = archivo
    char i_perm[3];             // Permisos UGO (ej. "664")
    
    Inode();  // Declaración del constructor
};

// Superbloque - SOLO DECLARACIÓN
struct Superblock {
    int s_filesystem_type;      // 2 para EXT2
    int s_inodes_count;         // Total de inodos
    int s_blocks_count;         // Total de bloques
    int s_free_blocks_count;    // Bloques libres
    int s_free_inodes_count;    // Inodos libres
    time_t s_mtime;             // Último montaje
    time_t s_umtime;            // Último desmontaje
    int s_mnt_count;            // Veces montado
    int s_magic;                // 0xEF53
    int s_inode_size;           // Tamaño del inodo
    int s_block_size;           // Tamaño del bloque (64)
    int s_first_ino;            // Primer inodo libre
    int s_first_blo;            // Primer bloque libre
    int s_bm_inode_start;       // Inicio bitmap inodos
    int s_bm_block_start;       // Inicio bitmap bloques
    int s_inode_start;          // Inicio tabla inodos
    int s_block_start;          // Inicio tabla bloques
    
    Superblock();  // SOLO DECLARACIÓN, sin implementación aquí
};


struct Content {
    char b_name[12];    // Nombre del archivo/carpeta
    int b_inodo;        // Apuntador al inodo
    
    Content();  // Declaración
};

struct BlockFolder {
    Content b_content[4];  // 4 contenidos (64 bytes)
    
    BlockFolder();  // Declaración
};

struct BlockFile {
    char b_content[64];  // Contenido del archivo
    
    BlockFile();  // Declaración
};

struct BlockPointers {
    int b_pointers[16];  // 16 apuntadores (64 bytes)
    
    BlockPointers();  // Declaración
};

// Para particiones montadas en memoria
struct MountedPartition {
    std::string path;
    std::string name;
    std::string id;
    int start;
    int size;
    bool isMounted;
    
    MountedPartition();  // Declaración
};

#endif