#include "../../include/server/Server.hpp"
#include "../../include/core/Request.hpp"
#include "../../include/core/Response.hpp"
#include "../../include/server/ClientBuffer.hpp"
#include "../../include/utils/ErrorPageHandler.hpp"
#include "../../include/config/ConfigParser.hpp"
#include "../include/factory/IHandlerFactory.hpp"
#include "../include/factory/UploadHandlerFactory.hpp"
#include "../include/factory/CGIHandlerFactory.hpp"

Server::Server(ConfigParser& cfg, std::string cgiPath, const std::string& rootPath, std::string uploadPath, IResponseBuilder *builder):
_cfg(cfg), _cgiPath(cgiPath), _rootPath(rootPath), _uploadPath(uploadPath), _responseBuilder(builder), _router(Router(_rootPath)), _error(-1)
{
	IHandlerFactory* staticFactory = new StaticHandlerFactory(_rootPath, _responseBuilder, _cfg);
    _router.registerFactory("/", staticFactory);
	factory_ptr.push_back(staticFactory);
	IHandlerFactory* uploadFactory = new UploadHandlerFactory(_uploadPath, _responseBuilder, _cfg);
    _router.registerFactory("/uploads", uploadFactory);
	factory_ptr.push_back(uploadFactory);
	IHandlerFactory* cgiFactory = new CGIHandlerFactory(_cgiPath, _responseBuilder, _cfg);
    _router.registerFactory("/cgi-bin", cgiFactory);
	factory_ptr.push_back(cgiFactory);
}

Server& Server::getInstance(ConfigParser& cfg, std::string cgiPath, const std::string& rootPath, std::string uploadPath, IResponseBuilder *builder)
{
	static Server server(cfg, cgiPath, rootPath, uploadPath, builder);
	return (server);
}

Server::~Server()
{
	for (std::vector<int>::iterator it = listen_sockets.begin(); it != listen_sockets.end(); ++it)
		close(*it);
	listen_sockets.clear();
	for (std::vector<IHandlerFactory*>::iterator it = factory_ptr.begin(); it != factory_ptr.end(); ++it)
		delete *it;
	factory_ptr.clear();
	delete _responseBuilder;
}

void Server::startEpoll()
{
	int							epollfd;
	std::vector<int>			clientFdList;
	std::map<int, Response> 	pending_writes;
	std::map<int, ClientBuffer> client_buffers;
	int 						client_fd;

	if ((epollfd = init_epoll()) < 0)
		return ;

	struct epoll_event events[MAX_EVENTS];
	while (g_signal_received == 0)
	{
		int event_nmb = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		if (event_nmb == -1)
		{
			std::cout << "[DEBUG][LOOP EPOLL] epoll_wait() ended, epollfd = " << epollfd << std::endl;
			break;
		}
		for (int i = 0; i < event_nmb; i++)
		{
			std::cout << "------------------------LOOP_EPOLL++------------------------" << std::endl;
			client_fd = events[i].data.fd;
			if (std::find(listen_sockets.begin(), listen_sockets.end(), client_fd) != listen_sockets.end())
			{
				std::cout << "[DEBUG][LOOP_EPOLL] accept_connection" << std::endl;
				accept_connection(client_fd, epollfd, clientFdList, client_buffers);
			}
			else if (events[i].events & EPOLLERR || events[i].events & EPOLLHUP || !(events[i].events & (EPOLLIN | EPOLLOUT)))
			{
				std::cout << "[DEBUG][LOOP_EPOLL] EPOLLERR EPOLLHUP" << std::endl;
				close_fd(client_fd, epollfd, clientFdList, pending_writes, client_buffers);
			}
			else
			{
				if (events[i].events & EPOLLIN)
				{
					std::cout << "[DEBUG][LOOP_EPOLL] EPOLLIN START"<< std::endl;			
					if (handleClientRead(client_fd, pending_writes, client_buffers[client_fd]))
						close_fd(client_fd, epollfd, clientFdList, pending_writes, client_buffers);
					if (client_buffers[client_fd].getFinishedReading() == true && ft_epoll_ctl(client_fd, epollfd, EPOLL_CTL_MOD, EPOLLOUT))
						close_fd(client_fd, epollfd, clientFdList, pending_writes, client_buffers);
					std::cout << "[DEBUG][LOOP_EPOLL] EPOLLIN END" << std::endl;
				}
				if (events[i].events & EPOLLOUT)
				{
					std::cout << "[DEBUG][LOOP_EPOLL] EPOLLOUT START" << std::endl;
					if (handleClientResponse(client_fd, pending_writes))
						close_fd(client_fd, epollfd, clientFdList, pending_writes, client_buffers);
					else if (ft_epoll_ctl(client_fd, epollfd, EPOLL_CTL_MOD, EPOLLIN))
						close_fd(client_fd, epollfd, clientFdList, pending_writes, client_buffers);
					client_buffers[client_fd].reset();
					std::cout << "[DEBUG][LOOP_EPOLL] EPOLLOUT END" << std::endl;
				}
			}
		}
    }
	freeEpoll(epollfd, clientFdList);
}

