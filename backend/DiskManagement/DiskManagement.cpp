#include "DiskManagement.h"
#include <iostream>
#include <sstream>
#include <random>
#include <algorithm>
#include <climits>

DiskManagement::DiskManagement() {}

bool DiskManagement::openDisk(const std::string& path, std::ios::openmode mode) {
    closeDisk();
    diskFile.open(path, mode);
    currentPath = path;
    return diskFile.is_open();
}

void DiskManagement::closeDisk() {
    if (diskFile.is_open()) {
        diskFile.close();
    }
}

//========== MKDISK ==========
bool DiskManagement::mkdisk(int size, const std::string& unit, const std::string& path, const std::string& fit) {
    std::cout << "Ejecutando MKDISK..." << std::endl;
    
    // Validar parámetros
    if (size <= 0) {
        std::cout << "Error: El tamaño debe ser positivo." << std::endl;
        return false;
    }
    
    std::string fitType = fit.empty() ? "FF" : fit;
    if (!Utilities::isValidFit(fitType)) {
        std::cout << "Error: Tipo de fit no válido." << std::endl;
        return false;
    }
    
    // Crear directorios si no existen
    std::string parentPath = Utilities::getParentPath(path);
    if (!parentPath.empty() && !Utilities::fileExists(parentPath)) {
        if (!Utilities::createDirectories(parentPath)) {
            std::cout << "Error: No se pudieron crear los directorios." << std::endl;
            return false;
        }
    }
    
    // Calcular tamaño en bytes
    int bytes = Utilities::convertToBytes(size, unit);
    
    // Crear archivo
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cout << "Error: No se pudo crear el archivo." << std::endl;
        return false;
    }
    
    // Llenar con ceros (usando buffer de 1024 bytes para velocidad)
    char buffer[1024] = {0};
    int written = 0;
    while (written < bytes) {
        int chunk = std::min(1024, bytes - written);
        file.write(buffer, chunk);
        written += chunk;
    }
    file.close();
    
    // Escribir MBR
    if (!openDisk(path, std::ios::in | std::ios::out | std::ios::binary)) {
        std::cout << "Error: No se pudo abrir el disco para escribir MBR." << std::endl;
        return false;
    }
    
    MBR mbr;
    mbr.mbr_tamano = bytes;
    mbr.mbr_fecha_creacion = time(nullptr);
    
    // Generar signature aleatoria
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 999999);
    mbr.mbr_dsk_signature = dis(gen);
    mbr.dsk_fit = fitType[0];
    
    for (int i = 0; i < 4; i++) {
        mbr.mbr_partitions[i] = Partition();
        mbr.mbr_partitions[i].part_fit = fitType[0];
    }
    
    if (!writeStructure(0, mbr)) {
        std::cout << "Error: No se pudo escribir el MBR." << std::endl;
        closeDisk();
        return false;
    }
    
    closeDisk();
    std::cout << "Disco creado exitosamente: " << path << std::endl;
    std::cout << "Tamaño: " << bytes << " bytes" << std::endl;
    std::cout << "Signature: " << mbr.mbr_dsk_signature << std::endl;
    
    return true;
}

//========== RMDISK ==========
bool DiskManagement::rmdisk(const std::string& path) {
    std::cout << "Ejecutando RMDISK..." << std::endl;
    
    if (!Utilities::fileExists(path)) {
        std::cout << "Error: El disco no existe." << std::endl;
        return false;
    }
    
    if (std::remove(path.c_str()) == 0) {
        std::cout << "Disco eliminado exitosamente." << std::endl;
        return true;
    }
    std::cout << "Error: No se pudo eliminar el disco." << std::endl;
    return false;
}

