#ifndef SERVER_H
#define SERVER_H

#include <string>
#include <functional>
#include <map>

// Estructura simple para petición HTTP
struct Request {
    std::string path;
    std::string method;
    std::string body;
    std::map<std::string, std::string> params;
};

struct Response {
    int statusCode;
    std::string body;
    std::string contentType;
    
    Response() : statusCode(200), contentType("text/plain") {}
    void setJson(const std::string& json) {
        contentType = "application/json";
        body = json;
    }
};

using RouteHandler = std::function<void(const Request&, Response&)>;

class Server {
private:
    std::map<std::string, RouteHandler> getRoutes;
    std::map<std::string, RouteHandler> postRoutes;
    
public:
    Server();
    
    void get(const std::string& path, RouteHandler handler);
    void post(const std::string& path, RouteHandler handler);
    
    void start(int port);
    
private:
    void handleRequest(const std::string& rawRequest, std::string& rawResponse);
    std::string readFile(const std::string& path);
};

#endif