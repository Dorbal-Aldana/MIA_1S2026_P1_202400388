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
    static bool createDirectories(const std::string& path);
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