//========== FDISK ==========
bool DiskManagement::fdisk(int size, const std::string& unit, const std::string& path, 
                          const std::string& type, const std::string& fit, const std::string& name) {
    std::cout << "Ejecutando FDISK..." << std::endl;
    
    if (!Utilities::fileExists(path)) {
        std::cout << "Error: El disco no existe." << std::endl;
        return false;
    }
    if (name.empty() || name.length() > 15) {
        std::cout << "Error: Nombre de partición inválido (máx 15 caracteres)." << std::endl;
        return false;
    }
    std::string t = type.empty() ? "P" : type;
    std::string f = fit.empty() ? "WF" : fit;
    if (!Utilities::isValidType(t) || !Utilities::isValidFit(f)) {
        std::cout << "Error: Tipo o fit no válido." << std::endl;
        return false;
    }
    
    if (!openDisk(path, std::ios::in | std::ios::out | std::ios::binary)) {
        std::cout << "Error: No se pudo abrir el disco." << std::endl;
        return false;
    }
    MBR mbr;
    if (!readStructure(0, mbr)) {
        closeDisk();
        return false;
    }
    
    int bytes = Utilities::convertToBytes(size, unit);
    if (bytes <= 0) {
        std::cout << "Error: El tamaño debe ser positivo." << std::endl;
        closeDisk();
        return false;
    }
    
    // Nombre duplicado
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_size > 0) {
            std::string pname(mbr.mbr_partitions[i].part_name);
            if (pname == name) {
                std::cout << "Error: Ya existe una partición con ese nombre." << std::endl;
                closeDisk();
                return false;
            }
        }
    }
    
    if (t == "E") {
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_type == 'E') {
                std::cout << "Error: Ya existe una partición extendida." << std::endl;
                closeDisk();
                return false;
            }
        }
        if (bytes > mbr.mbr_tamano) {
            std::cout << "Error: La partición excede el tamaño del disco." << std::endl;
            closeDisk();
            return false;
        }
        std::vector<std::pair<int, int>> freeSpaces = getFreeSpaces(path, mbr.mbr_tamano);
        int start = -1;
        if (f == "FF") start = findFirstFit(bytes, freeSpaces);
        else if (f == "BF") start = findBestFit(bytes, freeSpaces);
        else start = findWorstFit(bytes, freeSpaces);
        if (start == -1) {
            std::cout << "Error: No hay espacio suficiente." << std::endl;
            closeDisk();
            return false;
        }
        int slot = -1;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_size == 0) { slot = i; break; }
        }
        if (slot == -1) {
            std::cout << "Error: No hay más slots (máx 4)." << std::endl;
            closeDisk();
            return false;
        }
        Partition newPart;
        newPart.part_status = '0';
        newPart.part_type = 'E';
        newPart.part_fit = f[0];
        newPart.part_start = start;
        newPart.part_size = bytes;
        strncpy(newPart.part_name, name.c_str(), 15);
        newPart.part_name[15] = '\0';
        newPart.part_correlative = -1;
        memset(newPart.part_id, 0, 4);
        mbr.mbr_partitions[slot] = newPart;
        writeStructure(0, mbr);
        EBR firstEbr;
        firstEbr.part_mount = '0';
        firstEbr.part_fit = f[0];
        firstEbr.part_start = start;
        firstEbr.part_size = 0;
        firstEbr.part_next = -1;
        memset(firstEbr.part_name, 0, 16);
        writeStructure(start, firstEbr);
        closeDisk();
        std::cout << "Partición extendida creada: " << name << std::endl;
        return true;
    }
    
    if (t == "L") {
        int extStart = -1, extSize = -1;
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_type == 'E') {
                extStart = mbr.mbr_partitions[i].part_start;
                extSize = mbr.mbr_partitions[i].part_size;
                break;
            }
        }
        if (extStart < 0) {
            std::cout << "Error: No hay partición extendida para crear lógica." << std::endl;
            closeDisk();
            return false;
        }
        if (bytes > extSize - (int)sizeof(EBR)) {
            std::cout << "Error: La partición lógica excede el espacio de la extendida." << std::endl;
            closeDisk();
            return false;
        }
        std::vector<std::pair<int, int>> freeSpaces = getFreeSpacesInExtended(path, extStart, extSize);
        int start = -1;
        if (f == "FF") start = findFirstFit(bytes, freeSpaces);
        else if (f == "BF") start = findBestFit(bytes, freeSpaces);
        else start = findWorstFit(bytes, freeSpaces);
        if (start == -1) {
            std::cout << "Error: No hay espacio suficiente en la extendida." << std::endl;
            closeDisk();
            return false;
        }
        int ebrSize = (int)sizeof(EBR);
        int ebrPos = start - ebrSize;
        EBR newEbr;
        newEbr.part_mount = '0';
        newEbr.part_fit = f[0];
        newEbr.part_start = ebrPos;
        newEbr.part_size = bytes;
        newEbr.part_next = -1;
        strncpy(newEbr.part_name, name.c_str(), 15);
        newEbr.part_name[15] = '\0';
        int currentEbr = extStart;
        EBR ebr;
        readStructure(currentEbr, ebr);
        if (ebr.part_size == 0 && ebr.part_next == -1) {
            ebr.part_size = bytes;
            ebr.part_fit = f[0];
            strncpy(ebr.part_name, name.c_str(), 15);
            ebr.part_name[15] = '\0';
            ebr.part_next = (start + bytes);
            writeStructure(extStart, ebr);
            EBR terminator;
            terminator.part_mount = '0';
            terminator.part_fit = f[0];
            terminator.part_start = start + bytes;
            terminator.part_size = 0;
            terminator.part_next = -1;
            memset(terminator.part_name, 0, 16);
            writeStructure(start + bytes, terminator);
        } else {
            while (true) {
                readStructure(currentEbr, ebr);
                if (ebr.part_next == -1) {
                    ebr.part_next = ebrPos;
                    writeStructure(currentEbr, ebr);
                    break;
                }
                currentEbr = ebr.part_next;
            }
            writeStructure(ebrPos, newEbr);
        }
        closeDisk();
        std::cout << "Partición lógica creada: " << name << std::endl;
        return true;
    }
    
    if (bytes > mbr.mbr_tamano) {
        std::cout << "Error: La partición excede el tamaño del disco." << std::endl;
        closeDisk();
        return false;
    }
    std::vector<std::pair<int, int>> freeSpaces = getFreeSpaces(path, mbr.mbr_tamano);
    int start = -1;
    if (f == "FF") start = findFirstFit(bytes, freeSpaces);
    else if (f == "BF") start = findBestFit(bytes, freeSpaces);
    else start = findWorstFit(bytes, freeSpaces);
    if (start == -1) {
        std::cout << "Error: No hay espacio suficiente para la partición." << std::endl;
        closeDisk();
        return false;
    }
    int slot = -1;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_size == 0) { slot = i; break; }
    }
    if (slot == -1) {
        std::cout << "Error: No hay más slots de partición disponibles (máx 4)." << std::endl;
        closeDisk();
        return false;
    }
    Partition newPart;
    newPart.part_status = '0';
    newPart.part_type = 'P';
    newPart.part_fit = f[0];
    newPart.part_start = start;
    newPart.part_size = bytes;
    strncpy(newPart.part_name, name.c_str(), 15);
    newPart.part_name[15] = '\0';
    newPart.part_correlative = -1;
    memset(newPart.part_id, 0, 4);
    mbr.mbr_partitions[slot] = newPart;
    if (!writeStructure(0, mbr)) {
        closeDisk();
        return false;
    }
    closeDisk();
    std::cout << "Partición primaria creada: " << name << std::endl;
    return true;
}