int Server::init_epoll()
{
	int epollfd = epoll_create(1);
	if (epollfd < 0)
		return (std::cerr << "[ERROR] on epoll_create" << std::endl, -1);

	if (listen_sockets.empty())
		return (std::cerr << "[ERROR] No listen_sockets to add to epoll" << std::endl, -1);
	for (std::vector<int>::iterator it = listen_sockets.begin(); it != listen_sockets.end(); ++it)
		if (ft_epoll_ctl(*it, epollfd, EPOLL_CTL_ADD, EPOLLIN))
			return (close(*it), std::cerr << "[ERROR] Couldn't add initial listen socket to epoll" << std::endl, -1);
	return (epollfd);
}

int Server::accept_connection(int listen_socket, int epollfd, std::vector<int> &clientFdList, std::map<int, ClientBuffer> &client_buffers)
{
	int client_fd;

	if ((client_fd = ::accept(listen_socket, NULL, NULL)) < 0)
		return (std::cerr << "[ERROR] accepting incoming connection, fd = " << client_fd << std::endl, 1);
	if (ft_epoll_ctl(client_fd, epollfd, EPOLL_CTL_ADD, EPOLLIN))
		return (close(client_fd), std::cerr << "[ERROR] accepting incoming connection, fd = " << client_fd << std::endl, 1);
	
	fcntl(client_fd, F_SETFL, O_NONBLOCK);

	clientFdList.push_back(client_fd);
	ClientBuffer newClientBuffer;
	client_buffers[client_fd] = newClientBuffer;
	
	std::cout << "[DEBUG][accept_connection] New connection accepted() fd = " << client_fd << std::endl;
	return (0);
}

int	Server::ft_epoll_ctl(int fd, int epollfd, int mod, uint32_t events)
{
	struct epoll_event event_struct;
	event_struct.events = events;
	event_struct.data.fd = fd;
	if (epoll_ctl(epollfd, mod, fd, &event_struct) < 0)
		return (1);
	return (0);
}

void Server::close_fd(const int fd, int epollfd, std::vector<int> &clientFdList,  std::map<int, Response> &pending_writes, std::map<int, ClientBuffer> &client_buffers)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);

	std::vector<int>::iterator it = std::find(clientFdList.begin(), clientFdList.end(), fd);
	if (it != clientFdList.end())
		clientFdList.erase(it);
	pending_writes.erase(fd);
	client_buffers.erase(fd);
	close(fd);
	std::cout << "[DEBUG][close_fd] client_fd: " << fd << " closed" << std::endl;
}

void Server::freeEpoll(int epollfd, std::vector<int> &clientFdList)
{
	if (epollfd < 0)
		return;
	for (std::vector<int>::iterator it = listen_sockets.begin(); it != listen_sockets.end(); ++it)
		epoll_ctl(epollfd, EPOLL_CTL_DEL, *it, NULL);
	for (std::vector<int>::iterator it = clientFdList.begin(); it != clientFdList.end(); ++it)
	{
		epoll_ctl(epollfd, EPOLL_CTL_DEL, *it, NULL);
		close(*it);
	}
	close(epollfd);
	clientFdList.clear();
}

