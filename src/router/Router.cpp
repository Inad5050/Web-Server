#include "router/Router.hpp"
#include "utils/Utils.hpp"
#include "utils/ErrorPageHandler.hpp"
#include "utils/Debug.hpp"
#include <iostream>

Router::Router(const std::string& absRoot) : _absRoot(absRoot)
{
    debug << "[DEBUG][Router] absRoot: " << _absRoot << std::endl;
}
Router::Router(const Router& other) 
{
	*this = other;
}

Router& Router::operator=(const Router& other) 
{
	if (this != &other)
		this->_routes = other._routes;
	return (*this);
}

Router::~Router() 
{
    debug << "[DEBUG] Router destructor called" << std::endl;
}

void Router::registerFactory(const std::string& pathPrefix, IHandlerFactory* factory)
{
	_routes[pathPrefix] = factory;
}

IRequestHandler* Router::resolve(Request& request) const
{
    Response res;

    const std::string& uriWithQuery = request.getURI();
    std::string uri = getUriWithoutQuery(uriWithQuery);

    debug << "[DEBUG][Router] URI sin query: " << uri << std::endl;

    for (std::map<std::string,IHandlerFactory*>::const_reverse_iterator it = _routes.rbegin(); it != _routes.rend(); ++it)
    {
        if (uri == it->first || uri.find(it->first) == 0)
        {
            try 
			{
                std::string basePath = it->first;
                request.setBasePath(basePath);

                std::string relativePath = uri.substr(basePath.length());
                if (relativePath.empty() || relativePath[0] != '/')
                    relativePath = "/" + relativePath;

                debug << "[DEBUG][Router::resolve] relativePath = " << relativePath << std::endl;
                debug << "[DEBUG][Router::resolve] uri = " << uri << std::endl;
				
                std::string abs = Utils::mapUriToPath(_absRoot, uri);

                debug << "[DEBUG][Router::resolve] abs = " << abs << std::endl;

				if (request.getMethod() != "POST")
				{
                	std::string safe = Utils::validateFilesystemEntry(abs);
                	request.setPhysicalPath(safe);
				}
				else
					request.setPhysicalPath(abs);
                return it->second->createHandler();
            }
            catch (const std::exception& e) 
			{
                std::cerr << "[ERROR][Router::resolve] Sanitización fallida: " << e.what() << std::endl;

                ErrorPageHandler errorHandler(_absRoot);
                std::string body = errorHandler.render(request, 403, "Acceso no permitido");

                res.setStatus(403, "Forbidden");
                res.setBody(body);
                res.setHeader("Content-Type", "text/html");
                res.setHeader("Content-Length", Utils::intToString(body.length()));

                return NULL;
            }
        }
    }
    return NULL;
}

std::string Router::getUriWithoutQuery(std::string uriWithString) const
{
	size_t pos;
	pos = uriWithString.find("?");
	if (pos == std::string::npos)
		return (uriWithString);
	std::string uri = uriWithString.substr(0, pos);
	return (uri);
}