//========== MOUNT (actualizar partición en disco) ==========
bool DiskManagement::updatePartitionMount(const std::string& path, const std::string& name, const std::string& id, int& outStart, int& outSize) {
    if (!Utilities::fileExists(path)) {
        std::cout << "Error: El disco no existe." << std::endl;
        return false;
    }
    if (!openDisk(path, std::ios::in | std::ios::out | std::ios::binary)) {
        std::cout << "Error: No se pudo abrir el disco." << std::endl;
        return false;
    }
    MBR mbr;
    if (!readStructure(0, mbr)) {
        closeDisk();
        return false;
    }
    int partCorrelative = 1;
    if (id.length() >= 4)
        partCorrelative = std::stoi(id.substr(2, id.length() - 3));
    for (int i = 0; i < 4; i++) {
        Partition& p = mbr.mbr_partitions[i];
        if (p.part_size > 0 && p.part_type == 'P') {
            std::string pname(p.part_name);
            if (pname == name) {
                p.part_status = '1';
                p.part_correlative = partCorrelative;
                memset(p.part_id, 0, 4);
                for (size_t j = 0; j < 4 && j < id.length(); j++) p.part_id[j] = id[j];
                outStart = p.part_start;
                outSize = p.part_size;
                if (!writeStructure(0, mbr)) {
                    closeDisk();
                    return false;
                }
                closeDisk();
                return true;
            }
        }
    }
    std::cout << "Error: No se encontró la partición primaria con nombre '" << name << "'." << std::endl;
    closeDisk();
    return false;
}

//========== MKFS ==========
bool DiskManagement::mkfs(const std::string& id, const std::string& type, const std::string& diskPath, int partStart, int partSize) {
    std::cout << "Ejecutando MKFS en partición " << id << "..." << std::endl;
    if (!Utilities::fileExists(diskPath)) {
        std::cout << "Error: Disco no existe." << std::endl;
        return false;
    }
    return formatExt2(diskPath, partStart, partSize);
}

