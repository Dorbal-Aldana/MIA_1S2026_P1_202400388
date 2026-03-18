#ifndef DISKMANAGEMENT_H
#define DISKMANAGEMENT_H

#include <string>
#include <vector>
#include <fstream>
#include "../Structs/Structs.h"
#include "../Utilities/Utilities.h"

class DiskManagement {
private:
    std::fstream diskFile;
    std::string currentPath;
    
    // Operaciones con archivos binarios
    bool openDisk(const std::string& path, std::ios::openmode mode);
    void closeDisk();
    
    template<typename T>
    bool writeStructure(int start, const T& structure) {
        if (!diskFile.is_open()) return false;
        diskFile.seekp(start, std::ios::beg);
        diskFile.write(reinterpret_cast<const char*>(&structure), sizeof(T));
        diskFile.flush();
        return !diskFile.fail();
    }
    
    template<typename T>
    bool readStructure(int start, T& structure) {
        if (!diskFile.is_open()) return false;
        diskFile.seekg(start, std::ios::beg);
        diskFile.read(reinterpret_cast<char*>(&structure), sizeof(T));
        return !diskFile.fail();
    }
    
    // Búsqueda de espacios para particiones
    int findFirstFit(int size, const std::vector<std::pair<int, int>>& freeSpaces);
    int findBestFit(int size, const std::vector<std::pair<int, int>>& freeSpaces);
    int findWorstFit(int size, const std::vector<std::pair<int, int>>& freeSpaces);
    std::vector<std::pair<int, int>> getFreeSpaces(const std::string& path, int diskSize);
    std::vector<std::pair<int, int>> getFreeSpacesInExtended(const std::string& path, int extStart, int extSize);
    
    // Operaciones EXT2 (path del disco para abrir)
    bool formatExt2(const std::string& path, int start, int size);
    std::string readUsersFile(const std::string& path, int partStart, int partSize);
    bool writeUsersFile(const std::string& path, int partStart, int partSize, const std::string& content);
    
public:
    DiskManagement();
    
    // Comandos de discos
    bool mkdisk(int size, const std::string& unit, const std::string& path, const std::string& fit);
    bool rmdisk(const std::string& path);
    
    // Comandos de particiones
    bool fdisk(int size, const std::string& unit, const std::string& path, 
               const std::string& type, const std::string& fit, const std::string& name);
    
    // Comandos de montaje (actualiza partición en disco; el ID lo genera SessionManager)
    bool updatePartitionMount(const std::string& path, const std::string& name, const std::string& id, int& outStart, int& outSize);
    
    // Comandos de sistema de archivos (path del disco, start/size de la partición montada)
    bool mkfs(const std::string& id, const std::string& type, const std::string& diskPath, int partStart, int partSize);
    
    // Comandos de archivos/carpetas
    bool mkfile(const std::string& path, bool recursive, int size, const std::string& cont);
    bool mkdir(const std::string& path, bool recursive);
    bool cat(const std::string& diskPath, int partStart, int partSize, const std::vector<std::string>& files, std::string& output);
    
    // Comandos de usuarios (requieren path, start, size de la partición de la sesión)
    bool mkgrp(const std::string& path, int partStart, int partSize, const std::string& name);
    bool rmgrp(const std::string& path, int partStart, int partSize, const std::string& name);
    bool mkusr(const std::string& path, int partStart, int partSize, const std::string& user, const std::string& pass, const std::string& grp);
    bool rmusr(const std::string& path, int partStart, int partSize, const std::string& user);
    bool chgrp(const std::string& path, int partStart, int partSize, const std::string& user, const std::string& grp);
    
    // Reportes (path = salida, diskPath/partStart/partSize = partición o disco)
    bool rep(const std::string& name, const std::string& outputPath,
             const std::string& diskPath, int partStart, int partSize,
             const std::string& path_file_ls);
    
    bool validateUser(const std::string& path, int partStart, int partSize, const std::string& user, const std::string& pass);
};

#endif