#include "core/Request.hpp"
#include <sstream>
#include <iostream>
#include "utils/Debug.hpp"

Request::Request(): _cfg(NULL), _serverIndex(-1), _redirection(false)
{}

Request::Request(const Request& other)
{
    *this = other; 
}

Request& Request::operator=(const Request& other) 
{
	if (this != &other) 
    {
		_method = other._method;
		_uri = other._uri;
		_version = other._version;
		_headers = other._headers;
		_body = other._body;
		_path = other._path;
		_queryString = other._queryString;
		_keepAlive = other._keepAlive;
		_physicalPath = other._physicalPath;
		_originalUri = other._originalUri;
		_basePath = other._basePath;
		_cookie = other._cookie;
		_cfg = other._cfg;
		_serverIndex = other._serverIndex;
		_redirection = other._redirection;
	}
	return *this;
}

Request::~Request() 
{}

bool Request::parse(const std::string& raw)
{
    debug << "\n[DEBUG]-------------------[REQUEST] START-------------------" << std::endl;
    
	std::istringstream stream(raw);
    std::string line;

    if (!std::getline(stream, line))
        return (std::cerr << "[DEBUG][Request][parse] getline" << std::endl, false);
        
    std::istringstream firstLine(line);
    if (!(firstLine >> _method >> _uri >> _version))
        return false;
        
    _originalUri = _uri;
    
    size_t q = _uri.find('?');
    if (q != std::string::npos) {
        _path        = _uri.substr(0, q);
        _queryString = _uri.substr(q + 1);
    } else {
        _path        = _uri;
        _queryString.clear();
    }
    debug << "[DEBUG][Request] Start line parsed: " << _method << " " << _uri << " " << _version << "\n";
    debug << "[DEBUG][Request] Path: " << _path << std::endl;
    debug << "[DEBUG][Request] Query String: " << _queryString << std::endl;
    while (std::getline(stream, line) && line != "\r" && line != "\n") {
        size_t pos = line.find(':');
        if (pos == std::string::npos) continue;

        std::string key   = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        debug << "[DEBUG][Request] Header found: " << key << " = " << value << "\n";
        for (size_t i = 0; i < key.size(); ++i) {
            key[i] = std::tolower(key[i]);
        }

        value.erase(0, value.find_first_not_of(" \t"));
        while (!value.empty() && (value[value.size() - 1] == '\r' || value[value.size() - 1] == '\n'))
        {
            value.erase(value.size() - 1);
        }
        _headers[key] = value;
    }

    if (_headers.count("content-length") > 0 || _headers.count("transfer-encoding")) {
        std::stringstream ss;
        ss << stream.rdbuf();
        _body = ss.str();
    }

    if (_version == "HTTP/1.1") {
        _keepAlive = (getHeader("Connection") != "close");
    } else {
        _keepAlive = (getHeader("Connection") == "keep-alive");
    }
   
	if (_method != "POST")
		debug << "[Request] Body: " << _body << std::endl;
	debug << "[Request] POST method body may be to big to print" << std::endl;
	debug << "[Request] Keep-Alive: " << (_keepAlive ? "true" : "false") << std::endl;
	debug << "[Request] Path: " << _path << "\n"
		<< "[Request] Query String: " << _queryString << std::endl;
	debug << "[DEBUG][Request] Request parsing completed successfully." << std::endl;
	debug << "[DEBUG]-------------------[REQUEST] END-------------------\n" << std::endl;
    return (true);
}

const std::string& Request::getPath() const        
{ 
    return _path;
}

const std::string& Request::getQueryString() const
{ 
    return _queryString; 
}

const std::string& Request::Request::getMethod() const 
{
    return _method; 
}

const std::string& Request::getURI() const 
{
    return _uri; 
}

const std::string& Request::getHeader(const std::string& key) const 
{
    static const std::string empty;
	std::map<std::string, std::string>::const_iterator it = _headers.find(key);
	if (it != _headers.end())
		return it->second;
	return empty;
}

const std::string& Request::getBody() const 
{
    return _body; 
}

void Request::setPhysicalPath(const std::string &p)
{
	_physicalPath = p;
}

void Request::setPath(const std::string& path)
{
    _path = path;
}

void Request::setCfg(ConfigParser &parser)
{
    _cfg = &parser;
}

ConfigParser* Request::getCfg() const
{
    return (_cfg);
}

void Request::setServerIndex(size_t serverIndex)
{
	_serverIndex = serverIndex;
}

size_t Request::getServerIndex() const
{
	return (_serverIndex);
}

void Request::setCookie(Cookies &cookie)
{
	_cookie = cookie;
}

const Cookies& Request::getCookie() const
{
	return (_cookie);
}	

void Request::setRedirection(bool redirection)
{
	_redirection = redirection;
}

bool Request::getRedirection() const
{
	return (_redirection);
}