//========== FORMAT EXT2 ==========
bool DiskManagement::formatExt2(const std::string& path, int start, int size) {
    if (!openDisk(path, std::ios::in | std::ios::out | std::ios::binary)) return false;
    char zeroBuf[1024] = {0};
    for (int offset = 0; offset < size; offset += 1024) {
        diskFile.seekp(start + offset, std::ios::beg);
        diskFile.write(zeroBuf, std::min(1024, size - offset));
    }
    diskFile.flush();
    int n = Utilities::calculateInodes(size);
    int nBlocks = Utilities::calculateBlocks(n);
    int sbSize = (int)sizeof(Superblock);
    int inodeSize = (int)sizeof(Inode);
    int blockSize = 64;
    int bmInodeSize = Utilities::calculateBitmapSize(n);
    int bmBlockSize = Utilities::calculateBitmapSize(nBlocks);
    int sb_start = start;
    int bm_inode_start = sb_start + sbSize;
    int bm_block_start = bm_inode_start + bmInodeSize;
    int inode_start = bm_block_start + bmBlockSize;
    int block_start = inode_start + n * inodeSize;
    Superblock sb;
    sb.s_filesystem_type = 2;
    sb.s_inodes_count = n;
    sb.s_blocks_count = nBlocks;
    sb.s_free_blocks_count = nBlocks - 2;
    sb.s_free_inodes_count = n - 2;
    sb.s_mtime = time(nullptr);
    sb.s_umtime = 0;
    sb.s_mnt_count = 0;
    sb.s_magic = 0xEF53;
    sb.s_inode_size = inodeSize;
    sb.s_block_size = blockSize;
    sb.s_first_ino = 2;
    sb.s_first_blo = 2;
    sb.s_bm_inode_start = bm_inode_start;
    sb.s_bm_block_start = bm_block_start;
    sb.s_inode_start = inode_start;
    sb.s_block_start = block_start;
    writeStructure(sb_start, sb);
    char* bmInode = new char[bmInodeSize];
    memset(bmInode, 0, bmInodeSize);
    Utilities::setBit(bmInode, 0, true);
    Utilities::setBit(bmInode, 1, true);
    diskFile.seekp(bm_inode_start, std::ios::beg);
    diskFile.write(bmInode, bmInodeSize);
    delete[] bmInode;
    char* bmBlock = new char[bmBlockSize];
    memset(bmBlock, 0, bmBlockSize);
    Utilities::setBit(bmBlock, 0, true);
    Utilities::setBit(bmBlock, 1, true);
    diskFile.seekp(bm_block_start, std::ios::beg);
    diskFile.write(bmBlock, bmBlockSize);
    delete[] bmBlock;
    Inode rootInode;
    rootInode.i_uid = 1;
    rootInode.i_gid = 1;
    rootInode.i_size = 0;
    rootInode.i_atime = rootInode.i_ctime = rootInode.i_mtime = time(nullptr);
    rootInode.i_block[0] = 0;
    for (int i = 1; i < 15; i++) rootInode.i_block[i] = -1;
    rootInode.i_type = '0';
    rootInode.i_perm[0] = '6'; rootInode.i_perm[1] = '6'; rootInode.i_perm[2] = '4';
    writeStructure(inode_start, rootInode);
    std::string usersContent = "1,G,root\n1,U,root,root,123\n";
    Inode usersInode;
    usersInode.i_uid = 1;
    usersInode.i_gid = 1;
    usersInode.i_size = (int)usersContent.length();
    usersInode.i_atime = usersInode.i_ctime = usersInode.i_mtime = time(nullptr);
    usersInode.i_block[0] = 1;
    for (int i = 1; i < 15; i++) usersInode.i_block[i] = -1;
    usersInode.i_type = '1';
    usersInode.i_perm[0] = '6'; usersInode.i_perm[1] = '6'; usersInode.i_perm[2] = '4';
    writeStructure(inode_start + inodeSize, usersInode);
    BlockFolder rootBlock;
    rootBlock.b_content[0] = Content();
    strncpy(rootBlock.b_content[0].b_name, ".", 12);
    rootBlock.b_content[0].b_inodo = 0;
    rootBlock.b_content[1] = Content();
    strncpy(rootBlock.b_content[1].b_name, "..", 12);
    rootBlock.b_content[1].b_inodo = 0;
    rootBlock.b_content[2] = Content();
    strncpy(rootBlock.b_content[2].b_name, "users.txt", 12);
    rootBlock.b_content[2].b_inodo = 1;
    rootBlock.b_content[3] = Content();
    rootBlock.b_content[3].b_inodo = -1;
    writeStructure(block_start, rootBlock);
    BlockFile usersBlock;
    memset(usersBlock.b_content, 0, 64);
    for (size_t i = 0; i < usersContent.length() && i < 64; i++) usersBlock.b_content[i] = usersContent[i];
    writeStructure(block_start + blockSize, usersBlock);
    diskFile.flush();
    closeDisk();
    std::cout << "Partición formateada como EXT2. Inodos: " << n << ", Bloques: " << nBlocks << std::endl;
    return true;
}

