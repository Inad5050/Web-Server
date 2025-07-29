#ifndef UPLOADHANDLER_HPP
#define UPLOADHANDLER_HPP

#include "libraries.hpp"
#include "response/IResponseBuilder.hpp"
#include "handler/IRequestHandler.hpp"

class UploadHandler : public IRequestHandler 
{
	public:
        UploadHandler(const std::string& uploadsPath, IResponseBuilder* builder, const ConfigParser& cfg);
		virtual		~UploadHandler();
		Response	handleRequest(const Request& req);

	private:
        std::string			_uploadsPath;
        IResponseBuilder*	_builder;
        const ConfigParser&	_cfg;

		Response	getBodyType(const Request& req);

		Response	handleMultipartUpload(const Request& req, std::string contentType);
		std::string	getBoundary(const std::string& contentType);
        bool		parseMultipartBody(const std::string& body, const std::string& boundary, std::string& out_filename, std::string& out_fileContent);
		Response	handleRawUpload(const Request& req);
		Response 	handleUrlEncodedUpload(const Request& req); 
		std::string	urlDecode(const std::string& encoded);

		Response    uploadResponse(const Request &req, int status, const std::string &reason, const std::string &mime, const std::string &body);


		bool		checkCfgPermission(const Request &req, std::string method);
};

#endif
