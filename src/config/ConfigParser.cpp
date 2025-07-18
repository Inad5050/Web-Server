#include "../../include/config/ConfigParser.hpp"
#include "../../include/config/ConfigNode.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <stdexcept>

ConfigParser::ConfigParser() : _configRoot(NULL) {}

ConfigParser::~ConfigParser() {
    delete _configRoot;
}

ConfigParser& ConfigParser::getInst() {
    static ConfigParser instance;
    return instance;
}

bool ConfigParser::load(const std::string& filePath) {
    delete _configRoot;
    _configRoot = new ConfigNode();
    static_cast<ConfigNode*>(_configRoot)->setType("root");

    std::ifstream file(filePath.c_str());
    if (!file.is_open()) {
        std::cerr << "Error: No se pudo abrir " << filePath << std::endl;
        return false;
    }
    try {
        std::vector<std::string> tokens = tokenize(file);
        if(tokens.empty()) {
            std::cerr << "Error: No se pudieron tokenizar los datos del archivo " << filePath << std::endl;
            return false;
        }
        size_t index = 0;
        while (index < tokens.size()) {
            parse(_configRoot, tokens, index);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error de parseo en " << filePath << ": " << e.what() << std::endl;
        delete _configRoot;
        _configRoot = NULL;
        return false;
    }
    return true;
}

std::string ConfigParser::getGlobalAlt(IConfig* server, const std::string& key) const 
{
    if (!_configRoot || !server)
		return "";

    const IConfig* serverNode = server;
    const IConfig* directiveNode = serverNode->getChild(key);

    if (directiveNode && !directiveNode->getValues().empty()) {
        return directiveNode->getValues()[0];
    }
    
    if (key == "listen") {
        const IConfig* listenNode = serverNode->getChild("listen");
        if(listenNode && !listenNode->getValues().empty()) return listenNode->getValues()[0];
    }
    
    return "";
}

const IConfig* ConfigParser::getConfig() const {
    return _configRoot;
}

std::vector<std::string> ConfigParser::tokenize(std::ifstream& file) {
    std::vector<std::string> tokens;
    std::string line;
    const std::string delimiters = " \t\r\n";
    const std::string special_chars = "{};";
    
    std::cout << "[DEBUG] Iniciando tokenización del archivo de configuración." << std::endl;
    
    int processed_servers = 0;
    int brace_count = 0;
    bool inside_server = false;
    bool found_server_keyword = false;
    std::vector<std::string> current_server_tokens;
    
    while (std::getline(file, line)) {
        std::cout << "[DEBUG][NEW_LINE]: " << line << std::endl;
        
     
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line.erase(comment_pos);
        }
        
 
        if (line.find_first_not_of(" \t") == std::string::npos) {
            continue;
        }
        
        
        if (line.find("server") != std::string::npos && !inside_server) {
            found_server_keyword = true;
            std::cout << "[DEBUG] Encontrada palabra clave 'server'" << std::endl;
        }
        
        // Check if we found opening brace after server keyword
        if (found_server_keyword && line.find("{") != std::string::npos && !inside_server) {
            inside_server = true;
            found_server_keyword = false;
            brace_count = 0;
            current_server_tokens.clear();
            std::cout << "[DEBUG] Iniciando procesamiento del servidor " << (processed_servers + 1) << std::endl;
        }
        
        if (inside_server || found_server_keyword) {
            // Tokenize the current line and store in temporary vector
            size_t current_pos = 0;
            while (current_pos < line.length()) {
                size_t start = line.find_first_not_of(delimiters, current_pos);
                if (start == std::string::npos) {
                    break;
                }
                
                if (special_chars.find(line[start]) != std::string::npos) {
                    current_server_tokens.push_back(line.substr(start, 1));
                    
                    // Track braces to know when server block ends
                    if (line[start] == '{') {
                        brace_count++;
                        if (!inside_server && found_server_keyword) {
                            inside_server = true;
                            found_server_keyword = false;
                            std::cout << "[DEBUG] Iniciando procesamiento del servidor " << (processed_servers + 1) << std::endl;
                        }
                    } else if (line[start] == '}') {
                        brace_count--;
                        if (brace_count == 0 && inside_server) {
/*                             // End of server block - validate before adding
                            if (validateServerTokens(current_server_tokens)) {
                                // Server is valid, add tokens to main vector */
                                for (size_t i = 0; i < current_server_tokens.size(); ++i) {
                                    tokens.push_back(current_server_tokens[i]);
                                }
                                processed_servers++;
                                std::cout << "[DEBUG] Servidor " << processed_servers << " validado y agregado." << std::endl;
/*                             } else {
                                std::cout << "[DEBUG] Servidor descartado por errores de validación." << std::endl;
                            } */
                            
                            inside_server = false;
                            current_server_tokens.clear();
                        }
                    }
                    
                    current_pos = start + 1;
                    continue;
                }
                
                size_t end = line.find_first_of(delimiters + special_chars, start);
                if (end == std::string::npos) {
                    std::string token = line.substr(start);
                    current_server_tokens.push_back(token);
                    break;
                } else {
                    std::string token = line.substr(start, end - start);
                    current_server_tokens.push_back(token);
                    current_pos = end;
                }
            }
        }
    }
    
    std::cout << "[DEBUG] Tokenización completada. Total tokens válidos: " << tokens.size() << std::endl;
    std::cout << "[DEBUG] Servidores válidos procesados: " << processed_servers << std::endl;
    return tokens;
}