bool DiskManagement::validateUser(const std::string& path, int partStart, int partSize, const std::string& user, const std::string& pass) {
    if (!openDisk(path, std::ios::in | std::ios::binary)) return false;
    int n = Utilities::calculateInodes(partSize);
    int nBlocks = Utilities::calculateBlocks(n);
    int sbSize = (int)sizeof(Superblock);
    int inodeSize = (int)sizeof(Inode);
    int blockSize = 64;
    int bmInodeSize = Utilities::calculateBitmapSize(n);
    int bmBlockSize = Utilities::calculateBitmapSize(nBlocks);
    int inode_start = partStart + sbSize + bmInodeSize + bmBlockSize;
    int block_start = inode_start + n * inodeSize;
    Inode ino;
    readStructure(inode_start + inodeSize, ino);
    std::string content;
    if (ino.i_block[0] >= 0) {
        BlockFile bf;
        readStructure(block_start + ino.i_block[0] * blockSize, bf);
        for (int i = 0; i < 64 && bf.b_content[i]; i++) content += bf.b_content[i];
    }
    closeDisk();
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string p;
        while (std::getline(ss, p, ',')) parts.push_back(p);
        if (parts.size() >= 5 && parts[1] == "U") {
            int uid = std::stoi(parts[0]);
            if (uid != 0 && parts[3] == user && parts[4] == pass) return true;
        }
    }
    return false;
}

std::string DiskManagement::readUsersFile(const std::string& path, int partStart, int partSize) {
    if (!openDisk(path, std::ios::in | std::ios::binary)) return "";
    int n = Utilities::calculateInodes(partSize);
    int nBlocks = Utilities::calculateBlocks(n);
    int sbSize = (int)sizeof(Superblock);
    int inodeSize = (int)sizeof(Inode);
    int blockSize = 64;
    int bmInodeSize = Utilities::calculateBitmapSize(n);
    int bmBlockSize = Utilities::calculateBitmapSize(nBlocks);
    int inode_start = partStart + sbSize + bmInodeSize + bmBlockSize;
    int block_start = inode_start + n * inodeSize;
    Inode ino;
    readStructure(inode_start + inodeSize, ino);
    std::string content;
    int totalRead = 0;
    for (int i = 0; i < 12 && ino.i_block[i] >= 0 && totalRead < ino.i_size; i++) {
        BlockFile bf;
        readStructure(block_start + ino.i_block[i] * blockSize, bf);
        int toRead = std::min(64, ino.i_size - totalRead);
        for (int j = 0; j < toRead; j++) content += bf.b_content[j];
        totalRead += toRead;
    }
    closeDisk();
    return content;
}

bool DiskManagement::writeUsersFile(const std::string& path, int partStart, int partSize, const std::string& content) {
    if (!openDisk(path, std::ios::in | std::ios::out | std::ios::binary)) return false;
    int n = Utilities::calculateInodes(partSize);
    int nBlocks = Utilities::calculateBlocks(n);
    int sbSize = (int)sizeof(Superblock);
    int inodeSize = (int)sizeof(Inode);
    int blockSize = 64;
    int bmInodeSize = Utilities::calculateBitmapSize(n);
    int bmBlockSize = Utilities::calculateBitmapSize(nBlocks);
    int sb_start = partStart;
    int bm_block_start = partStart + sbSize + bmInodeSize;
    int inode_start = bm_block_start + bmBlockSize;
    int block_start = inode_start + n * inodeSize;
    Superblock sb;
    readStructure(sb_start, sb);
    Inode ino;
    readStructure(inode_start + inodeSize, ino);
    int numBlocks = (int)((content.length() + blockSize - 1) / blockSize);
    if (numBlocks > 12) { closeDisk(); return false; }
    char* bm = new char[bmBlockSize];
    diskFile.seekg(bm_block_start);
    diskFile.read(bm, bmBlockSize);
    for (int i = 0; i < numBlocks; i++) {
        if (ino.i_block[i] < 0) {
            int freeIdx = Utilities::findFreeBit(bm, nBlocks);
            if (freeIdx < 0) { delete[] bm; closeDisk(); return false; }
            ino.i_block[i] = freeIdx;
            Utilities::setBit(bm, freeIdx, true);
            sb.s_free_blocks_count--;
        }
        BlockFile bf;
        memset(bf.b_content, 0, 64);
        int start = i * blockSize;
        for (int j = 0; j < blockSize && start + j < (int)content.length(); j++)
            bf.b_content[j] = content[start + j];
        writeStructure(block_start + ino.i_block[i] * blockSize, bf);
    }
    diskFile.seekp(bm_block_start);
    diskFile.write(bm, bmBlockSize);
    delete[] bm;
    ino.i_size = (int)content.length();
    ino.i_mtime = time(nullptr);
    writeStructure(inode_start + inodeSize, ino);
    writeStructure(sb_start, sb);
    closeDisk();
    return true;
}

