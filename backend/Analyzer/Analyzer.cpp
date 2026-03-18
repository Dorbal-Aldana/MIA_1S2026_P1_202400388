#include "Analyzer.h"
#include <algorithm>
#include <cctype>
#include <iostream>

Analyzer::Analyzer() {}

std::string Analyzer::toLowerCase(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string Analyzer::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    std::string result = str.substr(first, last - first + 1);
    while (!result.empty() && result.back() == '\r') result.pop_back();
    return result;
}

std::vector<std::string> Analyzer::split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

Analyzer::Command Analyzer::parseCommand(const std::string& input) {
    Command cmd;
    std::string line = trim(input);
    
    if (line.empty() || line[0] == '#') {
        cmd.name = "comment";
        return cmd;
    }
    
    // Separar por espacios respetando comillas
    std::vector<std::string> parts;
    std::string current;
    bool inQuotes = false;
    
    for (char c : line) {
        if (c == '"') {
            inQuotes = !inQuotes;
        } else if (c == ' ' && !inQuotes) {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }
    
    if (parts.empty()) return cmd;
    
    // El primer elemento es el nombre del comando
    cmd.name = toLowerCase(parts[0]);
    
    // Procesar parámetros
    for (size_t i = 1; i < parts.size(); i++) {
        std::string param = parts[i];
        size_t eqPos = param.find('=');
        
        if (eqPos != std::string::npos) {
            std::string key = toLowerCase(param.substr(0, eqPos));
            std::string value = param.substr(eqPos + 1);
            while (!value.empty() && (value.front() == '"' || value.front() == '\'')) value.erase(0, 1);
            while (!value.empty() && (value.back() == '"' || value.back() == '\'')) value.pop_back();
            if (!key.empty() && key[0] == '-') key = key.substr(1);
            cmd.params[key] = value;
        }
    }
    
    return cmd;
}

std::vector<Analyzer::Command> Analyzer::parseScript(const std::string& scriptContent) {
    std::vector<Command> commands;
    std::vector<std::string> lines = split(scriptContent, '\n');
    
    for (const auto& line : lines) {
        std::string trimmed = trim(line);
        if (!trimmed.empty()) {
            commands.push_back(parseCommand(trimmed));
        }
    }
    
    return commands;
}

bool Analyzer::isComment(const std::string& line) {
    std::string trimmed = trim(line);
    return !trimmed.empty() && trimmed[0] == '#';
}