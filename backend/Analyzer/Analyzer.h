#ifndef ANALYZER_H
#define ANALYZER_H

#include <string>
#include <map>
#include <vector>
#include <sstream>

class Analyzer {
public:
    struct Command {
        std::string name;
        std::map<std::string, std::string> params;
        
        bool hasParam(const std::string& key) const {
            return params.find(key) != params.end();
        }
        
        std::string getParam(const std::string& key, const std::string& defaultValue = "") const {
            auto it = params.find(key);
            return (it != params.end()) ? it->second : defaultValue;
        }
    };
    
    Analyzer();
    
    Command parseCommand(const std::string& input);
    std::vector<Command> parseScript(const std::string& scriptContent);
    bool isComment(const std::string& line);
    
private:
    std::string toLowerCase(const std::string& str);
    std::string trim(const std::string& str);
    std::vector<std::string> split(const std::string& str, char delimiter);
};

#endif