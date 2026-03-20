#include "DiskManagement.h"
#include "DiskManagementImpl.h"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>
#include <random>
#include <algorithm>
#include <climits>
#include <functional>

namespace detail {

template <typename T>
bool readAt(std::fstream& f, int pos, T& t) {
    f.clear();
    f.seekg(pos);
    f.read(reinterpret_cast<char*>(&t), sizeof(T));
    return static_cast<bool>(f) && f.gcount() == static_cast<std::streamsize>(sizeof(T));
}

// Espacio libre solo desde MBR en memoria (no cerrar el disco abierto en fdisk).
std::vector<std::pair<int, int>> freeSpacesFromMbr(const MBR& mbr, int diskSize) {
    std::vector<std::pair<int, int>> freeSpaces;
    std::vector<Partition> partitions;
    for (int i = 0; i < 4; i++) {
        if (mbr.mbr_partitions[i].part_size > 0) partitions.push_back(mbr.mbr_partitions[i]);
    }
    std::sort(partitions.begin(), partitions.end(), [](const Partition& a, const Partition& b) {
        return a.part_start < b.part_start;
    });
    int currentPos = sizeof(MBR);
    for (const auto& p : partitions) {
        if (p.part_start > currentPos) freeSpaces.push_back({currentPos, p.part_start - currentPos});
        currentPos = p.part_start + p.part_size;
    }
    if (currentPos < diskSize) freeSpaces.push_back({currentPos, diskSize - currentPos});
    return freeSpaces;
}

// Cadena EBR usando el mismo fstream que fdisk (no abrir/cerrar otro handle).
std::vector<std::pair<int, int>> freeSpacesInExtendedFile(std::fstream& f, int extStart, int extSize) {
    std::vector<std::pair<int, int>> freeSpaces;
    int current = extStart;
    int ebrSize = static_cast<int>(sizeof(EBR));
    while (current != -1 && current >= extStart && current < extStart + extSize) {
        EBR ebr;
        if (!readAt(f, current, ebr)) break;
        int dataEnd = current + ebrSize + ebr.part_size;
        if (ebr.part_next == -1) {
            int freeStart = dataEnd;
            int freeSize = (extStart + extSize) - freeStart;
            if (freeSize > 0) freeSpaces.push_back({freeStart, freeSize});
            break;
        }
        int freeStart = dataEnd + ebrSize;
        int freeSize = ebr.part_next - dataEnd - ebrSize;
        if (freeSize > 0) freeSpaces.push_back({freeStart, freeSize});
        current = ebr.part_next;
    }
    return freeSpaces;
}

}  // namespace detail

bool DiskManagementImpl::openDisk(const std::string& path, std::ios::openmode mode) {
    closeDisk();
    std::string p = Utilities::sanitizeHostPath(path);
    if (p.empty()) return false;
    diskFile.open(p, mode);
    currentPath = p;
    return diskFile.is_open();
}

void DiskManagementImpl::closeDisk() {
    if (diskFile.is_open()) {
        diskFile.close();
    }
}

