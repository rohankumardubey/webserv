/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Response.cpp                                       :+:    :+:            */
/*                                                     +:+                    */
/*   By: novan-ve <marvin@codam.nl>                   +#+                     */
/*                                                   +#+                      */
/*   Created: 2021/02/04 23:28:03 by novan-ve      #+#    #+#                 */
/*   Updated: 2021/03/07 16:04:56 by tishj         ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include "Response.hpp"
#include "Utilities.hpp"
#include "Server.hpp"
#include "Properties.hpp"
#include "Cgi.hpp"

bool g_sigpipe;

Response::Response() : server_name(""), location_block(NULL), isDir(false)
{
	this->status_codes[200] = "200 OK";
	this->status_codes[201] = "201 Created";
	this->status_codes[204] = "204 No Content";
	this->status_codes[301] = "301 Moved Permanently";
	this->status_codes[400] = "400 Bad Request";
	this->status_codes[401] = "401 Unauthorized";
	this->status_codes[404] = "404 Not Found";
	this->status_codes[405] = "405 Method Not Allowed";
	this->status_codes[409] = "409 Conflict";
	this->status_codes[413] = "413 Payload Too Large";
	this->status_codes[505] = "505 HTTP Version Not Supported";
}

Response::Response(const Response& other) {

	*this = other;
}

void	Response::setRequest(Request& req)
{
	this->req = req;
	this->response_code = req.get_status_code();
}

int		Response::get_status_code() const
{
	return (this->response_code);
}

Response& Response::operator = (const Response& other)
{
	if (this != &other)
	{
		this->req = other.req;
		this->status_line = other.status_line;
		this->response_code = other.response_code;
		this->status_codes = other.status_codes;
		this->location_block = other.location_block;
		this->isDir = other.isDir;
		this->server_name = other.server_name;
	}
	return (*this);
}

Response::~Response() {}

void	Response::setSigpipe(int)
{
	std::cout << "SIGPIPE received" << std::endl;
	g_sigpipe = true;
}

Response*	responseCopy;
void	Response::sendResponse(int fd)
{
	std::string response;

	responseCopy = this;
	g_sigpipe = false;

	// Copy status line into response
	response.append(this->status_line + "\r\n");

	// Copy headers into response
	for (std::map<std::string, std::string>::const_iterator it = this->headers.begin(); it != this->headers.end(); it++)
		response.append(it->first + ": " + it->second + "\r\n");

	// Copy newline into response to seperate headers and body
	response.append("\r\n");

	// Copy body into response
	if (this->req.get_method() != "HEAD")
	{
		for (std::vector<std::string>::const_iterator it = this->body.begin(); it != this->body.end(); it++)
			response.append(*it + "\r\n");
	}

	//response.append("\r\n");

	if (send(fd, response.c_str(), response.length(), 0) < 0 && !g_sigpipe)
			throw ft::runtime_error("Error: Could not send request to the client");
	if (g_sigpipe)
		responseCopy->response_code = 400;
}

void	Response::printResponse(void) const
{
	// Print values for debugging
	std::cout << std::endl << "Response:" << std::endl;
	std::cout << "  Headers:" << std::endl;
	std::cout << "\t" << this->status_line << "\r" << std::endl;
	for (std::map<std::string, std::string>::const_iterator it = this->headers.begin(); it != this->headers.end(); it++) {
		std::cout << "\t" << it->first << ": " << it->second << "\r" << std::endl;
	}

	if (this->req.get_method() != "HEAD")
	{
		std::cout << "  Body:" << std::endl;
		for (std::vector<std::string>::const_iterator it = this->body.begin(); it != this->body.end(); it++)
			std::cout << "\t" << *it << "\r" << std::endl;
		std::cout << std::endl;
	}
}

void	Response::composeResponse(void)
{

//	this->checkRequestBody();
	if (!this->checkAuthorization())
	{
		this->response_code = 401;
		this->headers["WWW-Authenticate"] = "Basic realm=\"";
		if (this->location_block)
			this->headers["WWW-Authenticate"] += this->location_block->get_properties().auth.realm + "\"";
	}
	this->checkMethod();
	this->checkPath();

	this->handlePut();

	this->setStatusLine();
	this->setServer();
	this->setDate();
	this->setContentType();
	this->setBody();
	this->setContentLen();
	this->setContentLang();
	this->setLocation();
	this->setModified();
}

// void	Response::checkRequestBody(void)
// {
// 	if (!this->req.
// }

bool	Response::checkAuthorization(void)
{
	if (this->location_block && this->location_block->get_properties().auth.enabled)
	{
		std::map<std::string, std::string>& headers = this->req.get_headers();
		if (!this->location_block->get_properties().auth.user_pass.size())
			return (true);
		if (!this->req.get_headers().count("Authorization"))
			return (false);
		std::vector<std::string>	value = ft::split(headers["Authorization"], " ");
		for (size_t i = 0; i < value.size(); i++)
			std::cout << "value[" << i << "] = " << value[i] << std::endl;
		if (value.size() != 2 || value[0] != "Basic" || 
			!ft::onlyConsistsOf(value[1], "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=") ||
			!this->location_block->get_properties().auth(value[1]))
			return (false);
	}
	return (true);
}

void	Response::checkMethod(void)
{
	if (this->response_code != 200)
		return;

	std::map<std::string, bool>	accepted_methods;
	if (this->location_block)
		accepted_methods = this->location_block->get_properties().accepted_methods;

	if (accepted_methods.count(this->req.get_method()) && accepted_methods[this->req.get_method()])
		return ;
	this->response_code = 405;
}

void	Response::checkPath(void)
{
	if (!this->location_block)
		return ;
	this->path = "/" + this->req.uri.get_path();
	if (this->location_block && this->path.size() >= this->location_block->get_location().size() && this->path.substr(0, this->location_block->get_location().size()) == this->location_block->get_location())
		this->path.replace(0, this->location_block->get_location().size(), this->location_block->get_properties().root);
	else
	{
		if (this->path.size() > 1 && this->path[this->path.size() - 1] != '/')
			this->path += "/";
		this->path.replace(0, this->location_block->get_location().size(), this->location_block->get_properties().root);
	}
	if (this->response_code != 200 || (this->req.get_method() == "PUT" && this->req.get_headers().count("Content-Length")))
		return;

	std::cout << "final path: " << this->path << std::endl;

	int fd = open(this->path.c_str(), O_RDONLY);
	if (fd == -1)
	{
		this->response_code = 404;
		return;
	}
	close(fd);

	struct stat s;
	if (stat(this->path.c_str(), &s) == 0)
	{
		if ( s.st_mode & S_IFDIR )
		{
			// ********************** Change to contents of index key from config **********************
			std::vector<std::string>	index;
			if (this->location_block)
				index = this->location_block->get_properties().index;

			if (this->path.size() && this->path[this->path.size() - 1] != '/')
				this->path += "/";
			for (std::vector<std::string>::iterator it = index.begin(); it != index.end(); it++)
			{
				fd = open((this->path + *it).c_str(), O_RDONLY);
				if (fd != -1)
				{
					this->path += *it;
					close(fd);
					return;
				}
			}

			// ********************** Change to autoindex key from config **********************
			
			bool	autoindex = false;
			if (this->location_block)
				autoindex = this->location_block->get_properties().auto_index;

			if (autoindex)
			{
				this->isDir = true;
				if (this->path[this->path.length() - 1] != '/')
					this->response_code = 301;
			}
			else
				this->response_code = 404;
		}
	}
	else
		throw ft::runtime_error("Error: stat failed in Response::checkPath");
}

void	Response::handlePut(void)
{
	if (this->response_code != 200 || this->req.get_method() != "PUT" || !this->req.get_headers().count("Content-Length"))
		return;

	// Check if requested path is a directory
	if (this->path[this->path.length() - 1] == '/')
	{
		this->response_code = 409;
		return;
	}

	// Check if files directory exist (e.g. /nonexistent/new.html)
	int fd = open((this->path.substr(0, this->path.find_last_of('/'))).c_str(), O_RDONLY);
	if (fd == -1)
	{
		this->response_code = 404;
		return;
	}
	close(fd);

	// Check if body is too large
	size_t	bodylen = static_cast<size_t>(ft::stoi(this->req.getBodyLen()));
	if (bodylen > this->location_block->get_properties().client_max_body_size)
	{
		this->response_code = 413;
		return;
	}

	// Check if file exists
	fd = open(this->path.c_str(), O_RDONLY);
	if (fd == -1)
		this->response_code = 201;
	else
		this->response_code = 204;
	close(fd);

	// Check if file is directory
	if (this->response_code == 204)
	{
		struct stat	s;
		if (stat(this->path.c_str(), &s) != 0)
			throw ft::runtime_error("Error: stat failed in Response::handlePut");
		if (s.st_mode & S_IFDIR) {
			this->response_code = 409;
			return;
		}
	}

	fd = open(this->path.c_str(), O_TRUNC | O_CREAT | O_WRONLY, 0644);
	for (std::vector<std::string>::iterator it = req.get_body().begin(); it != req.get_body().end(); it++)
		write(fd, (*it).c_str(), (*it).length());
	close(fd);
}

void	Response::setStatusLine(void)
{
	std::cout << "RESPONSE CODE: " << this->response_code << std::endl;
	this->status_line.append("HTTP/1.1 ");
	this->status_line.append(this->status_codes[this->response_code]);
}

void	Response::setServer(void)
{
	this->headers["Server"] = "webserv/1.0";
}

void	Response::setDate(void)
{
	struct tm 	tm = ft::getTime();
	char		buf[64];

	ft::memset(buf, '\0', 64);
	strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);

	this->headers["Date"] = std::string(buf);
}

void	Response::setContentType()
{
	if (this->response_code == 201 || this->response_code == 204)
		return;
	if (this->response_code != 200 || this->isDir)
	{
		this->headers["Content-Type"] = "text/html";
		return;
	}

	size_t		pos = this->path.find_last_of('.');

	if (pos == std::string::npos)
	{
		this->headers["Content-Type"] = "text/plain";
		return;
	}

	std::string	ext = this->path.substr(pos + 1, path.length() - pos - 1);
	std::string type;

	if (ext == "html" || ext == "css" || ext == "csv" || ext == "xml")
		type = "text/" + ext;
	else if (ext == "pdf" || ext == "json" || ext == "zip")
		type = "application/" + ext;
	else if (ext == "js")
		type = "application/javascript";
	else if (ext == "php")
		type = "application/x-httpd-php";
	else if (ext == "jpg" || ext == "jpeg")
		type = "image/jpeg";
	else if (ext == "gif" || ext == "png" || ext == "tiff")
		type = "image/" + ext;
	else if (ext == "ico")
		type = "image/x-icon";
	else if (ext == "mpeg" || ext == "mp4" || ext == "webm")
		type = "video/" + ext;
	else if (ext == "qt")
		type = "video/quicktime";
	else
		type = "application/octet-stream";

	this->headers["Content-Type"] = type;
}

void	Response::setBody(void)
{
	if (this->response_code == 201 || this->response_code == 204)
		return;
	if (this->response_code != 200)
	{
		this->setBodyError();
		return;
	}

	std::cout << "BODY PATH: " << this->path << std::endl;

	if (this->isDir)
	{
		this->listDirectory();
		return;
	}

	int fd;

	if (this->req.get_path().substr(0, 9) == "/cgi-bin/" ||
		(this->headers["Content-Type"] == "application/x-httpd-php" &&
		!this->location_block->get_properties().php_cgi.empty()))
	{
		fd = 0;
		if (this->headers["Content-Type"] == "application/x-httpd-php")
			fd = open(this->location_block->get_properties().php_cgi.c_str(), O_RDONLY);
		if (fd != -1)
		{
			if (fd)
				close(fd);
			Cgi	c;
			c.execute(&req, this->path.substr(2, path.length() - 2), this->server_name,
					  this->location_block->get_properties().ip_port.second,
					  this->location_block->get_properties().php_cgi);
			fd = open("/tmp/webservout", O_RDONLY);

			// Set default cgi content type to text/html
			if (this->req.get_path().substr(0, 9) == "/cgi-bin/")
				this->headers["Content-Type"] = "text/html";
		}
		else
			fd = open(this->path.c_str(), O_RDONLY);
	}
	else
		fd = open(this->path.c_str(), O_RDONLY);
	if (fd == -1)
		throw ft::runtime_error("Error: Response can't open previously checked file in setBody()");

	this->body = ft::get_lines(fd);

	if (this->req.get_path().substr(0, 9) == "/cgi-bin/" ||
		this->headers["Content-Type"] == "application/x-httpd-php" || this->req.get_method() == "POST")
		this->parseCgiHeaders();

	size_t body_size = 0;
	for (size_t i = 0; i < this->body.size(); i++)
		body_size += this->body[i].size() + ((i + 1 == this->body.size()) ? 0 : 2);
	if (this->location_block && body_size > this->location_block->get_properties().client_max_body_size)
	{
		this->response_code = 413;
		this->setBodyError();
	}

	close(fd);
}

void	Response::setBodyError(void)
{
	bool		error_page_found = false;
	std::map<int, std::string>	error_pages;
	if (this->location_block)
		error_pages = this->location_block->get_properties().error_pages;

	if (this->response_code == 405 && this->location_block)
	{
		size_t i = 0;
		const std::map<std::string, bool>&	accepted_methods = this->location_block->get_properties().accepted_methods;
		for (std::map<std::string, bool>::const_iterator it = accepted_methods.begin(); it != accepted_methods.end(); it++)
		{
			if (it->second)
			{
				if (i)
					this->headers["Allow"] += ", ";
				this->headers["Allow"] += it->first;
				i++;
			}
		}
	}
	if (error_pages.count(this->response_code))
	{

		std::string errorpage = this->location_block->get_properties().root + error_pages[this->response_code];
		std::cout << "Custom error page for " << this->response_code << " is: " << errorpage << std::endl;

		int fd = open(errorpage.c_str(), O_RDONLY);
		if (fd != -1)
		{
			error_page_found = true;
			this->body = ft::get_lines(fd);
			close(fd);
		}
	}
	if (!error_page_found)
	{
		this->body.push_back("<html>");
		this->body.push_back("<head><title>" + this->status_codes[this->response_code] + "</title></head>");
		this->body.push_back("<body>");
		this->body.push_back("<center><h1>" + this->status_codes[this->response_code] + "</h1></center>");
		this->body.push_back("<center><hr>webserv/1.0</center>");
		this->body.push_back("</body>");
		this->body.push_back("</html>");
	}
}

void	Response::listDirectory(void)
{
	struct stat 				result;
	struct dirent				*entry;
	char						buf[64];
	std::vector<std::string>	filenames;
	std::vector<std::string>	dirs;
	std::vector<std::string>	files;
	std::string					full_path = this->req.get_path();

	full_path.resize(full_path.find_last_not_of('/') + 1);

	this->body.push_back("<html>");
	this->body.push_back("<head><title>Index of " + full_path + "</title></head>");
	this->body.push_back("<body>");
	this->body.push_back("<h1>Index of " + full_path + "</h1><hr><pre><a href=\"..\">../</a>");

	DIR *dir = opendir(this->path.c_str());
	if (dir == NULL)
		throw ft::runtime_error("Error: opendir in Response::listDirectory failed");

	if (this->path[this->path.length() - 1] != '/')
		this->path.append("/");

	while ((entry = readdir(dir)) != NULL)
		filenames.insert(filenames.begin(), std::string(entry->d_name));
	closedir(dir);
	for (std::vector<std::string>::iterator it = filenames.begin(); it != filenames.end(); it++)
	{
		if (*it == "." || *it == "..")
			continue;
		if (stat((this->path + *it).c_str(), &result) == 0)
		{
			struct tm 	tm = ft::getTime(result.st_mtime);

			ft::memset(buf, '\0', 64);
			strftime(buf, sizeof(buf), "%d-%b-%Y %H:%M", &tm);

			if ( result.st_mode & S_IFDIR )
			{
				dirs.push_back("<a href=\"" + this->req.get_path());
				if (this->req.get_path()[this->req.get_path().length() - 1] != '/')
					dirs.back().append("/");
				dirs.back().append(*it + "/\">" + *it + "/</a>");
				dirs.back().append(std::string(50 - (*it).length(), ' ') + std::string(buf));
				dirs.back().append(std::string(19, ' ') + "-");
			}
			else if ( result.st_mode & S_IFREG )
			{
				files.push_back("<a href=\"" + this->req.get_path());
				if (this->req.get_path()[this->req.get_path().length() - 1] != '/')
					files.back().append("/");
				files.back().append(*it + "\">" + *it + "</a>");
				files.back().append(std::string(51 - (*it).length(), ' ') + std::string(buf));
				files.back().append(std::string(20 - ft::itos(result.st_size).length(), ' '));
				files.back().append(ft::itos(result.st_size));
			}
		}
	}
	for (std::vector<std::string>::iterator it = dirs.begin(); it != dirs.end(); it++)
		this->body.push_back(*it);
	for (std::vector<std::string>::iterator it = files.begin(); it != files.end(); it++)
		this->body.push_back(*it);

	this->body.push_back("</pre><hr></body>");
	this->body.push_back("</html>");
}

void	Response::parseCgiHeaders(void)
{
	std::vector<std::string>::iterator it = this->body.begin();

	while (it != this->body.end())
	{
		if ((*it).find(':') == std::string::npos)
			break;
		std::pair<std::string, std::string> header = ft::get_keyval(*it, ": ");

		// Make sure header is in title case
		header.first[0] = toupper(header.first[0]);
		for (unsigned long i = 0; i < header.first.length(); i++)
		{
			if (header.first[i] == '-' && i + 1 != header.first.length())
				header.first[i + 1] = toupper(header.first[i + 1]);
		}
		this->headers[header.first] = header.second;
		it = this->body.erase(this->body.begin());
	}
	if (this->body.size() && (this->body.front() == "\r" || this->body.front() == ""))
		this->body.erase(this->body.begin());
}

void	Response::setContentLen(void)
{
	if (this->response_code != 204)
		this->headers["Content-Length"] = this->getBodyLen();
}

void	Response::setContentLang(void)
{
	size_t		html_pos;
	size_t		lang_pos;
	std::string lang;

	for (std::vector<std::string>::iterator it = this->body.begin(); it != this->body.end(); it++)
	{
		html_pos = (*it).find("<html");
		if (html_pos == std::string::npos)
			continue;

		lang_pos = (*it).find("lang", html_pos);
		if (lang_pos != std::string::npos)
		{
			for (size_t i = lang_pos + 6; (*it)[i] != '"'; i++)
				lang += (*it)[i];
			this->headers["Content-Language"] = lang;
			return;
		}
	}
}

Server*	Response::server_match(const std::map<Server*, std::vector<std::string> >& server_names)
{
	Server*	matching_server_name = NULL;
	std::vector<Server *>	best_match;
	std::pair<int, bool>	ip_port_match;
	std::string host = this->req.get_header("Host") + "/";
	URI	host_uri(host);
	if (host_uri.get_host() == "localhost")
		host_uri.set_host("127.0.0.1");
	if (host_uri.get_port() == "")
		host_uri.set_port("80");

	std::cout << "'host' of HOST field: " << host_uri.get_host() << std::endl;
	ip_port_match.second = false;
	ip_port_match.first = 0;

	//gather information;
	for (std::map<Server*, std::vector<std::string> >::const_iterator it = server_names.begin(); it != server_names.end(); it++)
	{
		std::pair<int, bool>	current_match;
		const Properties& server_properties = it->first->get_properties();

//		std::cout << "RESPONSE HOST HEADER: " << this->headers["Host"] << std::endl;
		std::cout << "Server listen - " << server_properties.ip_port.first << ":" << server_properties.ip_port.second << std::endl;
//		std::cout << "Request - " << this->req.uri.get_uri() << " | " << this->req.uri.get_host() << ":" << this->req.uri.get_port() << std::endl;
//		std::cout << "Host: " << host_uri.get_host() << ":" << host_uri.get_port() << std::endl;

		//if server_name matches explicitly
		if ((std::find(server_properties.server_names.begin(), server_properties.server_names.end(), host_uri.get_host()) != server_properties.server_names.end()) || 
			(std::find(server_properties.server_names.begin(), server_properties.server_names.end(), this->req.uri.get_host()) != server_properties.server_names.end()))
			matching_server_name = it->first;

		std::cout << "Server with matching_server_name: " << (void*)matching_server_name << std::endl;

		if (this->req.uri.get_port() == server_properties.ip_port.second || host_uri.get_port() == server_properties.ip_port.second)
			current_match.second = true;
		else
		{
			std::cout << "Port doesn't match" << std::endl;
			continue ; //port has to match explicitly
		}

		if (this->req.uri.get_host() == server_properties.ip_port.first || host_uri.get_host() == server_properties.ip_port.first)
			current_match.first = 2;
		else if (server_properties.ip_port.first == "0.0.0.0") //"any ip" matches, but is less explicit
			current_match.first = 1;
		else
		{
			std::cout << "IP doesn't match" << std::endl;
			continue ;
		}

		//if server_name matches explicitly
		if (std::find(server_properties.server_names.begin(), server_properties.server_names.end(), host_uri.get_host()) != server_properties.server_names.end())
		{
			this->server_name = host_uri.get_host();
			matching_server_name = it->first;
		}
		if (std::find(server_properties.server_names.begin(), server_properties.server_names.end(), this->req.uri.get_host()) != server_properties.server_names.end())
		{
			this->server_name = this->req.uri.get_host();
			matching_server_name = it->first;
		}

		if (current_match.first > ip_port_match.first)
		{
			best_match.clear();
			ip_port_match = current_match;
			best_match.push_back(it->first);
		}
		else if (current_match.first == ip_port_match.first)
			best_match.push_back(it->first);
	}

	//choose
	if (best_match.empty() && !matching_server_name)
	{
//		std::cout << "NO SERVERS MATCHING REQUEST!!" << std::endl;
		return (NULL);
	}
	if (best_match.size() == 1 || (best_match.size() > 1 && matching_server_name == NULL))
		return (best_match[0]);
	else
		return (matching_server_name);
}

void	Response::location_match(const std::map<Server*, std::vector<std::string> >& server_names)
{
	Server* server_block = this->server_match(server_names);

	if (!server_block)
	{
		std::cout << "No server-block found for request!" << std::endl;
		return ;
	}
	else
	{
		std::cout << "SERVER_BLOCK selected:" << std::endl;
		ft::print_iteration(server_block->get_properties().server_names.begin(), server_block->get_properties().server_names.end(), "\n");
		if (this->server_name == "")
			this->server_name = server_block->get_properties().server_names.front();
	}

//	std::cout << "full uri: " << this->req.uri.get_uri() << std::endl;
//	std::cout << "uri path: " << this->req.uri.get_path() << std::endl;
	std::string uri_target = "/" + this->req.uri.get_path();
	std::string location_path = "";

	for (std::map<std::string, Location*>::iterator it = server_block->locations.begin(); it != server_block->locations.end() && !this->location_block; it++)
	{
		std::string location = it->first;
//		std::cout << "CURRENT LOCATION_BLOCK: " << location << std::endl;
		if (uri_target.size() && uri_target[uri_target.size() - 1] != '/')
			uri_target += "/";
		if (uri_target.size() >= location.size() && uri_target.substr(0, location.size()) == location)
		{
//			std::cout << location << " is a match!" << std::endl;
			location_path = location;
			this->location_block = it->second;
		}
	}
	if (!this->location_block)
	{
		std::cout << "No matching location block!" << std::endl;
		return ;
	}
	else
		std::cout << "Location block matching is: " << location_path << std::endl;
}

void	Response::setLocation(void)
{
	if (this->response_code != 301 && this->response_code != 201)
		return;

	std::cout << "Pre" << this->req.get_path() << "Post" << std::endl;
	std::string location = "http://" + this->req.get_header("Host") + this->req.get_path();
	if (this->response_code == 301)
		location += "/";
	this->headers["Location"] = location;
}

void	Response::setModified(void)
{
	if (this->response_code != 200 || this->isDir)
		return;

	struct stat	result;

	if (stat(this->path.c_str(), &result) == 0)
	{
		struct tm 	tm = ft::getTime(result.st_mtime);
		char		buf[64];

		ft::memset(buf, '\0', 64);
		strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
		this->headers["Last-Modified"] = std::string(buf);
	}
}