//========== REPORTES ==========
bool DiskManagement::rep(const std::string& name, const std::string& outputPath,
                        const std::string& diskPath, int partStart, int partSize,
                        const std::string& path_file_ls) {
    std::cout << "Generando reporte " << name << "..." << std::endl;
    std::string parentPath = Utilities::getParentPath(outputPath);
    if (!parentPath.empty() && !Utilities::fileExists(parentPath)) Utilities::createDirectories(parentPath);
    if (name == "mbr") {
        if (!openDisk(diskPath, std::ios::in | std::ios::binary)) return false;
        MBR mbr;
        if (!readStructure(0, mbr)) { closeDisk(); return false; }
        closeDisk();
        std::ofstream dotFile(outputPath + ".dot");
        dotFile << "digraph MBR { node [shape=plaintext]; mbr [label=<\n<table border='1'>\n";
        dotFile << "<tr><td colspan='2'>MBR</td></tr>\n<tr><td>mbr_tamano</td><td>" << mbr.mbr_tamano << "</td></tr>\n";
        dotFile << "<tr><td>mbr_fecha_creacion</td><td>" << Utilities::timeToString(mbr.mbr_fecha_creacion) << "</td></tr>\n";
        dotFile << "<tr><td>mbr_dsk_signature</td><td>" << mbr.mbr_dsk_signature << "</td></tr>\n<tr><td>dsk_fit</td><td>" << mbr.dsk_fit << "</td></tr>\n";
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_size > 0) {
                dotFile << "<tr><td>part" << (i+1) << "_name</td><td>" << mbr.mbr_partitions[i].part_name << "</td></tr>\n";
                dotFile << "<tr><td>part" << (i+1) << "_size</td><td>" << mbr.mbr_partitions[i].part_size << "</td></tr>\n";
            }
        }
        dotFile << "</table>>]; }" << std::endl;
        dotFile.close();
        system(("dot -Tjpg \"" + outputPath + ".dot\" -o \"" + outputPath + "\" 2>/dev/null || true").c_str());
        return true;
    }
    if (name == "sb") {
        if (!openDisk(diskPath, std::ios::in | std::ios::binary)) return false;
        Superblock sb;
        if (!readStructure(partStart, sb)) { closeDisk(); return false; }
        closeDisk();
        std::ofstream dotFile(outputPath + ".dot");
        dotFile << "digraph SB { node [shape=plaintext]; sb [label=<\n<table border='1'>\n";
        dotFile << "<tr><td>s_filesystem_type</td><td>" << sb.s_filesystem_type << "</td></tr>\n";
        dotFile << "<tr><td>s_inodes_count</td><td>" << sb.s_inodes_count << "</td></tr>\n";
        dotFile << "<tr><td>s_blocks_count</td><td>" << sb.s_blocks_count << "</td></tr>\n";
        dotFile << "<tr><td>s_magic</td><td>0x" << std::hex << sb.s_magic << std::dec << "</td></tr>\n</table>>]; }" << std::endl;
        dotFile.close();
        system(("dot -Tjpg \"" + outputPath + ".dot\" -o \"" + outputPath + "\" 2>/dev/null || true").c_str());
        return true;
    }
    if (name == "bm_inode" || name == "bm_block") {
        if (!openDisk(diskPath, std::ios::in | std::ios::binary)) return false;
        int n = Utilities::calculateInodes(partSize);
        int nBlocks = Utilities::calculateBlocks(n);
        int sbSize = (int)sizeof(Superblock);
        int bmInodeSize = Utilities::calculateBitmapSize(n);
        int bmBlockSize = Utilities::calculateBitmapSize(nBlocks);
        int bm_inode_start = partStart + sbSize;
        int bm_block_start = bm_inode_start + bmInodeSize;
        char* bm = name == "bm_inode" ? new char[bmInodeSize] : new char[bmBlockSize];
        int bits = name == "bm_inode" ? n : nBlocks;
        int bmStart = name == "bm_inode" ? bm_inode_start : bm_block_start;
        int bmLen = name == "bm_inode" ? bmInodeSize : bmBlockSize;
        diskFile.seekg(bmStart);
        diskFile.read(bm, bmLen);
        closeDisk();
        std::ofstream txt(outputPath);
        for (int i = 0; i < bits; i++) {
            txt << (Utilities::getBit(bm, i) ? '1' : '0');
            if ((i+1) % 20 == 0) txt << '\n';
        }
        txt << '\n';
        txt.close();
        delete[] bm;
        return true;
    }
    return true;
}

