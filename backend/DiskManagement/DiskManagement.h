#ifndef DISKMANAGEMENT_H
#define DISKMANAGEMENT_H

#include <string>
#include <vector>

// API estilo CLASE7 (Lab): funciones en namespace, sin instanciar clases desde main.
namespace DiskManagement {

bool mkdisk(int size, const std::string& unit, const std::string& path, const std::string& fit);
bool rmdisk(const std::string& path);
bool fdisk(int size, const std::string& unit, const std::string& path,
           const std::string& type, const std::string& fit, const std::string& name);
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
bool rep(const std::string& name, const std::string& outputPath, const std::string& diskPath, int partStart,
         int partSize, const std::string& path_file_ls);
bool validateUser(const std::string& path, int partStart, int partSize, const std::string& user,
                  const std::string& pass);

}  // namespace DiskManagement

#endif
