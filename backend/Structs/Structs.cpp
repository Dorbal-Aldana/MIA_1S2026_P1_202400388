#include "Structs.h"
#include <cstring>

Partition::Partition() {
    part_status = '0';
    part_type = 'P';
    part_fit = 'F';
    part_start = 0;
    part_size = 0;
    memset(part_name, 0, 16);
    part_correlative = -1;
    memset(part_id, 0, 4);
}

MBR::MBR() {
    mbr_tamano = 0;
    mbr_fecha_creacion = 0;
    mbr_dsk_signature = 0;
    dsk_fit = 'F';
    for (int i = 0; i < 4; i++) mbr_partitions[i] = Partition();
}

EBR::EBR() {
    part_mount = '0';
    part_fit = 'F';
    part_start = 0;
    part_size = 0;
    part_next = -1;
    memset(part_name, 0, 16);
}

Inode::Inode() {
    i_uid = 0;
    i_gid = 0;
    i_size = 0;
    i_atime = 0;
    i_ctime = 0;
    i_mtime = 0;
    for (int i = 0; i < 15; i++) i_block[i] = -1;
    i_type = 0;
    memset(i_perm, 0, 3);
}

Superblock::Superblock() {
    s_filesystem_type = 0;
    s_inodes_count = 0;
    s_blocks_count = 0;
    s_free_blocks_count = 0;
    s_free_inodes_count = 0;
    s_mtime = 0;
    s_umtime = 0;
    s_mnt_count = 0;
    s_magic = 0;
    s_inode_size = sizeof(Inode);
    s_block_size = 64;
    s_first_ino = 0;
    s_first_blo = 0;
    s_bm_inode_start = 0;
    s_bm_block_start = 0;
    s_inode_start = 0;
    s_block_start = 0;
}

Content::Content() {
    memset(b_name, 0, 12);
    b_inodo = -1;
}

BlockFolder::BlockFolder() {
    for (int i = 0; i < 4; i++) b_content[i] = Content();
}

BlockFile::BlockFile() {
    memset(b_content, 0, 64);
}

BlockPointers::BlockPointers() {
    for (int i = 0; i < 16; i++) b_pointers[i] = -1;
}

MountedPartition::MountedPartition() {
    start = 0;
    size = 0;
    isMounted = false;
}