int Server::handleClientRead(const int client_fd, std::map<int, Response> &pending_writes, ClientBuffer &additive_bff)
{
	char     str_buffer[BUFFER_SIZE];
    ssize_t  n = recv(client_fd, str_buffer, sizeof(str_buffer) - 1, 0);
    if (n == 0) 
		return (std::cout << "[DEBUG][handleClientRead] Client fd = " << client_fd << " closed connection" << std::endl, 1);
	if (n < 0)
		return (0);

	std::string newBuffer(str_buffer, n);
	additive_bff.add_buffer(newBuffer);
	try
	{
		if (!readRequest(client_fd, additive_bff))
			return (0);
	}
	catch (const std::runtime_error &e)
	{
		std::cerr << "[ERROR][[   [CATCH]   ]]" << std::endl;
		std::cerr << e.what() << std::endl;
		additive_bff.setFinishedReading(true);
		Response res = serverError(e.what(), additive_bff);
		pending_writes[client_fd] = res;
		return (0);
	}
	std::cout << "[DEBUG] [[  FINISHED READING REQUEST  ]]" << std::endl;

	Response res = createResponse(additive_bff);
	pending_writes[client_fd] = res;
	return (0);
}

Response Server::serverError(std::string description, ClientBuffer &additive_bff)
{
	std::cout << "[DEBUG][serverError] START" << std::endl;

	Request  req;
	req.parse(additive_bff.get_buffer());
	req.setCfg(_cfg);
	size_t serverIndex = findServerIndex(req);
	req.setServerIndex(serverIndex);
	
	Payload payload;
	if (_error == 500)
		payload = createServerError(500, "Internal Server Error", description, req);
	else if (_error == 404)
		payload = createServerError(404, "Not Found", description, req);
	else
		payload = createServerError(400, "Bad Request", description, req);
	
    return (_responseBuilder->build(payload));
}

Payload Server::createServerError(size_t status, std::string reason, std::string description, Request& req)
{
	std::cout << "[DEBUG][createServerError] START, error = " << status << std::endl;
	
	_error = -1;

	Payload payload;
	ErrorPageHandler errorHandler(_rootPath);
	payload.body = errorHandler.render(req, status, description);
	payload.keepAlive = false;
	payload.status = status;
	payload.reason = reason;
	payload.mime = "text/html";
	return (payload);
}

Response Server::createResponse(ClientBuffer &additive_bff)
{
	std::cout << "[DEBUG][createResponse] START" << std::endl;
	
	Request  req;
	req.parse(additive_bff.get_buffer());
	req.setCfg(_cfg);
	size_t serverIndex = findServerIndex(req);
	req.setServerIndex(serverIndex);

    // --- INICIO DE LA LÓGICA DE REDIRECCIÓN ---
	std::cout << "[DEBUG][processRedirection] pre START" << std::endl;
    Response redirectResponse;
    if (processRedirection(req, redirectResponse)) 
	{
        return redirectResponse;
    }
    // --- FIN DE LA LÓGICA DE REDIRECCIÓN ---

	if (COOKIES == true)
		checkCookies(req);

	Response res;
	IRequestHandler* handler = _router.resolve(req);
    if (handler) 
	{
        res = handler->handleRequest(req);
		delete handler;
    } 
	else
	{
		_error = 404;
		std::string description = "Handler failure at Server::createResponse"; 
		res = serverError(description, additive_bff);
	}
	return (res);
}

