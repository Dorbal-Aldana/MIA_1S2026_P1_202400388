#include "Utilities.h"
#include <sys/stat.h>
#include <sstream>
#include <iomanip>
#include <iostream>

//========== CONVERSIONES ==========
int Utilities::convertToBytes(int size, const std::string& unit) {
    if (unit == "B" || unit == "b") return size;
    if (unit == "K" || unit == "k") return size * 1024;
    if (unit == "M" || unit == "m") return size * 1024 * 1024;
    return size * 1024 * 1024; // Default: MB
}

std::string Utilities::timeToString(time_t t) {
    std::tm* tm = std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

//========== ARCHIVOS ==========
bool Utilities::fileExists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool Utilities::createDirectories(const std::string& path) {
    std::string command = "mkdir -p \"" + path + "\"";
    return system(command.c_str()) == 0;
}

std::string Utilities::getFileName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

std::string Utilities::getParentPath(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(0, pos);
    }
    return "";
}

//========== BITMAPS ==========
void Utilities::setBit(char* bitmap, int pos, bool value) {
    int bytePos = pos / 8;
    int bitPos = pos % 8;
    if (value) {
        bitmap[bytePos] |= (1 << bitPos);
    } else {
        bitmap[bytePos] &= ~(1 << bitPos);
    }
}

bool Utilities::getBit(const char* bitmap, int pos) {
    int bytePos = pos / 8;
    int bitPos = pos % 8;
    return (bitmap[bytePos] >> bitPos) & 1;
}

int Utilities::findFreeBit(const char* bitmap, int totalBits) {
    for (int i = 0; i < totalBits; i++) {
        if (!getBit(bitmap, i)) return i;
    }
    return -1;
}

std::string Utilities::bitmapToString(const char* bitmap, int totalBits) {
    std::string result;
    for (int i = 0; i < totalBits; i++) {
        result += getBit(bitmap, i) ? '1' : '0';
        if ((i + 1) % 20 == 0) result += '\n';
    }
    return result;
}

//========== CÁLCULOS EXT2 ==========
int Utilities::calculateInodes(int partitionSize) {
    // Fórmula: tamaño_particion = sizeof(Superblock) + n + 3n + n*sizeof(Inode) + 3n*sizeof(Block)
    int superblockSize = sizeof(Superblock);
    int inodeSize = sizeof(Inode);
    int blockSize = 64; // Los bloques son de 64 bytes
    
    // n + 3n + n*inodeSize + 3n*blockSize + superblockSize = partitionSize
    // n(4 + inodeSize + 3*blockSize) = partitionSize - superblockSize
    int denominator = 4 + inodeSize + (3 * blockSize);
    int n = (partitionSize - superblockSize) / denominator;
    
    return n > 0 ? n : 1; // Mínimo 1 inodo
}

int Utilities::calculateBlocks(int inodes) {
    return inodes * 3; // El número de bloques es el triple que el de inodos
}

int Utilities::calculateBitmapSize(int totalItems) {
    return (totalItems + 7) / 8; // Redondear hacia arriba
}

//========== IDs ==========
// ID = últimos 2 dígitos carnet + número partición + letra (A,B,C por disco)
std::string Utilities::generateId(int carnetLastTwoDigits, int diskLetterIndex, int partitionNumber) {
    std::string id = (carnetLastTwoDigits < 10 ? "0" : "") + std::to_string(carnetLastTwoDigits % 100);
    id += std::to_string(partitionNumber);
    id += char('A' + diskLetterIndex);
    return id;
}

int Utilities::getDiskNumberFromId(const std::string& id) {
    if (id.length() < 3) return -1;
    char lastChar = id[id.length() - 1];
    return (lastChar - 'A' + 1);
}

int Utilities::getPartitionNumberFromId(const std::string& id) {
    if (id.length() < 3) return -1;
    return std::stoi(id.substr(id.length() - 2, 1));
}

//========== VALIDACIONES ==========
bool Utilities::isValidFit(const std::string& fit) {
    return fit == "BF" || fit == "FF" || fit == "WF";
}

bool Utilities::isValidUnit(const std::string& unit) {
    return unit == "B" || unit == "K" || unit == "M";
}

bool Utilities::isValidType(const std::string& type) {
    return type == "P" || type == "E" || type == "L";
}