void ConfigParser::parse(IConfig* parent, std::vector<std::string>& tokens, size_t& index) {
    if (index >= tokens.size()) return;
    ConfigNode* node = new ConfigNode();
    node->setType(tokens[index++]);
    while (index < tokens.size() && tokens[index] != "{" && tokens[index] != ";") {
        node->addValue(tokens[index++]);
    }
    if (index < tokens.size()) {
        if (tokens[index] == "{") {
            index++;
            while (index < tokens.size() && tokens[index] != "}") {
                parse(node, tokens, index);
            }
            if (index >= tokens.size() || tokens[index] != "}") {
                delete node;
                throw std::runtime_error("Se esperaba '}' de cierre.");
            }
            index++;
        } else if (tokens[index] == ";") {
            index++;
        }
    }
    static_cast<ConfigNode*>(parent)->addChild(node);
}

std::string ConfigParser::getDirectiveValue(const IConfig* node, const std::string& key, const std::string& defaultValue) const
{
    if (!node) 
        return defaultValue;

    const IConfig* child = node->getChild(key);
    if (child && !child->getValues().empty())
        return child->getValues()[0];

    return defaultValue;
}

std::string ConfigParser::getServerName(const IConfig* serverNode) 
{
    std::cout << "[DEBUG][getServerName] START" << std::endl;
    const std::vector<std::string>& values = serverNode->getValues();
    for (size_t i = 0; i < values.size(); ++i) {
        std::cout << values[i] << std::endl; 
    }
    return getDirectiveValue(serverNode, "server_name", "default_server");
}

const std::vector<IConfig*>& ConfigParser::getServerBlocks() const 
{
    if (!_configRoot) {
        static const std::vector<IConfig*> empty_vector;
        return empty_vector;
    }
    return _configRoot->getChildren();
}

std::string ConfigParser::getErrorPage(int errorCode, const IConfig* serverNode) const {
    if (!_configRoot) return "";
    if (!serverNode) {
        const std::vector<IConfig*>& servers = _configRoot->getChildren();
        if (servers.empty() || servers[0]->getType() != "server") return "";
        serverNode = servers[0];
    }
    
    const std::vector<IConfig*>& locations = serverNode->getChildren();
    for (size_t i = 0; i < locations.size(); ++i) {
        if (locations[i]->getType() == "location" && 
            !locations[i]->getValues().empty() && 
            locations[i]->getValues()[0] == "/error_pages") {
            
            const std::vector<IConfig*>& directives = locations[i]->getChildren();
            for (size_t j = 0; j < directives.size(); ++j) {
                const std::string& directiveType = directives[j]->getType();
                
                if ((errorCode == 404 && directiveType == "not_found") ||
                    (errorCode == 403 && directiveType == "forbidden") ||
                    (errorCode == 400 && directiveType == "bad_request") ||
                    (errorCode == 502 && directiveType == "bad_getaway") ||
                    (errorCode == 500 && directiveType == "internal_error")) {
                    
                    const std::vector<std::string>& values = directives[j]->getValues();
                    if (!values.empty()) {
                        std::string filePath = values[0];
                        
                        if (validateErrorPagePath(filePath)) {
                            std::cout << "[INFO] Página de error " << errorCode << " configurada: " << filePath << std::endl;
                            return filePath;
                        } else {
                            std::cout << "[WARNING] Página de error " << errorCode << " no sigue el formato correcto. Usando página por defecto." << std::endl;
                            return "";
                        }
                    }
                }
            }
            break;
        }
    }
    return "";
}