//========== MKDISK ==========
bool DiskManagementImpl::mkdisk(int size, const std::string& unit, const std::string& path, const std::string& fit) {
    std::cout << "Ejecutando MKDISK..." << std::endl;
    std::string diskPath = Utilities::sanitizeHostPath(path);
    if (diskPath.empty()) {
        std::cout << "Error: -path inválido (revisa comillas o espacios)." << std::endl;
        return false;
    }

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
    
    // Calcular tamaño en bytes
    int bytes = Utilities::convertToBytes(size, unit);

    // Patrón CLASE7: CreateFile + OpenFile
    if (!Utilities::CreateFile(diskPath)) {
        std::cout << "Error: No se pudo crear el archivo del disco (permisos o ruta incorrecta). Sustituye su_usuario por tu usuario real." << std::endl;
        return false;
    }
    std::fstream file = Utilities::OpenFile(diskPath);
    if (!file.is_open()) {
        std::cout << "Error: No se pudo abrir el disco para inicializarlo." << std::endl;
        return false;
    }

    // Llenar con ceros (buffer de 1024 bytes)
    char buffer[1024] = {0};
    int written = 0;
    while (written < bytes) {
        int chunk = std::min(1024, bytes - written);
        file.write(buffer, chunk);
        written += chunk;
    }
    file.close();
    
    // Escribir MBR
    if (!openDisk(diskPath, std::ios::in | std::ios::out | std::ios::binary)) {
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
    std::cout << "Disco creado exitosamente: " << diskPath << std::endl;
    std::cout << "Tamaño: " << bytes << " bytes" << std::endl;
    std::cout << "Signature: " << mbr.mbr_dsk_signature << std::endl;
    
    return true;
}

//========== RMDISK ==========
bool DiskManagementImpl::rmdisk(const std::string& path) {
    std::cout << "Ejecutando RMDISK..." << std::endl;
    std::string p = Utilities::sanitizeHostPath(path);
    if (p.empty()) {
        std::cout << "Error: Ruta inválida." << std::endl;
        return false;
    }
    if (!Utilities::fileExists(p)) {
        std::cout << "Error: El disco no existe." << std::endl;
        return false;
    }
    
    if (std::remove(p.c_str()) == 0) {
        std::cout << "Disco eliminado exitosamente." << std::endl;
        return true;
    }
    std::cout << "Error: No se pudo eliminar el disco." << std::endl;
    return false;
}

//========== FDISK ==========
bool DiskManagementImpl::fdisk(int size, const std::string& unit, const std::string& path, 
                          const std::string& type, const std::string& fit, const std::string& name) {
    std::cout << "Ejecutando FDISK..." << std::endl;
    std::string dpath = Utilities::sanitizeHostPath(path);
    if (dpath.empty()) {
        std::cout << "Error: Ruta de disco inválida." << std::endl;
        return false;
    }

    if (!Utilities::fileExists(dpath)) {
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
    
    if (!openDisk(dpath, std::ios::in | std::ios::out | std::ios::binary)) {
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
        std::vector<std::pair<int, int>> freeSpaces = detail::freeSpacesFromMbr(mbr, mbr.mbr_tamano);
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
        std::vector<std::pair<int, int>> freeSpaces = detail::freeSpacesInExtendedFile(diskFile, extStart, extSize);
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
    std::vector<std::pair<int, int>> freeSpaces = detail::freeSpacesFromMbr(mbr, mbr.mbr_tamano);
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
bool DiskManagementImpl::updatePartitionMount(const std::string& path, const std::string& name, const std::string& id, int& outStart, int& outSize) {
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
bool DiskManagementImpl::mkfs(const std::string& id, const std::string& type, const std::string& diskPath, int partStart, int partSize) {
    std::cout << "Ejecutando MKFS en partición " << id << "..." << std::endl;
    if (!Utilities::fileExists(diskPath)) {
        std::cout << "Error: Disco no existe." << std::endl;
        return false;
    }
    return formatExt2(diskPath, partStart, partSize);
}

//========== FORMAT EXT2 ==========
bool DiskManagementImpl::formatExt2(const std::string& path, int start, int size) {
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

bool DiskManagementImpl::validateUser(const std::string& path, int partStart, int partSize, const std::string& user, const std::string& pass) {
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

std::string DiskManagementImpl::readUsersFile(const std::string& path, int partStart, int partSize) {
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

bool DiskManagementImpl::writeUsersFile(const std::string& path, int partStart, int partSize, const std::string& content) {
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

//========== EXT2 (Ext2Ops) ==========
struct Ext2Ops {
    static bool readInode(DiskManagementImpl& d, int inode_start, int inodeSize, int idx, Inode& ino) {
        return d.readStructure(inode_start + idx * inodeSize, ino);
    }
    static bool writeInode(DiskManagementImpl& d, int inode_start, int inodeSize, int idx, const Inode& ino) {
        return d.writeStructure(inode_start + idx * inodeSize, ino);
    }
    static std::vector<std::string> splitVirt(const std::string& path) {
        std::vector<std::string> v;
        std::stringstream ss(path);
        std::string p;
        while (std::getline(ss, p, '/'))
            if (!p.empty()) v.push_back(p);
        return v;
    }
    static bool findInFolder(DiskManagementImpl& d, int block_start, int blockSize, const Inode& folder,
                             const std::string& name, int& outChild) {
        for (int bi = 0; bi < 12; bi++) {
            if (folder.i_block[bi] < 0) continue;
            BlockFolder bf;
            if (!d.readStructure(block_start + folder.i_block[bi] * blockSize, bf)) continue;
            for (int j = 0; j < 4; j++) {
                if (bf.b_content[j].b_inodo < 0) continue;
                if (name == std::string(bf.b_content[j].b_name)) {
                    outChild = bf.b_content[j].b_inodo;
                    return true;
                }
            }
        }
        return false;
    }
    static bool allocInode(char* bm, int n, Superblock& sb, int& outIdx) {
        outIdx = Utilities::findFreeBit(bm, n);
        if (outIdx < 0) return false;
        Utilities::setBit(bm, outIdx, true);
        sb.s_free_inodes_count--;
        return true;
    }
    static bool allocBlock(char* bm, int nBlocks, Superblock& sb, int& outIdx) {
        outIdx = Utilities::findFreeBit(bm, nBlocks);
        if (outIdx < 0) return false;
        Utilities::setBit(bm, outIdx, true);
        sb.s_free_blocks_count--;
        return true;
    }
    static bool addEntry(DiskManagementImpl& d, int inode_start, int inodeSize, int block_start, int blockSize,
                         char* bmBlock, int nBlocks, Superblock& sb, Inode& parent, const std::string& name,
                         int childIno) {
        char nbuf[12] = {0};
        strncpy(nbuf, name.c_str(), 11);
        for (int bi = 0; bi < 12; bi++) {
            if (parent.i_block[bi] < 0) {
                int nb = -1;
                if (!allocBlock(bmBlock, nBlocks, sb, nb)) return false;
                parent.i_block[bi] = nb;
                BlockFolder emp;
                for (int j = 0; j < 4; j++) emp.b_content[j] = Content();
                strncpy(emp.b_content[0].b_name, nbuf, 11);
                emp.b_content[0].b_inodo = childIno;
                if (!d.writeStructure(block_start + nb * blockSize, emp)) return false;
                return true;
            }
            BlockFolder bf;
            if (!d.readStructure(block_start + parent.i_block[bi] * blockSize, bf)) continue;
            for (int j = 0; j < 4; j++) {
                if (bf.b_content[j].b_inodo == -1) {
                    strncpy(bf.b_content[j].b_name, nbuf, 11);
                    bf.b_content[j].b_inodo = childIno;
                    return d.writeStructure(block_start + parent.i_block[bi] * blockSize, bf);
                }
            }
        }
        return false;
    }
    static bool mkdir(DiskManagementImpl& d, const std::string& diskPath, int partStart, int partSize,
                      const std::string& pathIn, bool recursive) {
        if (pathIn.empty() || pathIn[0] != '/') {
            std::cout << "Error: path debe iniciar con /." << std::endl;
            return false;
        }
        if (!d.openDisk(diskPath, std::ios::in | std::ios::out | std::ios::binary)) return false;
        Superblock sb;
        if (!d.readStructure(partStart, sb)) {
            d.closeDisk();
            return false;
        }
        int n = sb.s_inodes_count, nBlocks = sb.s_blocks_count;
        int inodeSize = (int)sizeof(Inode), blockSize = 64;
        int bmInodeSize = Utilities::calculateBitmapSize(n);
        int bmBlockSize = Utilities::calculateBitmapSize(nBlocks);
        int bm_inode_start = sb.s_bm_inode_start;
        int bm_block_start = sb.s_bm_block_start;
        int inode_start = sb.s_inode_start;
        int block_start = sb.s_block_start;
        std::vector<char> bmi(bmInodeSize), bmb(bmBlockSize);
        d.diskFile.seekg(bm_inode_start);
        d.diskFile.read(bmi.data(), bmInodeSize);
        d.diskFile.seekg(bm_block_start);
        d.diskFile.read(bmb.data(), bmBlockSize);
        auto parts = splitVirt(pathIn);
        int cur = 0;
        for (size_t i = 0; i < parts.size(); i++) {
            Inode curIno;
            if (!readInode(d, inode_start, inodeSize, cur, curIno)) {
                d.closeDisk();
                return false;
            }
            if (curIno.i_type != '0') {
                std::cout << "Error: no es carpeta en la ruta." << std::endl;
                d.closeDisk();
                return false;
            }
            int found = -1;
            if (findInFolder(d, block_start, blockSize, curIno, parts[i], found)) {
                if (i + 1 == parts.size()) {
                    std::cout << "Error: la carpeta ya existe." << std::endl;
                    d.closeDisk();
                    return false;
                }
                cur = found;
                continue;
            }
            if (!recursive && i + 1 < parts.size()) {
                std::cout << "Error: no existen carpetas padre (use -p)." << std::endl;
                d.closeDisk();
                return false;
            }
            int ni = -1, nb = -1;
            if (!allocInode(bmi.data(), n, sb, ni) || !allocBlock(bmb.data(), nBlocks, sb, nb)) {
                std::cout << "Error: sin espacio en el FS." << std::endl;
                d.closeDisk();
                return false;
            }
            Inode newIno;
            newIno.i_uid = newIno.i_gid = 1;
            newIno.i_size = 0;
            newIno.i_atime = newIno.i_ctime = newIno.i_mtime = time(nullptr);
            newIno.i_type = '0';
            newIno.i_perm[0] = newIno.i_perm[1] = '7';
            newIno.i_perm[2] = '5';
            for (int k = 0; k < 15; k++) newIno.i_block[k] = -1;
            newIno.i_block[0] = nb;
            BlockFolder fb;
            for (int k = 0; k < 4; k++) fb.b_content[k] = Content();
            strncpy(fb.b_content[0].b_name, ".", 11);
            fb.b_content[0].b_inodo = ni;
            strncpy(fb.b_content[1].b_name, "..", 11);
            fb.b_content[1].b_inodo = cur;
            if (!d.writeStructure(block_start + nb * blockSize, fb) ||
                !writeInode(d, inode_start, inodeSize, ni, newIno)) {
                d.closeDisk();
                return false;
            }
            Inode parentCopy;
            if (!readInode(d, inode_start, inodeSize, cur, parentCopy)) {
                d.closeDisk();
                return false;
            }
            if (!addEntry(d, inode_start, inodeSize, block_start, blockSize, bmb.data(), nBlocks, sb, parentCopy,
                          parts[i], ni)) {
                std::cout << "Error: carpeta padre llena o sin bloques." << std::endl;
                d.closeDisk();
                return false;
            }
            if (!writeInode(d, inode_start, inodeSize, cur, parentCopy)) {
                d.closeDisk();
                return false;
            }
            cur = ni;
        }
        d.writeStructure(partStart, sb);
        d.diskFile.seekp(bm_inode_start);
        d.diskFile.write(bmi.data(), bmInodeSize);
        d.diskFile.seekp(bm_block_start);
        d.diskFile.write(bmb.data(), bmBlockSize);
        d.diskFile.flush();
        d.closeDisk();
        return true;
    }
    static bool readFileContent(DiskManagementImpl& d, int inode_start, int inodeSize, int block_start,
                                int blockSize, const Inode& ino, std::string& out) {
        out.clear();
        int got = 0;
        for (int i = 0; i < 12 && got < ino.i_size && ino.i_block[i] >= 0; i++) {
            BlockFile bf;
            if (!d.readStructure(block_start + ino.i_block[i] * blockSize, bf)) return false;
            int take = std::min(64, ino.i_size - got);
            for (int j = 0; j < take; j++) out += bf.b_content[j];
            got += take;
        }
        return true;
    }
    static bool resolveFileInode(DiskManagementImpl& d, int inode_start, int inodeSize, int block_start,
                                 int blockSize, const std::vector<std::string>& parts, int& outFileIno) {
        int cur = 0;
        if (parts.empty()) return false;
        for (size_t i = 0; i + 1 < parts.size(); i++) {
            Inode ino;
            if (!readInode(d, inode_start, inodeSize, cur, ino) || ino.i_type != '0') return false;
            int nxt = -1;
            if (!findInFolder(d, block_start, blockSize, ino, parts[i], nxt)) return false;
            cur = nxt;
        }
        Inode parent;
        if (!readInode(d, inode_start, inodeSize, cur, parent) || parent.i_type != '0') return false;
        int fi = -1;
        if (!findInFolder(d, block_start, blockSize, parent, parts.back(), fi)) return false;
        Inode fino;
        if (!readInode(d, inode_start, inodeSize, fi, fino) || fino.i_type != '1') return false;
        outFileIno = fi;
        return true;
    }
    static bool mkfile(DiskManagementImpl& d, const std::string& diskPath, int partStart, int partSize,
                       const std::string& pathIn, bool recursive, int fileSize, const std::string& contHost) {
        if (pathIn.empty() || pathIn[0] != '/') {
            std::cout << "Error: path inválido." << std::endl;
            return false;
        }
        auto parts = splitVirt(pathIn);
        if (parts.empty()) return false;
        std::string fname = parts.back();
        if (fname.size() > 11) {
            std::cout << "Error: nombre de archivo muy largo (máx 11)." << std::endl;
            return false;
        }
        std::vector<std::string> parentParts(parts.begin(), parts.end() - 1);
        if (!parentParts.empty() && recursive) {
            std::string pp = "/";
            for (size_t i = 0; i < parentParts.size(); i++) {
                if (i) pp += "/";
                pp += parentParts[i];
            }
            if (!mkdir(d, diskPath, partStart, partSize, pp, true)) return false;
        }
        if (!d.openDisk(diskPath, std::ios::in | std::ios::out | std::ios::binary)) return false;
        Superblock sb;
        if (!d.readStructure(partStart, sb)) {
            d.closeDisk();
            return false;
        }
        int n = sb.s_inodes_count, nBlocks = sb.s_blocks_count;
        int inodeSize = (int)sizeof(Inode), blockSize = 64;
        int bmInodeSize = Utilities::calculateBitmapSize(n);
        int bmBlockSize = Utilities::calculateBitmapSize(nBlocks);
        int bm_inode_start = sb.s_bm_inode_start;
        int bm_block_start = sb.s_bm_block_start;
        int inode_start = sb.s_inode_start;
        int block_start = sb.s_block_start;
        std::vector<char> bmi(bmInodeSize), bmb(bmBlockSize);
        d.diskFile.seekg(bm_inode_start);
        d.diskFile.read(bmi.data(), bmInodeSize);
        d.diskFile.seekg(bm_block_start);
        d.diskFile.read(bmb.data(), bmBlockSize);
        std::string content;
        if (!contHost.empty()) {
            std::ifstream cf(Utilities::sanitizeHostPath(contHost));
            if (!cf) {
                std::cout << "Error: no se pudo abrir -cont." << std::endl;
                d.closeDisk();
                return false;
            }
            std::stringstream buf;
            buf << cf.rdbuf();
            content = buf.str();
        }
        if (fileSize > 0 && content.size() > (size_t)fileSize) content.resize(fileSize);
        if (fileSize > 0 && content.size() < (size_t)fileSize) content.append(fileSize - content.size(), '\0');
        int numB = (int)((content.size() + blockSize - 1) / blockSize);
        if (numB > 12) {
            std::cout << "Error: archivo demasiado grande (máx 12 bloques)." << std::endl;
            d.closeDisk();
            return false;
        }
        int cur = 0;
        for (size_t i = 0; i < parentParts.size(); i++) {
            Inode ino;
            if (!readInode(d, inode_start, inodeSize, cur, ino) || ino.i_type != '0') {
                d.closeDisk();
                return false;
            }
            int nxt = -1;
            if (!findInFolder(d, block_start, blockSize, ino, parentParts[i], nxt)) {
                d.closeDisk();
                return false;
            }
            cur = nxt;
        }
        Inode parent;
        if (!readInode(d, inode_start, inodeSize, cur, parent) || parent.i_type != '0') {
            d.closeDisk();
            return false;
        }
        int tmp = -1;
        if (findInFolder(d, block_start, blockSize, parent, fname, tmp)) {
            std::cout << "Error: el archivo ya existe." << std::endl;
            d.closeDisk();
            return false;
        }
        int ni = -1;
        if (!allocInode(bmi.data(), n, sb, ni)) {
            d.closeDisk();
            return false;
        }
        Inode fileIno;
        fileIno.i_uid = fileIno.i_gid = 1;
        fileIno.i_size = (int)content.size();
        fileIno.i_atime = fileIno.i_ctime = fileIno.i_mtime = time(nullptr);
        fileIno.i_type = '1';
        fileIno.i_perm[0] = fileIno.i_perm[1] = '6';
        fileIno.i_perm[2] = '4';
        for (int k = 0; k < 15; k++) fileIno.i_block[k] = -1;
        for (int b = 0; b < numB; b++) {
            int bi = -1;
            if (!allocBlock(bmb.data(), nBlocks, sb, bi)) {
                d.closeDisk();
                return false;
            }
            fileIno.i_block[b] = bi;
            BlockFile bf;
            memset(bf.b_content, 0, 64);
            int off = b * blockSize;
            for (int j = 0; j < blockSize && off + j < (int)content.size(); j++) bf.b_content[j] = content[off + j];
            if (!d.writeStructure(block_start + bi * blockSize, bf)) {
                d.closeDisk();
                return false;
            }
        }
        if (!writeInode(d, inode_start, inodeSize, ni, fileIno)) {
            d.closeDisk();
            return false;
        }
        if (!addEntry(d, inode_start, inodeSize, block_start, blockSize, bmb.data(), nBlocks, sb, parent, fname,
                      ni)) {
            d.closeDisk();
            return false;
        }
        if (!writeInode(d, inode_start, inodeSize, cur, parent)) {
            d.closeDisk();
            return false;
        }
        d.writeStructure(partStart, sb);
        d.diskFile.seekp(bm_inode_start);
        d.diskFile.write(bmi.data(), bmInodeSize);
        d.diskFile.seekp(bm_block_start);
        d.diskFile.write(bmb.data(), bmBlockSize);
        d.diskFile.flush();
        d.closeDisk();
        return true;
    }
};

bool DiskManagementImpl::mkdir(const std::string& diskPath, int partStart, int partSize, const std::string& path,
                               bool recursive) {
    std::cout << "Ejecutando MKDIR..." << std::endl;
    return Ext2Ops::mkdir(*this, diskPath, partStart, partSize, path, recursive);
}

bool DiskManagementImpl::mkfile(const std::string& diskPath, int partStart, int partSize, const std::string& path,
                                bool recursive, int size, const std::string& cont) {
    std::cout << "Ejecutando MKFILE..." << std::endl;
    return Ext2Ops::mkfile(*this, diskPath, partStart, partSize, path, recursive, size, cont);
}

bool DiskManagementImpl::cat(const std::string& diskPath, int partStart, int partSize,
                             const std::vector<std::string>& files, std::string& output) {
    output.clear();
    for (const std::string& f : files) {
        if (f == "/users.txt" || f == "users.txt") {
            output += readUsersFile(diskPath, partStart, partSize);
            output += "\n";
            continue;
        }
        if (!openDisk(diskPath, std::ios::in | std::ios::binary)) return false;
        Superblock sb;
        if (!readStructure(partStart, sb)) {
            closeDisk();
            return false;
        }
        int inodeSize = (int)sizeof(Inode), blockSize = 64;
        int inode_start = sb.s_inode_start;
        int block_start = sb.s_block_start;
        auto parts = Ext2Ops::splitVirt(f);
        if (parts.empty()) {
            output += "Error: ruta vacía.\n";
            closeDisk();
            continue;
        }
        int fi = -1;
        if (!Ext2Ops::resolveFileInode(*this, inode_start, inodeSize, block_start, blockSize, parts, fi)) {
            output += "Error: no se pudo leer '" + f + "'.\n";
            closeDisk();
            continue;
        }
        Inode ino;
        Ext2Ops::readInode(*this, inode_start, inodeSize, fi, ino);
        std::string data;
        Ext2Ops::readFileContent(*this, inode_start, inodeSize, block_start, blockSize, ino, data);
        output += data;
        output += "\n";
        closeDisk();
    }
    return true;
}

//========== REPORTES ==========
bool DiskManagementImpl::rep(const std::string& name, const std::string& outputPath,
                        const std::string& diskPath, int partStart, int partSize,
                        const std::string& path_file_ls) {
    std::cout << "Generando reporte " << name << "..." << std::endl;
    std::string outPath = Utilities::sanitizeHostPath(outputPath);
    if (outPath.empty()) return false;
    std::string parentPath = Utilities::getParentPath(outPath);
    if (!parentPath.empty() && !Utilities::fileExists(parentPath)) Utilities::createDirectories(parentPath);
    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
        return s;
    };
    std::string rname = toLower(name);
    auto extOf = [&](const std::string& p) -> std::string {
        size_t dot = p.find_last_of('.');
        if (dot == std::string::npos) return "";
        return toLower(p.substr(dot + 1));
    };
    auto graphvizTypeFor = [&](const std::string& p) -> std::string {
        std::string ext = extOf(p);
        if (ext == "pdf") return "pdf";
        if (ext == "png") return "png";
        if (ext == "jpg" || ext == "jpeg") return "jpg";
        return "jpg";
    };
    auto runDot = [&](const std::string& dotPath) {
        std::string t = graphvizTypeFor(outPath);
        std::string cmd = "dot -T" + t + " \"" + dotPath + "\" -o \"" + outPath + "\" 2>/dev/null || true";
        system(cmd.c_str());
    };
    if (rname == "mbr") {
        if (!openDisk(diskPath, std::ios::in | std::ios::binary)) return false;
        MBR mbr;
        if (!readStructure(0, mbr)) { closeDisk(); return false; }
        closeDisk();
        std::ofstream dotFile(outPath + ".dot");
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
        runDot(outPath + ".dot");
        return true;
    }
    if (rname == "disk") {
        if (!openDisk(diskPath, std::ios::in | std::ios::binary)) return false;
        MBR mbr;
        if (!readStructure(0, mbr)) { closeDisk(); return false; }
        closeDisk();
        std::ofstream dotFile(outPath + ".dot");
        dotFile << "digraph DISK {\n  rankdir=LR;\n  node [shape=box];\n";
        dotFile << "  mbr [label=\"MBR\\n" << sizeof(MBR) << " B\"];\n";
        for (int i = 0; i < 4; i++) {
            if (mbr.mbr_partitions[i].part_size <= 0) continue;
            dotFile << "  part" << i << " [label=\"" << mbr.mbr_partitions[i].part_name << "\\nstart="
                    << mbr.mbr_partitions[i].part_start << "\\nsize=" << mbr.mbr_partitions[i].part_size << "\"];\n";
            dotFile << "  mbr -> part" << i << ";\n";
        }
        dotFile << "  info [shape=note,label=\"Tamaño disco: " << mbr.mbr_tamano << " bytes\"];\n}\n";
        dotFile.close();
        runDot(outPath + ".dot");
        return true;
    }
    if (rname == "sb") {
        if (!openDisk(diskPath, std::ios::in | std::ios::binary)) return false;
        Superblock sb;
        if (!readStructure(partStart, sb)) { closeDisk(); return false; }
        closeDisk();
        std::ofstream dotFile(outPath + ".dot");
        dotFile << "digraph SB { node [shape=plaintext]; sb [label=<\n<table border='1'>\n";
        dotFile << "<tr><td>s_filesystem_type</td><td>" << sb.s_filesystem_type << "</td></tr>\n";
        dotFile << "<tr><td>s_inodes_count</td><td>" << sb.s_inodes_count << "</td></tr>\n";
        dotFile << "<tr><td>s_blocks_count</td><td>" << sb.s_blocks_count << "</td></tr>\n";
        dotFile << "<tr><td>s_magic</td><td>0x" << std::hex << sb.s_magic << std::dec << "</td></tr>\n</table>>]; }" << std::endl;
        dotFile.close();
        runDot(outPath + ".dot");
        return true;
    }
    if (rname == "bm_inode" || rname == "bm_block") {
        if (!openDisk(diskPath, std::ios::in | std::ios::binary)) return false;
        Superblock sb;
        if (!readStructure(partStart, sb)) { closeDisk(); return false; }
        int n = sb.s_inodes_count;
        int nBlocks = sb.s_blocks_count;
        int bmInodeSize = Utilities::calculateBitmapSize(n);
        int bmBlockSize = Utilities::calculateBitmapSize(nBlocks);
        int bm_inode_start = sb.s_bm_inode_start;
        int bm_block_start = sb.s_bm_block_start;
        char* bm = rname == "bm_inode" ? new char[bmInodeSize] : new char[bmBlockSize];
        int bits = rname == "bm_inode" ? n : nBlocks;
        int bmStart = rname == "bm_inode" ? bm_inode_start : bm_block_start;
        int bmLen = rname == "bm_inode" ? bmInodeSize : bmBlockSize;
        diskFile.seekg(bmStart);
        diskFile.read(bm, bmLen);
        closeDisk();
        std::ofstream txt(outPath);
        for (int i = 0; i < bits; i++) {
            txt << (Utilities::getBit(bm, i) ? '1' : '0');
            if ((i+1) % 20 == 0) txt << '\n';
        }
        txt << '\n';
        txt.close();
        delete[] bm;
        return true;
    }
    if (rname == "inode") {
        if (!openDisk(diskPath, std::ios::in | std::ios::binary)) return false;
        Superblock sb;
        if (!readStructure(partStart, sb)) { closeDisk(); return false; }
        int n = sb.s_inodes_count, inodeSize = (int)sizeof(Inode);
        int inode_start = sb.s_inode_start;
        std::ofstream dotFile(outPath + ".dot");
        dotFile << "digraph INODES { node [shape=plaintext]; tbl [label=<<table border='1'><tr><td>i</td><td>tipo</td><td>size</td><td>b0</td></tr>\n";
        for (int i = 0; i < n; i++) {
            Inode ino;
            if (!readStructure(inode_start + i * inodeSize, ino)) break;
            dotFile << "<tr><td>" << i << "</td><td>" << ino.i_type << "</td><td>" << ino.i_size << "</td><td>"
                    << ino.i_block[0] << "</td></tr>\n";
        }
        dotFile << "</table>>]; }\n";
        dotFile.close();
        closeDisk();
        runDot(outPath + ".dot");
        return true;
    }
    if (rname == "block") {
        if (!openDisk(diskPath, std::ios::in | std::ios::binary)) return false;
        Superblock sb;
        if (!readStructure(partStart, sb)) { closeDisk(); return false; }
        int nBlocks = sb.s_blocks_count, blockSize = sb.s_block_size;
        int block_start = sb.s_block_start;
        std::ofstream dotFile(outPath + ".dot");
        dotFile << "digraph BLOCKS { node [shape=plaintext]; tbl [label=<<table border='1'><tr><td>blk</td><td>hex (primeros 8)</td></tr>\n";
        int maxShow = std::min(nBlocks, 64);
        for (int b = 0; b < maxShow; b++) {
            BlockFile bf;
            if (!readStructure(block_start + b * blockSize, bf)) break;
            std::ostringstream hx;
            for (int j = 0; j < 8; j++) hx << std::hex << (int)(unsigned char)bf.b_content[j] << " ";
            dotFile << "<tr><td>" << b << "</td><td>" << hx.str() << "</td></tr>\n";
        }
        dotFile << "</table>>]; }\n";
        dotFile.close();
        closeDisk();
        runDot(outPath + ".dot");
        return true;
    }
    if (rname == "file" || rname == "ls") {
        std::string vpath = Utilities::sanitizeHostPath(path_file_ls);
        if (vpath.empty() || vpath[0] != '/') {
            std::cout << "Error: path_file_ls inválido." << std::endl;
            return false;
        }
        if (!openDisk(diskPath, std::ios::in | std::ios::binary)) return false;
        Superblock sb;
        if (!readStructure(partStart, sb)) { closeDisk(); return false; }
        int inodeSize = (int)sizeof(Inode), blockSize = sb.s_block_size;
        int inode_start = sb.s_inode_start, block_start = sb.s_block_start;
        if (rname == "file") {
            auto parts = Ext2Ops::splitVirt(vpath);
            int fi = -1;
            if (!Ext2Ops::resolveFileInode(*this, inode_start, inodeSize, block_start, blockSize, parts, fi)) {
                closeDisk();
                return false;
            }
            Inode ino;
            readStructure(inode_start + fi * inodeSize, ino);
            std::string data;
            Ext2Ops::readFileContent(*this, inode_start, inodeSize, block_start, blockSize, ino, data);
            if (data.size() > 500) data = data.substr(0, 500) + "...";
            std::ofstream dotFile(outPath + ".dot");
            dotFile << "digraph FILE { f [label=\"";
            for (char c : data) {
                if (c == '"' || c == '\\' || c == '\n') dotFile << ' ';
                else dotFile << c;
            }
            dotFile << "\"]; }\n";
            dotFile.close();
            closeDisk();
            runDot(outPath + ".dot");
            return true;
        }
        auto parts = Ext2Ops::splitVirt(vpath);
        int cur = 0;
        for (const std::string& seg : parts) {
            Inode ino;
            if (!readStructure(inode_start + cur * inodeSize, ino) || ino.i_type != '0') {
                closeDisk();
                return false;
            }
            int nxt = -1;
            if (!Ext2Ops::findInFolder(*this, block_start, blockSize, ino, seg, nxt)) {
                closeDisk();
                return false;
            }
            cur = nxt;
        }
        Inode dirIno;
        if (!readStructure(inode_start + cur * inodeSize, dirIno) || dirIno.i_type != '0') {
            closeDisk();
            return false;
        }
        std::ofstream dotFile(outPath + ".dot");
        dotFile << "digraph LS { node [shape=plaintext]; t [label=<<table border='1'><tr><td>nombre</td><td>inodo</td></tr>\n";
        for (int bi = 0; bi < 12; bi++) {
            if (dirIno.i_block[bi] < 0) break;
            BlockFolder bf;
            if (!readStructure(block_start + dirIno.i_block[bi] * blockSize, bf)) break;
            for (int j = 0; j < 4; j++) {
                if (bf.b_content[j].b_inodo < 0) continue;
                dotFile << "<tr><td>" << bf.b_content[j].b_name << "</td><td>" << bf.b_content[j].b_inodo
                        << "</td></tr>\n";
            }
        }
        dotFile << "</table>>]; }\n";
        dotFile.close();
        closeDisk();
        runDot(outPath + ".dot");
        return true;
    }
    if (rname == "tree") {
        if (!openDisk(diskPath, std::ios::in | std::ios::binary)) return false;
        Superblock sb;
        if (!readStructure(partStart, sb)) { closeDisk(); return false; }
        int inodeSize = (int)sizeof(Inode), blockSize = sb.s_block_size;
        int inode_start = sb.s_inode_start, block_start = sb.s_block_start;
        std::function<void(std::ostream&, int, const std::string&)> dump;
        dump = [&](std::ostream& o, int inoIdx, const std::string& indent) {
            Inode ino;
            if (!readStructure(inode_start + inoIdx * inodeSize, ino)) return;
            if (ino.i_type == '0') {
                for (int bi = 0; bi < 12; bi++) {
                    if (ino.i_block[bi] < 0) break;
                    BlockFolder bf;
                    if (!readStructure(block_start + ino.i_block[bi] * blockSize, bf)) break;
                    for (int j = 0; j < 4; j++) {
                        if (bf.b_content[j].b_inodo < 0) continue;
                        std::string nm = bf.b_content[j].b_name;
                        if (nm == "." || nm == "..") continue;
                        o << indent << nm << "\\n";
                        dump(o, bf.b_content[j].b_inodo, indent + "  ");
                    }
                }
            }
        };
        std::ofstream dotFile(outPath + ".dot");
        dotFile << "digraph TREE { root [shape=box,label=\"";
        std::ostringstream tr;
        dump(tr, 0, "");
        dotFile << tr.str();
        dotFile << "\"]; }\n";
        dotFile.close();
        closeDisk();
        runDot(outPath + ".dot");
        return true;
    }
    std::cout << "Error: tipo de reporte no soportado: " << name << std::endl;
    return false;
}

// Funciones auxiliares para búsqueda de espacios
std::vector<std::pair<int, int>> DiskManagementImpl::getFreeSpaces(const std::string& path, int diskSize) {
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

std::vector<std::pair<int, int>> DiskManagementImpl::getFreeSpacesInExtended(const std::string& path, int extStart, int extSize) {
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

int DiskManagementImpl::findFirstFit(int size, const std::vector<std::pair<int, int>>& freeSpaces) {
    for (const auto& space : freeSpaces) {
        if (space.second >= size) {
            return space.first;
        }
    }
    return -1;
}

int DiskManagementImpl::findBestFit(int size, const std::vector<std::pair<int, int>>& freeSpaces) {
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

int DiskManagementImpl::findWorstFit(int size, const std::vector<std::pair<int, int>>& freeSpaces) {
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
bool DiskManagementImpl::mkgrp(const std::string& path, int partStart, int partSize, const std::string& name) {
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

bool DiskManagementImpl::rmgrp(const std::string& path, int partStart, int partSize, const std::string& name) {
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

bool DiskManagementImpl::mkusr(const std::string& path, int partStart, int partSize, const std::string& user, const std::string& pass, const std::string& grp) {
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

bool DiskManagementImpl::rmusr(const std::string& path, int partStart, int partSize, const std::string& user) {
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

bool DiskManagementImpl::chgrp(const std::string& path, int partStart, int partSize, const std::string& user, const std::string& grp) {
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

// ----- API pública estilo lab: reenvío a una sola instancia (servidor atiende un cliente a la vez) -----
namespace DiskManagement {
namespace {
DiskManagementImpl& impl() {
    static DiskManagementImpl g;
    return g;
}
}  // namespace

bool mkdisk(int size, const std::string& unit, const std::string& path, const std::string& fit) {
    return impl().mkdisk(size, unit, path, fit);
}
bool rmdisk(const std::string& path) { return impl().rmdisk(path); }
bool fdisk(int size, const std::string& unit, const std::string& path, const std::string& type,
           const std::string& fit, const std::string& name) {
    return impl().fdisk(size, unit, path, type, fit, name);
}
bool updatePartitionMount(const std::string& path, const std::string& name, const std::string& id,
                          int& outStart, int& outSize) {
    return impl().updatePartitionMount(path, name, id, outStart, outSize);
}
bool mkfs(const std::string& id, const std::string& type, const std::string& diskPath, int partStart,
          int partSize) {
    return impl().mkfs(id, type, diskPath, partStart, partSize);
}
bool mkfile(const std::string& diskPath, int partStart, int partSize, const std::string& path,
            bool recursive, int size, const std::string& cont) {
    return impl().mkfile(diskPath, partStart, partSize, path, recursive, size, cont);
}
bool mkdir(const std::string& diskPath, int partStart, int partSize, const std::string& path,
           bool recursive) {
    return impl().mkdir(diskPath, partStart, partSize, path, recursive);
}
bool cat(const std::string& diskPath, int partStart, int partSize, const std::vector<std::string>& files,
         std::string& output) {
    return impl().cat(diskPath, partStart, partSize, files, output);
}
bool mkgrp(const std::string& path, int partStart, int partSize, const std::string& name) {
    return impl().mkgrp(path, partStart, partSize, name);
}
bool rmgrp(const std::string& path, int partStart, int partSize, const std::string& name) {
    return impl().rmgrp(path, partStart, partSize, name);
}
bool mkusr(const std::string& path, int partStart, int partSize, const std::string& user,
           const std::string& pass, const std::string& grp) {
    return impl().mkusr(path, partStart, partSize, user, pass, grp);
}
bool rmusr(const std::string& path, int partStart, int partSize, const std::string& user) {
    return impl().rmusr(path, partStart, partSize, user);
}
bool chgrp(const std::string& path, int partStart, int partSize, const std::string& user,
           const std::string& grp) {
    return impl().chgrp(path, partStart, partSize, user, grp);
}
bool rep(const std::string& name, const std::string& outputPath, const std::string& diskPath, int partStart,
         int partSize, const std::string& path_file_ls) {
    return impl().rep(name, outputPath, diskPath, partStart, partSize, path_file_ls);
}
bool validateUser(const std::string& path, int partStart, int partSize, const std::string& user,
                  const std::string& pass) {
    return impl().validateUser(path, partStart, partSize, user, pass);
}
}  // namespace DiskManagement