#include "SessionManager.h"
#include "../Utilities/Utilities.h"
#include <iostream>
#include <algorithm>

SessionManager::SessionManager() {
    currentSession.active = false;
}

bool SessionManager::login(const std::string& user, const std::string& pass, const std::string& id) {
    if (currentSession.active) {
        std::cout << "Error: Ya hay una sesión activa. Debe hacer logout primero." << std::endl;
        return false;
    }
    MountedPartition* part = findMountedPartition(id);
    if (!part) {
        std::cout << "Error: La partición con ID " << id << " no está montada." << std::endl;
        return false;
    }
    return setSession(user, id);
}

bool SessionManager::setSession(const std::string& user, const std::string& id) {
    if (currentSession.active) return false;
    currentSession.username = user;
    currentSession.partitionId = id;
    currentSession.active = true;
    std::cout << "Sesión iniciada correctamente como " << user << std::endl;
    return true;
}

void SessionManager::logout() {
    if (!currentSession.active) {
        std::cout << "Error: No hay una sesión activa." << std::endl;
        return;
    }
    
    currentSession.active = false;
    currentSession.username = "";
    currentSession.partitionId = "";
    std::cout << "Sesión cerrada correctamente." << std::endl;
}

bool SessionManager::isAuthenticated() {
    return currentSession.active;
}

std::string SessionManager::getCurrentUser() {
    return currentSession.username;
}

std::string SessionManager::getCurrentPartitionId() {
    return currentSession.partitionId;
}

bool SessionManager::mountPartition(const std::string& path, const std::string& name, std::string& generatedId) {
    if (isMounted(path, name)) {
        std::cout << "Error: La partición ya está montada." << std::endl;
        return false;
    }
    
    // Orden de discos por primera vez montados: A, B, C...
    std::vector<std::string> diskOrder;
    for (const auto& m : mountedPartitions) {
        if (std::find(diskOrder.begin(), diskOrder.end(), m.path) == diskOrder.end())
            diskOrder.push_back(m.path);
    }
    auto it = std::find(diskOrder.begin(), diskOrder.end(), path);
    int letterIndex;
    int partitionNumber;
    if (it == diskOrder.end()) {
        letterIndex = (int)diskOrder.size();
        partitionNumber = 1;
    } else {
        letterIndex = (int)(it - diskOrder.begin());
        partitionNumber = 0;
        for (const auto& m : mountedPartitions)
            if (m.path == path) partitionNumber++;
        partitionNumber++;
    }
    
    generatedId = Utilities::generateId(CARNET_LAST_TWO, letterIndex, partitionNumber);
    
    MountedPartition mp;
    mp.path = path;
    mp.name = name;
    mp.id = generatedId;
    mp.start = 0;
    mp.size = 0;
    mp.isMounted = true;
    mountedPartitions.push_back(mp);
    std::cout << "Partición montada con ID: " << generatedId << std::endl;
    return true;
}

void SessionManager::addMountedPartition(const std::string& path, const std::string& name, const std::string& id, int start, int size) {
    MountedPartition* mp = findMountedPartition(id);
    if (mp) {
        mp->start = start;
        mp->size = size;
    }
}

bool SessionManager::unmountPartition(const std::string& id) {
    for (auto it = mountedPartitions.begin(); it != mountedPartitions.end(); ++it) {
        if (it->id == id) {
            mountedPartitions.erase(it);
            return true;
        }
    }
    return false;
}

std::vector<MountedPartition> SessionManager::getMountedPartitions() {
    return mountedPartitions;
}

MountedPartition* SessionManager::findMountedPartition(const std::string& id) {
    for (auto& mp : mountedPartitions) {
        if (mp.id == id) {
            return &mp;
        }
    }
    return nullptr;
}

bool SessionManager::isMounted(const std::string& path, const std::string& name) {
    for (const auto& mp : mountedPartitions) {
        if (mp.path == path && mp.name == name) {
            return true;
        }
    }
    return false;
}

bool SessionManager::canExecuteCommand() {
    if (!currentSession.active) {
        std::cout << "Error: Necesita iniciar sesión para ejecutar este comando." << std::endl;
        return false;
    }
    return true;
}