bool ConfigParser::validateErrorPagePath(const std::string& filePath) const {
    std::ifstream file(filePath.c_str());
    if (file.good()) {
        return true;
    }

    std::cout << "[WARNING] Archivo de error no encontrado: " << filePath << std::endl;
    return false;
}

bool ConfigParser::isMethodAllowed(const IConfig* serverNode, const std::string& path, const std::string& method) const 
{
    if (!serverNode) 
		return false;

    const IConfig* locationNode = findLocationBlock(serverNode, path);
    const IConfig* permissionSourceNode = NULL;

    if (locationNode)
        permissionSourceNode = locationNode->getChild("allow_methods");

    if (!permissionSourceNode)
        permissionSourceNode = serverNode->getChild("allow_methods");

    if (!permissionSourceNode)
        return true;

    const std::vector<std::string>& allowedMethods = permissionSourceNode->getValues();
    for (size_t i = 0; i < allowedMethods.size(); ++i) 
	{
        if (allowedMethods[i] == method)
            return true;
    }
    return false;
}

const IConfig* ConfigParser::findLocationBlock(const IConfig* serverNode, const std::string& path) const
{
    if (!serverNode || serverNode->getType() != "server")
        return NULL;

    const std::vector<IConfig*>& children = serverNode->getChildren();
    const IConfig* bestMatch = NULL;
    size_t longestMatchLength = 0;

    for (size_t i = 0; i < children.size(); ++i) 
	{
        if (children[i]->getType() == "location") 
		{
            const std::string& locationPath = children[i]->getValues()[0];
            if (path.rfind(locationPath, 0) == 0) 
			{
                if (locationPath.length() > longestMatchLength) 
				{
                    longestMatchLength = locationPath.length();
                    bestMatch = children[i];
                }
            }
        }
    }
    return bestMatch;
}

