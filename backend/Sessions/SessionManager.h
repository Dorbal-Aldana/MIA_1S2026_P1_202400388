#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <string>
#include <vector>
#include "../Structs/Structs.h"

class SessionManager {
private:
    struct ActiveSession {
        std::string username;
        std::string partitionId;
        bool active;
        
        ActiveSession() : active(false) {}
    } currentSession;
    
    std::vector<MountedPartition> mountedPartitions;
    
public:
    SessionManager();
    
    // Gestión de sesión (login verifica en main con users.txt; setSession solo asigna)
    bool login(const std::string& user, const std::string& pass, const std::string& id);
    bool setSession(const std::string& user, const std::string& id);
    void logout();
    bool isAuthenticated();
    std::string getCurrentUser();
    std::string getCurrentPartitionId();
    
    // Gestión de montajes
    bool mountPartition(const std::string& path, const std::string& name, std::string& generatedId);
    void addMountedPartition(const std::string& path, const std::string& name, const std::string& id, int start, int size);
    bool unmountPartition(const std::string& id);
    std::vector<MountedPartition> getMountedPartitions();
    MountedPartition* findMountedPartition(const std::string& id);
    
    // Validaciones
    bool isMounted(const std::string& path, const std::string& name);
    bool canExecuteCommand();
    
    // Script de calificación usa "34" como ejemplo; pon aquí tus 2 últimos dígitos (ej. 388 -> 88).
    static const int CARNET_LAST_TWO = 34;
};

#endif