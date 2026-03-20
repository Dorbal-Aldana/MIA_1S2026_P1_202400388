#ifndef DISKMANAGEMENT_IMPL_H
#define DISKMANAGEMENT_IMPL_H

#include <fstream>
#include <string>
#include <vector>
#include "../Structs/Structs.h"
#include "../Utilities/Utilities.h"

struct Ext2Ops;

/// Implementación interna (estilo lab: lógica en una sola clase; el exterior usa `namespace DiskManagement`).
class DiskManagementImpl {
    friend struct Ext2Ops;

private:
    std::fstream diskFile;
    std::string currentPath;

    bool openDisk(const std::string& path, std::ios::openmode mode);
    void closeDisk();

    template <typename T>
    bool writeStructure(int start, const T& structure) {
        if (!diskFile.is_open()) return false;
        diskFile.seekp(start, std::ios::beg);
        diskFile.write(reinterpret_cast<const char*>(&structure), sizeof(T));
        diskFile.flush();
        return !diskFile.fail();
    }

    template <typename T>
    bool readStructure(int start, T& structure) {
        if (!diskFile.is_open()) return false;
        diskFile.seekg(start, std::ios::beg);
        diskFile.read(reinterpret_cast<char*>(&structure), sizeof(T));
        return !diskFile.fail();
    }

    int findFirstFit(int size, const std::vector<std::pair<int, int>>& freeSpaces);
    int findBestFit(int size, const std::vector<std::pair<int, int>>& freeSpaces);
    int findWorstFit(int size, const std::vector<std::pair<int, int>>& freeSpaces);
    std::vector<std::pair<int, int>> getFreeSpaces(const std::string& path, int diskSize);
    std::vector<std::pair<int, int>> getFreeSpacesInExtended(const std::string& path, int extStart,
                                                             int extSize);

    bool formatExt2(const std::string& path, int start, int size);
    std::string readUsersFile(const std::string& path, int partStart, int partSize);
    bool writeUsersFile(const std::string& path, int partStart, int partSize, const std::string& content);

public:
    bool mkdisk(int size, const std::string& unit, const std::string& path, const std::string& fit);
    bool rmdisk(const std::string& path);
    bool fdisk(int size, const std::string& unit, const std::string& path, const std::string& type,
               const std::string& fit, const std::string& name);
    bool updatePartitionMount(const std::string& path, const std::string& name, const std::string& id,
                              int& outStart, int& outSize);
    bool mkfs(const std::string& id, const std::string& type, const std::string& diskPath, int partStart,
              int partSize);
    bool mkfile(const std::string& diskPath, int partStart, int partSize, const std::string& path,
                bool recursive, int size, const std::string& cont);
    bool mkdir(const std::string& diskPath, int partStart, int partSize, const std::string& path,
                 bool recursive);
    bool cat(const std::string& diskPath, int partStart, int partSize, const std::vector<std::string>& files,
             std::string& output);
    bool mkgrp(const std::string& path, int partStart, int partSize, const std::string& name);
    bool rmgrp(const std::string& path, int partStart, int partSize, const std::string& name);
    bool mkusr(const std::string& path, int partStart, int partSize, const std::string& user,
               const std::string& pass, const std::string& grp);
    bool rmusr(const std::string& path, int partStart, int partSize, const std::string& user);
    bool chgrp(const std::string& path, int partStart, int partSize, const std::string& user,
               const std::string& grp);
    bool rep(const std::string& name, const std::string& outputPath, const std::string& diskPath,
             int partStart, int partSize, const std::string& path_file_ls);
    bool validateUser(const std::string& path, int partStart, int partSize, const std::string& user,
                      const std::string& pass);
};

#endif