bool ConfigParser::validateServerTokens(const std::vector<std::string>& serverTokens) const {
    if (serverTokens.empty()) {
        std::cout << "[DEBUG] Validación falló: tokens vacíos" << std::endl;
        return false;
    }
    
    // Check basic structure: server { ... }
    if (serverTokens.size() < 3) {
        std::cout << "[DEBUG] Validación falló: estructura mínima no cumplida" << std::endl;
        return false;
    }
    
    if (serverTokens[0] != "server") {
        std::cout << "[DEBUG] Validación falló: no comienza con 'server'" << std::endl;
        return false;
    }
    
    if (serverTokens[1] != "{") {
        std::cout << "[DEBUG] Validación falló: no tiene '{' después de 'server'" << std::endl;
        return false;
    }
    
    if (serverTokens.back() != "}") {
        std::cout << "[DEBUG] Validación falló: no termina con '}'" << std::endl;
        return false;
    }
    
    
    int brace_count = 0;
    for (size_t i = 0; i < serverTokens.size(); ++i) {
        if (serverTokens[i] == "{") {
            brace_count++;
        } else if (serverTokens[i] == "}") {
            brace_count--;
            if (brace_count < 0) {
                std::cout << "[DEBUG] Validación falló: llaves desbalanceadas" << std::endl;
                return false;
            }
        }
    }
    
    if (brace_count != 0) {
        std::cout << "[DEBUG] Validación falló: llaves no balanceadas" << std::endl;
        return false;
    }
    
    // Validate directive syntax - check for malformed directives like "port8081;"
    for (size_t i = 1; i < serverTokens.size() - 1; ++i) {
        const std::string& token = serverTokens[i];
        
        // Skip special characters and location blocks
        if (token == "{" || token == "}" || token == ";" || token == "location") {
            continue;
        }
        
        // Check for malformed directives (directive mixed with value)
        if (token.find("port") == 0 && token.length() > 4) {
            std::cout << "[ERROR] Validación falló: directiva 'port' mal formada: " << token << std::endl;
            std::cout << "[ERROR] Formato correcto: 'port 8081;'" << std::endl;
            return false;
        }
        
        if (token.find("server_name") == 0 && token.length() > 11) {
            std::cout << "[ERROR] Validación falló: directiva 'server_name' mal formada: " << token << std::endl;
            std::cout << "[ERROR] Formato correcto: 'server_name config1.com;'" << std::endl;
            return false;
        }
        
        if (token.find("host") == 0 && token.length() > 4) {
            std::cout << "[ERROR] Validación falló: directiva 'host' mal formada: " << token << std::endl;
            std::cout << "[ERROR] Formato correcto: 'host 127.0.0.1;'" << std::endl;
            return false;
        }
        
        if (token.find("root") == 0 && token.length() > 4) {
            std::cout << "[ERROR] Validación falló: directiva 'root' mal formada: " << token << std::endl;
            std::cout << "[ERROR] Formato correcto: 'root ./www;'" << std::endl;
            return false;
        }
        
        if (token.find("index") == 0 && token != "index.html" && token.length() > 5) {
            std::cout << "[ERROR] Validación falló: directiva 'index' mal formada: " << token << std::endl;
            std::cout << "[ERROR] Formato correcto: 'index index.html;'" << std::endl;
            return false;
        }
        
        if (token.find("body_size") == 0 && token.length() > 9) {
            std::cout << "[ERROR] Validación falló: directiva 'body_size' mal formada: " << token << std::endl;
            std::cout << "[ERROR] Formato correcto: 'body_size 100000;'" << std::endl;
            return false;
        }
        
        // Check for tokens that contain semicolons but shouldn't
        if (token.find(';') != std::string::npos && token != ";") {
            std::cout << "[ERROR] Validación falló: token mal formado con ';' incluido: " << token << std::endl;
            std::cout << "[ERROR] Los tokens deben estar separados por espacios" << std::endl;
            return false;
        }
    }
    
    // Check for required directive (like port)
    bool hasPort = false;
    for (size_t i = 0; i < serverTokens.size() - 1; ++i) {
        if (serverTokens[i] == "port") {
            hasPort = true;
            break;
        }
    }
    
    if (!hasPort) {
        std::cout << "[DEBUG] Validación falló: falta directiva 'port'" << std::endl;
        return false;
    }
    
    std::cout << "[DEBUG] Validación exitosa para bloque servidor" << std::endl;
    return true;
}

////

std::pair<int, std::string> ConfigParser::getRedirection(const IConfig* block) const {
    if (!block) {
        return std::make_pair(0, ""); // No hay bloque, no hay redirección
    }

    // 1. Obtener el nodo de configuración para la directiva "return"
    const IConfig* returnNode = block->getChild("return");
    if (!returnNode) {
        return std::make_pair(0, ""); // No hay directiva "return"
    }

    // 2. Obtener la lista de valores (debería ser [código, url])
    const std::vector<std::string>& values = returnNode->getValues();
    if (values.size() < 2) {
        return std::make_pair(0, ""); // Faltan argumentos (código o url)
    }

    // 3. Extraer el código y la URL
    int code = 0;
    const std::string& codeStr = values[0];
    const std::string& url = values[1];

    // Convertir el código de string a int
    std::stringstream ss(codeStr);
    ss >> code;
    if (ss.fail()) {
        return std::make_pair(0, ""); // El código no es un número válido
    }

    // 4. Validar que es un código de redirección HTTP
    if ((code >= 301 && code <= 303) || code == 307 || code == 308) {
        return std::make_pair(code, url); // Devolver el par (código, url) válido
    }

    return std::make_pair(0, ""); // El código no es de redirección
}

////