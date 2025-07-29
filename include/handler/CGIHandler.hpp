#pragma once

#include "libraries.hpp"

#include "handler/IRequestHandler.hpp"
#include "core/Request.hpp"
#include "core/Response.hpp"
#include "response/IResponseBuilder.hpp"

class IResponseBuilder;

class CGIHandler: public IRequestHandler
{
	private:
        std::string			_cgiRoot;
		std::string			_cgiDir;
		IResponseBuilder*	_builder;
		Response			_resDefault;
        const ConfigParser& _cfg;
       
		Response	handleCGI(const Request &req);
		Response	handleGET(const Request &req, std::string interpreter);
        Response	handlePOST(const Request &req, std::string interpreter);
		int			createResponse(std::string output, Response &res);
		bool		checkScriptTime(time_t time_start);
		
		int			identifyScriptType(const Request &req);
		Response	autoindexCGIAux(const Request &req);
		int			identifyMethod(const Request &req);
		void		checkCfgPermission(const Request &req, std::string method);
		
		Response	getScript(const Request &req, std::map<std::string, std::string> &map);
		std::string	getScriptName(const std::string &uri);
		std::string	getScriptQuery(const std::string &uri);
		int			checkScriptAccess(std::string &directory, std::string &name);
		bool		getEnviroment(std::vector<std::string> &env, std::string method, std::string path, std::string queryString, const Request &req);            

		Response	CGIerror(const Request &req,int status, std::string reason, std::string mime);
    
	public:        
        CGIHandler(const std::string& cgiRoot, IResponseBuilder* builder, const ConfigParser& cfg);
        virtual ~CGIHandler();    
        virtual Response handleRequest(const Request& req);
};
