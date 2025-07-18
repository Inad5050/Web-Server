#pragma once

#include "../include/libraries.hpp"
#include "../include/config/ConfigParser.hpp"
#include "../include/router/Router.hpp"
#include "../include/core/Request.hpp"
#include "../include/server/ClientBuffer.hpp"
#include "../include/core/Response.hpp"
#include "../include/factory/StaticHandlerFactory.hpp"
#include "../include/server/Cookies.hpp"

extern volatile sig_atomic_t g_signal_received;

class IResponseBuilder;

class Server
{
	private:
		ConfigParser& _cfg;
		std::string _cgiPath;
		std::string _rootPath;
		std::string _uploadPath;
		IResponseBuilder* _responseBuilder;
		Router		_router;
		std::vector<int> listen_sockets;
		size_t		_error;
		std::vector<IHandlerFactory*> factory_ptr;
		std::vector<IConfig*> _serverList;
		std::map<std::string, Cookies> _cookieList;
        
        Server(ConfigParser& cfg, std::string cgiPath, const std::string& rootPath, std::string uploadPath, IResponseBuilder *builder);
		Server(const Server& other);
        Server& operator=(const Server& other);

		int			addListeningSocket(IConfig* server);
        void		getHostAndPort(IConfig* server, std::string &host, std::string &port);
        static      void    closeAddListeningSocket(const std::string &str, struct addrinfo *output, int listen_socket);

		int			init_epoll();
        int			accept_connection(int listen_socket, int epollfd, std::vector<int> &client_fds, std::map<int, ClientBuffer> &client_buffers);
		int			ft_epoll_ctl(int fd, int epollfd, int mod, uint32_t events);
		void		close_fd(const int socket, int epollfd, std::vector<int> &container, std::map<int, Response> &pending_writes, std::map<int, ClientBuffer> &client_buffers);
        void		freeEpoll(int epollfd, std::vector<int> &client_fds);

		int			readRequest(int client_fd, ClientBuffer &additive_bff);
		bool		getCompleteHeader(ClientBuffer &additive_bff, Request &req);
		void		checkBodyLimits(ClientBuffer &additive_bff,  Request &req);
		bool		checkIsChunked(ClientBuffer &additive_bff, const Request &req);
		bool		checkIsContentLength(ClientBuffer &additive_bff, Request &req);
		void		checkMaxContentLength(std::string contentLength, ssize_t chunkedReadBytes, Request &req);
		bool		areWeFinishedReading(ClientBuffer &additive_bff, Request &req);
		void		validateChunkedBody(ClientBuffer &additive_bff);
		size_t		findServerIndex(const Request &req);

		int			handleClientRead(const int client_fd, std::map<int, Response> &pending_writes, ClientBuffer &additive_bff);
		Response	serverError(std::string description, ClientBuffer &additive_bff);
		Payload		createServerError(size_t status, std::string reason, std::string description, Request& req);
		Response	createResponse(ClientBuffer &additive_bff);
		int			handleClientResponse(const int client_fd, std::map<int, Response> &pending_writes);

		void		checkCookies(Request &req);
		Cookies		createCookie();

		////
		bool processRedirection(const Request& req, Response& redirectResponse);
		////

	public:
		static Server&	getInstance(ConfigParser& cfg, std::string cgiPath, const std::string& rootPath, std::string uploadPath, IResponseBuilder *builder);
        ~Server();
		void			setUpServers();
		void			setRouter(const Router &router);
        void			startEpoll();
};
