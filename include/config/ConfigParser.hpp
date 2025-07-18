#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include "../include/libraries.hpp"
#include "../include/config/IConfig.hpp"

class ConfigParser {
    private:
        IConfig* _configRoot;

        std::vector<std::string> tokenize(std::ifstream& file);
        void parse(IConfig* parent, std::vector<std::string>& tokens, size_t& index);

        ConfigParser(const ConfigParser&);
        ConfigParser& operator=(const ConfigParser&);
	
    public:
        ConfigParser();
        static ConfigParser& getInst();
        ~ConfigParser();
            
        bool			load(const std::string& filePath);
        const IConfig*	getConfig() const;
        std::string		getGlobal(const std::string& key) const;
		std::string		getGlobalAlt(IConfig* server, const std::string& key) const;
        std::string		getLocation(const std::string& locationPath, const std::string& key) const;
        std::string		getServerName(const IConfig* serverNode);
        std::string		getDirectiveValue(const IConfig* node, const std::string& key, const std::string& defaultValue) const;
        std::string		getErrorPage(int errorCode, const IConfig* serverNode = NULL) const;
        bool			hasErrorPagesLocation(const IConfig* serverNode = NULL) const;
        bool			validateErrorPagePath(const std::string& filePath) const;
		const std::vector<IConfig*>& getServerBlocks() const;

		const IConfig*	findLocationBlock(const IConfig* serverNode, const std::string& path) const;
		bool			validateServerTokens(const std::vector<std::string>& serverTokens) const;
		bool			isMethodAllowed(const IConfig* serverNode, const std::string& path, const std::string& method) const;

	////
		std::pair<int, std::string> getRedirection(const IConfig* block) const;
	////
	};

#endif
