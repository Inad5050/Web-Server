/* ************************************************************************** */
/* webserv main                                */
/* ************************************************************************** */

#include <iostream>
#include <fstream>
#include <csignal>

#include "libraries.hpp"
#include "config/ConfigParser.hpp"
#include "server/Server.hpp"
#include "utils/Utils.hpp"
#include "router/Router.hpp"
#include "factory/StaticHandlerFactory.hpp"
#include "factory/UploadHandlerFactory.hpp"
#include "factory/CGIHandlerFactory.hpp"
#include "response/DefaultResponseBuilder.hpp"
#include "config/validateRoot.hpp"
#include "utils/Debug.hpp"

volatile sig_atomic_t g_signal_received = 0;

static void sigHandler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM) g_signal_received = 1;
    debug << "\n❎ [DEBUG] Signal received, shutting down…" << std::endl;
}

static std::string getDirectiveValue(const IConfig* node, const std::string& key, const std::string& defaultValue) 
{
    if (!node) 
        return defaultValue;

    const IConfig* child = node->getChild(key);
    if (child && !child->getValues().empty())
        return child->getValues()[0];

    return defaultValue;
}

int main(int argc, char** argv) 
{
    if (argc != 2) 
    {
        std::cerr << "Usage: " << argv[0] << " <file.conf>" << std::endl;
        return EXIT_FAILURE;
    }

    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);
    std::signal(SIGPIPE, SIG_IGN);

    ConfigParser& parser = ConfigParser::getInst();
    if (!parser.load(argv[1])) 
    {
        std::cerr << "[ERROR][main] fatal: Cannot load config file." << std::endl;
        return EXIT_FAILURE;
    }

    const IConfig* rootConfig = parser.getConfig();
    if (!rootConfig || rootConfig->getChildren().empty()) 
    {
        std::cerr << "[ERROR][main] fatal: config file doesn't contain 'server' blocks." << std::endl;
        return EXIT_FAILURE;
    }

    const IConfig* serverNode = rootConfig->getChildren()[0];

    try 
	{
        validateRoot validator(argv[1]);
        validator.validationRoot();

        std::string serverName = parser.getServerName(serverNode);
        debug << "[INFO] Server name: " << serverName << std::endl;
        std::string bodySize = getDirectiveValue(serverNode, "body_size", "1000000");
        debug << "[INFO] Max body size: " << bodySize << std::endl;
        std::string getAllowedValue = getDirectiveValue(serverNode, "get_allowed", "true");
        debug << "[INFO] GET method allowed: " << getAllowedValue << std::endl;
        std::string postAllowedValue = getDirectiveValue(serverNode, "post_allowed", "true");
        debug << "[INFO] POST method allowed: " << postAllowedValue << std::endl;
        std::string deleteAllowedValue = getDirectiveValue(serverNode, "delete_allowed", "false");
        debug << "[INFO] DELETE method allowed: " << deleteAllowedValue << std::endl;
        std::string rootPathConf = getDirectiveValue(serverNode, "root", "./www");
        std::string rootPath = Utils::resolveAndValidateDir(rootPathConf);

        const IConfig* cgiLocationNode = NULL;
        const IConfig* uploadLocationNode = NULL;

        const std::vector<IConfig*>& locations = serverNode->getChildren();
        for (size_t i = 0; i < locations.size(); ++i) {
            const std::string& path = locations[i]->getValues()[0];
            if (path == "/cgi-bin") cgiLocationNode = locations[i];
            if (path == "/uploads") uploadLocationNode = locations[i];
        }

        std::string cgiDirRaw = getDirectiveValue(cgiLocationNode, "root", "cgi-bin");
        debug << "[DEBUG] CGI factory raw: " << cgiDirRaw << std::endl;
        std::string cgiDir = Utils::resolveAndValidateDir(rootPath + "/" + cgiDirRaw);
        debug << "[DEBUG] CGI directory: " << cgiDir << std::endl;

        std::string cgiPath = getDirectiveValue(cgiLocationNode, "cgi_path", "/usr/bin/python");

        std::string uploadPathRaw = getDirectiveValue(uploadLocationNode, "upload_path", "uploads");
        std::string uploadPath = Utils::resolveAndValidateDir(rootPath + "/" + uploadPathRaw);

        if (rootPath.empty() || uploadPath.empty() || cgiDir.empty())
            throw std::runtime_error("Alguna de las rutas 'root', 'upload_path' o 'cgiDir' no es válida.");

        IResponseBuilder* responseBuilder = new DefaultResponseBuilder();
        Server& server = Server::getInstance(parser, cgiDir, rootPath, uploadPath, responseBuilder);
		server.setUpServers();

        std::cout << "\n✅ [DEBUG] Webserv setup. Listening for connections" << std::endl;

        server.startEpoll();
    }
	catch (const std::exception& e)
	{
        std::cerr << "[ERROR][main] fatal error during set up: " << e.what() << "\n❎Closing..." << std::endl;
        return EXIT_FAILURE;
    }

    debug << "[DEBUG] Server closed" << std::endl;
    return EXIT_SUCCESS;
}
