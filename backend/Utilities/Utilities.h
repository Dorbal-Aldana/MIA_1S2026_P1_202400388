#ifndef UTILITIES_H
#define UTILITIES_H

#include <string>
#include <vector>
#include <fstream>
#include <cmath>
#include "../Structs/Structs.h"

class Utilities {
public:
    //========== CONVERSIONES ==========
    static int convertToBytes(int size, const std::string& unit);
    static std::string timeToString(time_t t);
    
    //========== ARCHIVOS ==========
    static bool fileExists(const std::string& path);
    /// Quita comillas sueltas y espacios (evita rutas tipo /home/"su_usuario"/... del PDF).
    static std::string sanitizeHostPath(std::string path);
    static bool createDirectories(const std::string& path);
    /// Estilo CLASE7 (Lab): crea directorios padre y archivo binario truncado.
    static bool createFileWithParents(const std::string& path);
    /// Mismo nombre que en el lab (CLASE7).
    static bool CreateFile(const std::string& name);
    /// Mismo nombre que en el lab (CLASE7): lectura/escritura binaria.
    static std::fstream OpenFile(const std::string& name);
    /// Estilo CLASE7: apertura binaria lectura/escritura (ruta saneada).
    static std::fstream openBinaryReadWrite(const std::string& path);

    template <typename T>
    static bool WriteObject(std::fstream& file, const T& data, std::streampos pos) {
        file.clear();
        file.seekp(pos);
        file.write(reinterpret_cast<const char*>(&data), sizeof(T));
        return file.good();
    }
    template <typename T>
    static bool ReadObject(std::fstream& file, T& data, std::streampos pos) {
        file.clear();
        file.seekg(pos);
        file.read(reinterpret_cast<char*>(&data), sizeof(T));
        return file.good();
    }
    static std::string getFileName(const std::string& path);
    static std::string getParentPath(const std::string& path);
    
    //========== BITMAPS ==========
    static void setBit(char* bitmap, int pos, bool value);
    static bool getBit(const char* bitmap, int pos);
    static int findFreeBit(const char* bitmap, int totalBits);
    static std::string bitmapToString(const char* bitmap, int totalBits);
    
    //========== CÁLCULOS EXT2 ==========
    static int calculateInodes(int partitionSize);
    static int calculateBlocks(int inodes);
    static int calculateBitmapSize(int totalItems);
    
    //========== IDs ==========
    static std::string generateId(int carnetLastTwoDigits, int diskNumber, int partitionNumber);
    static int getDiskNumberFromId(const std::string& id);
    static int getPartitionNumberFromId(const std::string& id);
    
    //========== VALIDACIONES ==========
    static bool isValidFit(const std::string& fit);
    static bool isValidUnit(const std::string& unit);
    static bool isValidType(const std::string& type);
};

#endif