int Server::handleClientResponse(const int client_fd,  std::map<int, Response> &pending_writes)
{
	std::cout << "[DEBUG][handleClientResponse] START" << std::endl;
	
	Response& res = pending_writes[client_fd];
	std::string response = res.toString();

	ssize_t bytes_sent = send(client_fd, response.c_str(), response.length(), 0);
	if (bytes_sent == 0)
		return (std::cerr << "[ERROR][handleClientResponse] No data sent: " << client_fd << std::endl, 1);
	if (bytes_sent < 0)
		return (std::cerr << "[ERROR][handleClientResponse] Client disconnected: " << client_fd << std::endl, 1);
	
	std::string connection = res.getHeader("Connection"); 

	std::cout << "[DEBUG][handleClientResponse] Connection = " << connection << std::endl;

	bool should_we_close = (connection == "close");

	std::cout << "[DEBUG][handleClientResponse] should_we_close = " << should_we_close << std::endl;

	pending_writes.erase(client_fd);
	if (should_we_close)
		return (std::cout << "[DEBUG][handleClientResponse] END connection closed" << std::endl, 1);
	else
		return (std::cout << "[DEBUG][handleClientResponse] END connection alive" << std::endl, 0);
}

void Server::checkCookies(Request &req)
{
	
	std::string cookieHeader = req.getHeader("cookie");
	if (cookieHeader.empty())
	{
		Cookies newCookie;
		newCookie = createCookie();
		_cookieList[newCookie.getKey()] = newCookie;
		req.setCookie(newCookie);
	}
	else
	{
		size_t pos = cookieHeader.find("session_id");
		if (pos == std::string::npos)
		{
			std::cerr << "[ERROR][checkCookies] cannot find session_id"  << std::endl;
			return;
		}
		std::string cookieKey = cookieHeader.substr(pos + 11);
		Cookies &oldCookie = _cookieList[cookieKey];
		oldCookie.increaseConnections();
		req.setCookie(oldCookie);
	}
}

Cookies Server::createCookie()
{
	Cookies newCookie;

	time_t creation_time;
	time(&creation_time);
	size_t cookieKeyNmb = static_cast<size_t>(creation_time);
	std::stringstream ss;
	ss << cookieKeyNmb;
	std::string cookieKey = ss.str();
	newCookie.setKey(cookieKey);

	return (newCookie);
}

////
namespace {
    // Esta función auxiliar se mantiene igual
    std::string getReasonForStatus(int code) {
        switch (code) {
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 303: return "See Other";
            case 307: return "Temporary Redirect";
            case 308: return "Permanent Redirect";
            default: return "Redirect";
        }
    }
}

bool Server::processRedirection(const Request& req, Response& redirectResponse)
{
    std::cout << "[DEBUG][processRedirection] START" << std::endl;
	
	// 1. Obtener los bloques de configuración para la petición actual.
    const IConfig* serverBlock = _cfg.getServerBlocks()[req.getServerIndex()];
    const IConfig* locationBlock = _cfg.findLocationBlock(serverBlock, req.getPath());

	std::cout << "[DEBUG][processRedirection] 1" << std::endl;
    // 2. Comprobar si el bloque 'location' tiene una directiva 'return'.
    std::pair<int, std::string> redirection = _cfg.getRedirection(locationBlock);

	std::cout << "[DEBUG][processRedirection] 2" << std::endl;
    // 3. Si no se encontró en 'location', comprobar en el bloque 'server' como fallback.
    if (redirection.first == 0) {
        redirection = _cfg.getRedirection(serverBlock);
    }

	std::cout << "[DEBUG][processRedirection] 3" << std::endl;
    // 4. Si NO se encontró una regla de redirección, devolver false.
    if (redirection.first == 0) {
        return false;
    }

    // 5. Si se encontró, construir la respuesta de redirección.
    std::cout << "[DEBUG][processRedirection] Redirection triggered: " 
              << redirection.first << " -> " << redirection.second << std::endl;

    Payload payload;
    payload.status = redirection.first;
    payload.reason = getReasonForStatus(redirection.first);
    payload.body = "";
    payload.keepAlive = true;

    redirectResponse = _responseBuilder->build(payload);
    redirectResponse.setHeader("Location", redirection.second);
    redirectResponse.setHeader("Content-Length", "0");

	std::cout << "[DEBUG][processRedirection] 4" << std::endl;
    
    return true; // Indicar que la redirección se ha procesado.
}
////