// Funciones auxiliares para búsqueda de espacios
std::vector<std::pair<int, int>> DiskManagement::getFreeSpaces(const std::string& path, int diskSize) {
    std::vector<std::pair<int, int>> freeSpaces;
    
    if (!openDisk(path, std::ios::in | std::ios::binary)) {
        return freeSpaces;
    }
    
    MBR mbr;
    if (!readStructure(0, mbr)) {
        closeDisk();
        return freeSpaces;
    }
    
    // Ordenar particiones por start
    std::vector<Partition> partitions;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_size > 0) {
            partitions.push_back(mbr.mbr_partitions[i]);
        }
    }
    
    std::sort(partitions.begin(), partitions.end(), 
              [](const Partition& a, const Partition& b) {
                  return a.part_start < b.part_start;
              });
    
    int currentPos = sizeof(MBR); // Después del MBR
    
    for (const auto& p : partitions) {
        if (p.part_start > currentPos) {
            freeSpaces.push_back({currentPos, p.part_start - currentPos});
        }
        currentPos = p.part_start + p.part_size;
    }
    
    if (currentPos < diskSize) {
        freeSpaces.push_back({currentPos, diskSize - currentPos});
    }
    
    closeDisk();
    return freeSpaces;
}

std::vector<std::pair<int, int>> DiskManagement::getFreeSpacesInExtended(const std::string& path, int extStart, int extSize) {
    std::vector<std::pair<int, int>> freeSpaces;
    if (!openDisk(path, std::ios::in | std::ios::binary)) return freeSpaces;
    int current = extStart;
    int ebrSize = (int)sizeof(EBR);
    while (current != -1 && current >= extStart && current < extStart + extSize) {
        EBR ebr;
        if (!readStructure(current, ebr)) break;
        int dataEnd = current + ebrSize + ebr.part_size;
        if (ebr.part_next == -1) {
            int freeStart = dataEnd;
            int freeSize = (extStart + extSize) - freeStart;
            if (freeSize > 0)
                freeSpaces.push_back({freeStart, freeSize});
            break;
        }
        int freeStart = dataEnd + ebrSize;
        int freeSize = ebr.part_next - dataEnd - ebrSize;
        if (freeSize > 0)
            freeSpaces.push_back({freeStart, freeSize});
        current = ebr.part_next;
    }
    closeDisk();
    return freeSpaces;
}

int DiskManagement::findFirstFit(int size, const std::vector<std::pair<int, int>>& freeSpaces) {
    for (const auto& space : freeSpaces) {
        if (space.second >= size) {
            return space.first;
        }
    }
    return -1;
}

int DiskManagement::findBestFit(int size, const std::vector<std::pair<int, int>>& freeSpaces) {
    int bestStart = -1;
    int smallestWaste = INT_MAX;
    
    for (const auto& space : freeSpaces) {
        if (space.second >= size) {
            int waste = space.second - size;
            if (waste < smallestWaste) {
                smallestWaste = waste;
                bestStart = space.first;
            }
        }
    }
    
    return bestStart;
}

int DiskManagement::findWorstFit(int size, const std::vector<std::pair<int, int>>& freeSpaces) {
    int worstStart = -1;
    int largestSpace = -1;
    
    for (const auto& space : freeSpaces) {
        if (space.second >= size && space.second > largestSpace) {
            largestSpace = space.second;
            worstStart = space.first;
        }
    }
    
    return worstStart;
}

// Implementaciones mínimas de otros comandos
bool DiskManagement::mkgrp(const std::string& path, int partStart, int partSize, const std::string& name) {
    if (name.length() > 10) { std::cout << "Error: Nombre de grupo máx 10 caracteres." << std::endl; return false; }
    std::string content = readUsersFile(path, partStart, partSize);
    std::istringstream iss(content);
    std::string line;
    int maxGid = 0;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string p;
        while (std::getline(ss, p, ',')) parts.push_back(p);
        if (parts.size() >= 3 && parts[1] == "G") {
            int gid = std::stoi(parts[0]);
            if (gid > maxGid) maxGid = gid;
            if (parts[2] == name && gid != 0) { std::cout << "Error: El grupo ya existe." << std::endl; return false; }
        }
    }
    content += std::to_string(maxGid + 1) + ",G," + name + "\n";
    return writeUsersFile(path, partStart, partSize, content);
}

