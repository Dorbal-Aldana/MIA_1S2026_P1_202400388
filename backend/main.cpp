#include <iostream>
#include <sstream>
#include "Server/Server.h"
#include "Analyzer/Analyzer.h"
#include "DiskManagement/DiskManagement.h"
#include "Sessions/SessionManager.h"
#include "Structs/Structs.h"
#include "Utilities/Utilities.h"

static std::string escapeJson(const std::string& s) {
    std::string r;
    for (char c : s) {
        if (c == '\\') r += "\\\\";
        else if (c == '"') r += "\\\"";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else r += c;
    }
    return r;
}

int main() {
    std::cout << "=== ExtreamFS Backend ===" << std::endl;
    
    Server server;
    DiskManagement diskManager;
    SessionManager sessionManager;
    Analyzer analyzer;
    
    // Servir frontend estático (opcional)
    server.get("/", [](const Request& req, Response& res) {
        // Si tienes archivos HTML en una carpeta "frontend"
        std::ifstream file("frontend/index.html");
        if (file) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            res.contentType = "text/html";
            res.body = buffer.str();
        } else {
            res.body = "ExtreamFS Backend - Servidor funcionando";
        }
    });
    
    // API endpoint para ejecutar comandos
    server.post("/api/command", [&](const Request& req, Response& res) {
        std::string commandText = req.body;
        if (commandText.empty()) {
            res.setJson("{\"output\": \"Error: No se recibió el cuerpo de la petición. Compruebe que el backend lee el body POST.\"}");
            return;
        }
        size_t pos = commandText.find("\"command\"");
        if (pos != std::string::npos) {
            pos = commandText.find(":", pos);
            if (pos != std::string::npos) {
                pos = commandText.find("\"", pos);
                if (pos != std::string::npos) {
                    size_t end = pos + 1;
                    for (; end < commandText.length(); end++) {
                        if (commandText[end] == '\\' && end + 1 < commandText.length()) { end++; continue; }
                        if (commandText[end] == '"') break;
                    }
                    commandText = commandText.substr(pos + 1, end - pos - 1);
                    for (size_t i = 0; i < commandText.length(); i++)
                        if (commandText[i] == '\\' && i + 1 < commandText.length() && commandText[i+1] == 'n')
                            commandText.replace(i, 2, "\n");
                }
            }
        } else if (commandText.find('{') == 0 && commandText.find('}') != std::string::npos) {
            res.setJson("{\"output\": \"Error: Body parece JSON pero no se encontró la clave \\\"command\\\". Envíe {\\\"command\\\": \\\"...\\\"}\"}");
            return;
        }
        if (commandText.size() >= 3 && (unsigned char)commandText[0] == 0xEF && (unsigned char)commandText[1] == 0xBB && (unsigned char)commandText[2] == 0xBF)
            commandText = commandText.substr(3);
        for (size_t i = 0; i < commandText.length(); )
            if (commandText[i] == '\r') commandText.erase(i, 1); else i++;
        std::string result;
        std::vector<std::string> lines;
        std::istringstream iss(commandText);
        std::string line;
        while (std::getline(iss, line)) {
            while (!line.empty() && line.back() == '\r') line.pop_back();
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) continue;
            line = line.substr(first);
            size_t last = line.find_last_not_of(" \t\r\n");
            if (last != std::string::npos) line = line.substr(0, last + 1);
            if (line.empty()) continue;
            lines.push_back(line);
        }
        if (lines.empty() && !commandText.empty()) lines.push_back(commandText);
        if (lines.empty()) lines.push_back("");
        
        for (const std::string& singleLine : lines) {
            auto cmd = analyzer.parseCommand(singleLine);
            std::string lineResult;
        
            if (cmd.name == "comment") {
                lineResult = (singleLine.find('#') == 0) ? singleLine : "";
            }
            else if (cmd.name == "mkdisk") {
            int size = 0;
            try { size = std::stoi(cmd.getParam("size", "0")); } catch(...) {}
            std::string unit = cmd.getParam("unit", "M");
            std::string path = cmd.getParam("path");
            std::string fit = cmd.getParam("fit", "FF");
            if (path.empty()) lineResult = "Error: -path obligatorio.";
            else if (size <= 0) lineResult = "Error: -size debe ser positivo.";
            else if (diskManager.mkdisk(size, unit, path, fit)) lineResult = "MKDISK ejecutado correctamente.";
            else lineResult = "Error al crear el disco.";
        }
        else if (cmd.name == "rmdisk") {
            std::string path = cmd.getParam("path");
            if (path.empty()) lineResult = "Error: -path obligatorio.";
            else if (!Utilities::fileExists(path)) lineResult = "Error: El disco no existe.";
            else if (diskManager.rmdisk(path)) lineResult = "RMDISK ejecutado correctamente.";
            else lineResult = "Error al eliminar el disco.";
        }
        else if (cmd.name == "fdisk") {
            int size = 0;
            try { size = std::stoi(cmd.getParam("size", "0")); } catch(...) {}
            std::string unit = cmd.getParam("unit", "K");
            std::string path = cmd.getParam("path");
            std::string type = cmd.getParam("type", "P");
            std::string fit = cmd.getParam("fit", "WF");
            std::string name = cmd.getParam("name");
            if (path.empty() || name.empty()) lineResult = "Error: -path y -name obligatorios.";
            else if (size <= 0) lineResult = "Error: -size debe ser positivo.";
            else if (diskManager.fdisk(size, unit, path, type, fit, name)) lineResult = "FDISK ejecutado correctamente.";
            else lineResult = "Error al crear la partición.";
        }
        else if (cmd.name == "mount") {
            std::string path = cmd.getParam("path");
            std::string name = cmd.getParam("name");
            std::string id;
            if (!sessionManager.mountPartition(path, name, id)) {
                lineResult = "Error: No se pudo montar (partición ya montada o no encontrada).";
            } else {
                int start, size;
                if (diskManager.updatePartitionMount(path, name, id, start, size)) {
                    sessionManager.addMountedPartition(path, name, id, start, size);
                    lineResult = "MOUNT ejecutado - ID: " + id;
                } else {
                    lineResult = "Error: No se pudo actualizar la partición en disco.";
                    sessionManager.unmountPartition(id);
                }
            }
        }
        else if (cmd.name == "mounted") {
            auto mounted = sessionManager.getMountedPartitions();
            lineResult = "Particiones montadas:\n";
            for (const auto& m : mounted) {
                lineResult += m.id + " - " + m.path + " - " + m.name + "\n";
            }
        }
        else if (cmd.name == "mkfs") {
            std::string id = cmd.getParam("id");
            std::string type = cmd.getParam("type", "full");
            MountedPartition* mp = sessionManager.findMountedPartition(id);
            if (!mp) {
                lineResult = "Error: La partición con ID " + id + " no está montada.";
            } else if (diskManager.mkfs(id, type, mp->path, mp->start, mp->size)) {
                lineResult = "MKFS ejecutado correctamente.";
            } else {
                lineResult = "Error al formatear la partición.";
            }
        }
        else if (cmd.name == "login") {
            std::string user = cmd.getParam("user");
            std::string pass = cmd.getParam("pass");
            std::string id = cmd.getParam("id");
            MountedPartition* mp = sessionManager.findMountedPartition(id);
            if (!mp) {
                lineResult = "Error: La partición con ID " + id + " no está montada.";
            } else if (!diskManager.validateUser(mp->path, mp->start, mp->size, user, pass)) {
                lineResult = "Error: Usuario o contraseña incorrectos.";
            } else if (!sessionManager.setSession(user, id)) {
                lineResult = "Error: Ya hay una sesión activa. Haga logout primero.";
            } else {
                lineResult = "LOGIN correcto. Sesión iniciada como " + user;
            }
        }
        else if (cmd.name == "logout") {
            if (!sessionManager.isAuthenticated()) {
                lineResult = "Error: No hay una sesión activa.";
            } else {
                sessionManager.logout();
                lineResult = "LOGOUT correcto.";
            }
        }
        else if (cmd.name == "mkgrp" || cmd.name == "rmgrp" || cmd.name == "mkusr" || cmd.name == "rmusr" || cmd.name == "chgrp") {
            if (!sessionManager.isAuthenticated()) {
                lineResult = "Error: Debe iniciar sesión para ejecutar este comando.";
            } else if (sessionManager.getCurrentUser() != "root") {
                lineResult = "Error: Solo el usuario root puede ejecutar este comando.";
            } else {
                MountedPartition* mp = sessionManager.findMountedPartition(sessionManager.getCurrentPartitionId());
                if (!mp) { lineResult = "Error: Partición no encontrada."; }
                else if (cmd.name == "mkgrp") {
                    std::string name = cmd.getParam("name");
                    if (name.empty()) lineResult = "Error: Parámetro -name obligatorio.";
                    else if (diskManager.mkgrp(mp->path, mp->start, mp->size, name)) lineResult = "MKGRP ejecutado correctamente.";
                    else lineResult = "Error al crear el grupo.";
                } else if (cmd.name == "rmgrp") {
                    std::string name = cmd.getParam("name");
                    if (name.empty()) lineResult = "Error: Parámetro -name obligatorio.";
                    else if (diskManager.rmgrp(mp->path, mp->start, mp->size, name)) lineResult = "RMGRP ejecutado correctamente.";
                    else lineResult = "Error al eliminar el grupo.";
                } else if (cmd.name == "mkusr") {
                    std::string user = cmd.getParam("user"), pass = cmd.getParam("pass"), grp = cmd.getParam("grp");
                    if (user.empty() || pass.empty() || grp.empty()) lineResult = "Error: -user, -pass y -grp obligatorios.";
                    else if (diskManager.mkusr(mp->path, mp->start, mp->size, user, pass, grp)) lineResult = "MKUSR ejecutado correctamente.";
                    else lineResult = "Error al crear el usuario.";
                } else if (cmd.name == "rmusr") {
                    std::string user = cmd.getParam("user");
                    if (user.empty()) lineResult = "Error: Parámetro -user obligatorio.";
                    else if (diskManager.rmusr(mp->path, mp->start, mp->size, user)) lineResult = "RMUSR ejecutado correctamente.";
                    else lineResult = "Error al eliminar el usuario.";
                } else if (cmd.name == "chgrp") {
                    std::string user = cmd.getParam("user"), grp = cmd.getParam("grp");
                    if (user.empty() || grp.empty()) lineResult = "Error: -user y -grp obligatorios.";
                    else if (diskManager.chgrp(mp->path, mp->start, mp->size, user, grp)) lineResult = "CHGRP ejecutado correctamente.";
                    else lineResult = "Error al cambiar el grupo.";
                }
            }
        }
        else if (cmd.name == "cat") {
            if (!sessionManager.isAuthenticated()) lineResult = "Error: Debe iniciar sesión.";
            else {
                std::vector<std::string> files;
                for (int i = 1; ; i++) {
                    std::string f = cmd.getParam("file" + std::to_string(i));
                    if (f.empty()) break;
                    files.push_back(f);
                }
                if (files.empty()) lineResult = "Error: Especifique al menos -file1=...";
                else {
                    MountedPartition* mp = sessionManager.findMountedPartition(sessionManager.getCurrentPartitionId());
                    if (!mp) lineResult = "Error: Partición no encontrada.";
                    else {
                        std::string catOut;
                        diskManager.cat(mp->path, mp->start, mp->size, files, catOut);
                        lineResult = catOut.empty() ? "(vacío)" : catOut;
                    }
                }
            }
        }
        else if (cmd.name == "rep") {
            std::string name = cmd.getParam("name");
            std::string path = cmd.getParam("path");
            std::string id = cmd.getParam("id");
            std::string path_file_ls = cmd.getParam("path_file_ls", "");
            MountedPartition* mp = sessionManager.findMountedPartition(id);
            if (!mp) lineResult = "Error: La partición con ID " + id + " no está montada.";
            else if (diskManager.rep(name, path, mp->path, mp->start, mp->size, path_file_ls)) lineResult = "REP ejecutado: " + path;
            else lineResult = "Error al generar el reporte.";
        }
        else {
            lineResult = "Comando no reconocido: " + cmd.name;
        }
            if (!lineResult.empty() || cmd.name != "comment") result += (result.empty() ? "" : "\n") + lineResult;
        }
        if (result.empty()) result = "(No se generó salida. Compruebe que el script tiene comandos y que el body JSON incluye \"command\".)";
        res.setJson("{\"output\": \"" + escapeJson(result) + "\"}");
    });
    
    // Iniciar servidor
    server.start(8080);
    
    return 0;
}