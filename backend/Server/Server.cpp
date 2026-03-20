#include "Server.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

Server::Server() {}

void Server::get(const std::string& path, RouteHandler handler) {
    getRoutes[path] = handler;
}

void Server::post(const std::string& path, RouteHandler handler) {
    postRoutes[path] = handler;
}

void Server::start(int port) {
    std::cout << "Iniciando servidor en puerto " << port << "..." << std::endl;
    
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[65536] = {0};
    
    // Crear socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cerr << "Error al crear socket" << std::endl;
        return;
    }
    
    // Configurar socket
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        std::cerr << "Error en setsockopt" << std::endl;
        return;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        std::cerr << "Error en bind" << std::endl;
        return;
    }
    
    // Listen
    if (listen(server_fd, 3) < 0) {
        std::cerr << "Error en listen" << std::endl;
        return;
    }
    
    std::cout << "Servidor escuchando en http://localhost:" << port << std::endl;
    
    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            std::cerr << "Error en accept" << std::endl;
            continue;
        }
        
        // Lectura robusta: primero aseguramos que llegan TODOS los headers (\r\n\r\n),
        // luego leemos exactamente Content-Length bytes del body.
        std::string rawRequest;
        rawRequest.reserve(1024 * 64);
        const size_t MAX_REQ_BYTES = 20 * 1024 * 1024; // 20MB
        size_t headerEnd = std::string::npos;

        // 1) Leer hasta encontrar el final de headers.
        while (rawRequest.size() < MAX_REQ_BYTES) {
            headerEnd = rawRequest.find("\r\n\r\n");
            if (headerEnd != std::string::npos) break;
            int n = read(new_socket, buffer, 65535);
            if (n <= 0) break;
            rawRequest.append(buffer, n);
        }

        // 2) Parsear Content-Length y leer el body completo.
        if (headerEnd != std::string::npos) {
            size_t clPos = rawRequest.find("Content-Length:");
            if (clPos != std::string::npos && clPos < headerEnd) {
                size_t numStart = rawRequest.find_first_not_of(" \t", clPos + 15);
                size_t numEnd = rawRequest.find_first_of("\r\n", numStart);
                int contentLen = -1;
                if (numStart != std::string::npos && numEnd != std::string::npos) {
                    try {
                        contentLen = std::stoi(rawRequest.substr(numStart, numEnd - numStart));
                    } catch (...) {
                        contentLen = -1;
                    }
                }

                if (contentLen >= 0) {
                    size_t bodyStart = headerEnd + 4;
                    size_t need = bodyStart + (size_t)contentLen;
                    while (rawRequest.size() < need && rawRequest.size() < MAX_REQ_BYTES) {
                        int n = read(new_socket, buffer, 65535);
                        if (n <= 0) break;
                        rawRequest.append(buffer, n);
                    }
                }
            }
        }
        
        std::string rawResponse;
        handleRequest(rawRequest, rawResponse);
        
        send(new_socket, rawResponse.c_str(), rawResponse.length(), 0);
        close(new_socket);
        memset(buffer, 0, 65536);
    }
}

void Server::handleRequest(const std::string& rawRequest, std::string& rawResponse) {
    Request req;
    req.method = "";
    req.path = "";
    req.body = "";
    
    size_t bodyStart = rawRequest.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        req.body = rawRequest.substr(bodyStart + 4);
        size_t clPos = rawRequest.find("Content-Length:");
        if (clPos != std::string::npos) {
            size_t numStart = rawRequest.find_first_not_of(" \t", clPos + 14);
            size_t numEnd = rawRequest.find_first_of("\r\n", numStart);
            if (numStart != std::string::npos && numEnd != std::string::npos) {
                int len = std::stoi(rawRequest.substr(numStart, numEnd - numStart));
                if (len > 0 && len <= (int)req.body.length())
                    req.body = req.body.substr(0, len);
            }
        }
    }
    
    std::istringstream iss(rawRequest);
    std::string method, path, version;
    iss >> method >> path >> version;
    req.method = method;
    req.path = path;
    
    Response res;
    
    if (method == "GET") {
        if (getRoutes.find(path) != getRoutes.end()) {
            getRoutes[path](req, res);
        } else {
            res.statusCode = 404;
            res.body = "Not Found";
        }
    } else if (method == "POST") {
        if (postRoutes.find(path) != postRoutes.end()) {
            postRoutes[path](req, res);
        } else {
            res.statusCode = 404;
            res.body = "Not Found";
        }
    } else {
        res.statusCode = 405;
        res.body = "Method Not Allowed";
    }
    
    // Construir respuesta HTTP
    std::ostringstream oss;
    oss << "HTTP/1.1 " << res.statusCode << " OK\r\n";
    oss << "Content-Type: " << res.contentType << "\r\n";
    oss << "Content-Length: " << res.body.length() << "\r\n";
    oss << "Access-Control-Allow-Origin: *\r\n";
    oss << "\r\n";
    oss << res.body;
    
    rawResponse = oss.str();
}

// ✅ VERSIÓN CORREGIDA - AHORA CON LOS INCLUDES NECESARIOS
std::string Server::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}