bool DiskManagement::rmgrp(const std::string& path, int partStart, int partSize, const std::string& name) {
    std::string content = readUsersFile(path, partStart, partSize);
    std::ostringstream out;
    std::istringstream iss(content);
    std::string line;
    bool found = false;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string p;
        while (std::getline(ss, p, ',')) parts.push_back(p);
        if (parts.size() >= 3 && parts[1] == "G" && parts[2] == name) {
            out << "0,G," << name << "\n";
            found = true;
        } else
            out << line << "\n";
    }
    if (!found) { std::cout << "Error: Grupo no encontrado." << std::endl; return false; }
    return writeUsersFile(path, partStart, partSize, out.str());
}

bool DiskManagement::mkusr(const std::string& path, int partStart, int partSize, const std::string& user, const std::string& pass, const std::string& grp) {
    if (user.length() > 10 || pass.length() > 10 || grp.length() > 10) {
        std::cout << "Error: Usuario/contraseña/grupo máx 10 caracteres." << std::endl;
        return false;
    }
    std::string content = readUsersFile(path, partStart, partSize);
    std::istringstream iss(content);
    std::string line;
    int maxUid = 0;
    bool grpExists = false;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string p;
        while (std::getline(ss, p, ',')) parts.push_back(p);
        if (parts.size() >= 3 && parts[1] == "G" && parts[2] == grp && std::stoi(parts[0]) != 0) grpExists = true;
        if (parts.size() >= 5 && parts[1] == "U") {
            int uid = std::stoi(parts[0]);
            if (uid > maxUid) maxUid = uid;
            if (parts[3] == user) { std::cout << "Error: El usuario ya existe." << std::endl; return false; }
        }
    }
    if (!grpExists) { std::cout << "Error: El grupo no existe." << std::endl; return false; }
    content += std::to_string(maxUid + 1) + ",U," + grp + "," + user + "," + pass + "\n";
    return writeUsersFile(path, partStart, partSize, content);
}

bool DiskManagement::rmusr(const std::string& path, int partStart, int partSize, const std::string& user) {
    std::string content = readUsersFile(path, partStart, partSize);
    std::ostringstream out;
    std::istringstream iss(content);
    std::string line;
    bool found = false;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string p;
        while (std::getline(ss, p, ',')) parts.push_back(p);
        if (parts.size() >= 5 && parts[1] == "U" && parts[3] == user) {
            out << "0,U," << parts[2] << "," << user << "," << parts[4] << "\n";
            found = true;
        } else
            out << line << "\n";
    }
    if (!found) { std::cout << "Error: Usuario no encontrado." << std::endl; return false; }
    return writeUsersFile(path, partStart, partSize, out.str());
}

bool DiskManagement::chgrp(const std::string& path, int partStart, int partSize, const std::string& user, const std::string& grp) {
    std::string content = readUsersFile(path, partStart, partSize);
    bool grpExists = false;
    std::istringstream iss2(content);
    std::string l;
    while (std::getline(iss2, l)) {
        std::vector<std::string> parts;
        std::stringstream ss(l);
        std::string p;
        while (std::getline(ss, p, ',')) parts.push_back(p);
        if (parts.size() >= 3 && parts[1] == "G" && parts[2] == grp && std::stoi(parts[0]) != 0) grpExists = true;
    }
    if (!grpExists) { std::cout << "Error: El grupo no existe o está eliminado." << std::endl; return false; }
    std::ostringstream out;
    std::istringstream iss(content);
    std::string line;
    bool found = false;
    while (std::getline(iss, line)) {
        if (line.empty()) continue;
        std::vector<std::string> parts;
        std::stringstream ss(line);
        std::string p;
        while (std::getline(ss, p, ',')) parts.push_back(p);
        if (parts.size() >= 5 && parts[1] == "U" && parts[3] == user) {
            out << parts[0] << ",U," << grp << "," << user << "," << parts[4] << "\n";
            found = true;
        } else
            out << line << "\n";
    }
    if (!found) { std::cout << "Error: Usuario no encontrado." << std::endl; return false; }
    return writeUsersFile(path, partStart, partSize, out.str());
}

bool DiskManagement::mkfile(const std::string& path, bool recursive, int size, const std::string& cont) {
    std::cout << "Ejecutando MKFILE..." << std::endl;
    return true;
}

bool DiskManagement::mkdir(const std::string& path, bool recursive) {
    std::cout << "Ejecutando MKDIR..." << std::endl;
    return true;
}

bool DiskManagement::cat(const std::string& diskPath, int partStart, int partSize, const std::vector<std::string>& files, std::string& output) {
    output.clear();
    for (const std::string& f : files) {
        if (f == "/users.txt" || f == "users.txt") {
            output += readUsersFile(diskPath, partStart, partSize);
            output += "\n";
        } else {
            output += "Error: No se pudo leer '" + f + "' (solo se admite /users.txt en esta versión).\n";
        }
    }
    return true;
}