/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Request.cpp                                        :+:    :+:            */
/*                                                     +:+                    */
/*   By: tbruinem <tbruinem@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2021/02/02 19:37:38 by tbruinem      #+#    #+#                 */
/*   Updated: 2021/03/03 11:25:37 by tishj         ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>

#include "includes/Request.hpp"
#include "includes/Exception.hpp"
#include "Utilities.hpp"
#include "Method.hpp"

Request::Request() : uri(""), done(false), status_line(""), status_code(200), method(GET), body_read(0), body_total(-1), body_started(false) {}

Request::Request(const Request& other) : uri(other.uri), method(other.method)
{
	*this = other;
}

Request& Request::operator = (const Request& other)
{
	if (this != &other)
	{
		this->done = other.done;
		this->status_line = other.status_line;
		this->lines = other.lines;
		this->status_code = other.status_code;
		this->method = other.method;
		this->path = other.path;
		this->headers = other.headers;
		this->body = other.body;
		this->uri = other.uri;
		this->body_read = other.body_read;
		this->body_total = other.body_total;
		this->body_started = other.body_started;
	}
	return (*this);
}

bool	Request::isMethod(std::string str)
{
	static const char *methods[] = {
	"OPTIONS",
	"GET",
	"HEAD",
	"POST",
	"PUT",
	"DELETE",
	"TRACE",
	"CONNECT"
	};

	for (size_t i = 0; i < sizeof(methods) / sizeof(char *); i++)
		if (std::string(methods[i]) == str)
			return (true);
	return (false);
}

void	Request::process(int fd)
{
	int		ret = 0;

	std::vector <std::string> lines_read = ft::get_lines(fd, "\r\n", &ret, (!this->lines.empty() && this->body_total != -1));

	 if (ret == -1)
	 {
	 	this->status_code = 400;
	 	this->done = true;
	 }

	// if (lines_read.size() == 1 && !lines_read[0].size())
	// 	return ;
	for (std::vector<std::string>::iterator it = lines_read.begin(); it != lines_read.end() && !this->done; it++)
	{
		std::cout << "REQUEST: " << *it << std::endl;
		this->done = parseLine(*it);
	}
	// if (this->done)
	// 	std::cout << "REQUEST IS DONE!" << std::endl;
}

Request::~Request() {}

//true if line is status_line, false otherwise
bool Request::isStatusLine(const std::string &line)
{
	size_t i = 0;
	if (!line.size() || line.size() >= 8000)
		return (false);
	std::vector<std::string>	parts = ft::split(line, " \r", " \r");
	//First part has to be METHOD
	if (parts[i++].find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") != std::string::npos)
		return (false);
//	std::cout << "METHOD IS ONLY CAPS\n";
	//Has to have atleast one space
	if (parts[i++] != " ")
		return (false);
//	std::cout << "SEPARATED BY SPACE\n";
	//Skip over all spaces
	for (; parts[i] == " "; i++) {}
	//request-target has to start with '/'
	// if (parts[i][0] != '/')
	// 	return (false);
	//Request-line has to end on carriage return
	size_t end = parts.size() - 1;
//	std::cout << "ENDS ON CARRIAGE RETURN\n";
	//Skip over all spaces that are at the end of the request-line
	for (;end > 0 && parts[end] == " "; end--) {}
	//Check if the version consists of HTTP/[0-9][0-9]
	if (parts[end].substr(0, 5) != "HTTP/")
		return (false);
//	std::cout << "PROTOCOL IS 'HTTP/'\n";
	std::vector<std::string>	version = ft::split(parts[end].substr(5, parts[end].size()), ".");
	if (version.size() != 2)
		return (false);
//	std::cout << "VERSION SIZE OF 2 ([0-9]\\.[0-9])\n";
	if (version[0].find_first_not_of("0123456789") != std::string::npos)
		return (false);
	if (version[1].find_first_not_of("0123456789") != std::string::npos)
		return (false);
//	std::cout << "VERSION IS ONLY NUMBERS\n";

	//Protocol has to have a minimum of one space before it
	if (parts[--end] != " ")
		return (false);
//	std::cout << "PROTOCOL SEPARATED FROM REQUEST-TARGET BY SPACE\n";
	if (i >= end)
		return (false);
	//Skip over all spaces preceding Protocol
	for (;end > i && parts[end] == " "; end--) {}

	//concatenate every part of the request-target together
	std::string request_target;
	for (--end; i < end; i++)
		request_target += parts[i];
	
	return (true);
}

bool	Request::parseLine(std::string line)
{
	if (line.empty())
	{
		if (this->status_line.empty())
			return false;
		else if (this->status_line.size() && this->lines.size() == 0)
		{
			this->status_code = 400;
			return (true);
		}
		else if (this->status_line.size() && this->lines.size() > 0)
		{
			if (this->body_total != 0 && this->body_total > this->body_read)
			{
				if (this->body_started)
				{
					this->body_read+= 2;
					this->lines.push_back("\r\n");
				}
				else
					this->lines.push_back("\r");
				this->body_started = true;
				return false;
			}

			int	end_pos_method = this->status_line.find(' ');
			int start_pos_path = end_pos_method;

			while (this->status_line[start_pos_path] == ' ')
				start_pos_path++;

			// Set end_pos_path to character before the \r\0 and remove whitespaces
			int end_pos_path = this->status_line.length() - 1;
			while (this->status_line[end_pos_path] == ' ')
				end_pos_path--;
			// Move back to first character before HTTP/1.1
			end_pos_path -= 8;

			// Remove white spaces and up 1 to get character right after path
			while (this->status_line[end_pos_path] == ' ')
				end_pos_path--;
			end_pos_path++;

			std::string methodpart = this->status_line.substr(0, end_pos_method);
			if (isMethod(methodpart))
				this->method = Method(methodpart);
			else
				this->status_code = 405;
			this->path = this->status_line.substr(start_pos_path, end_pos_path - start_pos_path);
			std::cout << "Path: " << this->path << std::endl;
			this->uri = URI(path);
			if (this->uri.get_port() == "" && this->uri.get_scheme() == "HTTP")
				this->uri.set_port("80");
			this->splitRequest();
			this->printRequest();

			return (true);
		}
	}
	else if (isStatusLine(line))
	{
		if (this->status_line == "")
		{
			int start = line.length() - 1;

			while (line[start] == ' ' || (line[start] >= 10 && line[start] <= 13))
				start--;

			int end = start;

			while (line[start] != ' ')
				start--;

			if (line.substr(start + 1, end - start) != "HTTP/1.1")
			{
				this->status_code = 505;
				return (true);
			}
			this->status_line = line;
		}
		return (false);
	}
	else if (line.find(':') != std::string::npos && !this->body_started)
	{
		if (this->status_line == "")
		{
			this->status_code = 400;
			return (true);
		}
		size_t carriage_return = line.find_last_of('\r');

		if (line.substr(0, 16) == "Content-Length: ")
		{
			if (carriage_return != std::string::npos)
				this->body_total = ft::stoi(line.substr(16, line.length() - 17));
			else
				this->body_total = ft::stoi(line.substr(16, line.length() - 16));
		}

		if (carriage_return != std::string::npos && carriage_return + 1 == line.size())
			this->lines.push_back(line.substr(0, line.size() - 1));
		else
			this->lines.push_back(line);
		return (false);
	}
	if (this->status_line != "")
	{
		if (body_started)
		{
			size_t carriage = line.find_last_of('\r');
			if (carriage != std::string::npos && carriage + 1 == line.size())
				line += "\n";
			else
				line += "\r\n";

			std::string newLine = "";

			for (int i = 0; line[i] != '\0'; i++)
			{
				newLine.push_back(line[i]);
				this->body_read++;
				if (this->body_read == this->body_total)
					break;
			}
			this->lines.push_back(newLine);

			if (this->body_read >= this->body_total)
				return (this->parseLine(""));
		}
		return (false);
	}
	this->status_code = 400;
	return (true);
}

void Request::splitRequest(void) {

	std::vector<std::string>::iterator	header_end;

	// Get position of empty line between header and body
	for (header_end = this->lines.begin(); header_end != this->lines.end(); header_end++) {
		if (*header_end == "\r")
			break;
	}

	// Place header values inside headers attribute
	for (std::vector<std::string>::iterator it = this->lines.begin(); it != header_end; it++) {
		if ((*it).find(':') != std::string::npos)
		{
			std::pair<std::string, std::string>	keyval = ft::get_keyval(*it, ": ");
			this->headers.insert(keyval);
		}
	}

	// If a body exists, place this->lines inside body attribute
	if (header_end != lines.end())
	{
		header_end++;
		if (header_end != lines.end())
		{
			for (std::vector<std::string>::iterator it = header_end; it != this->lines.end(); it++)
				this->body.push_back(*it);
		}
	}
}

void	Request::printRequest(void) const {
	// Print values for debugging
	std::cout << std::endl << "Request:" << std::endl;
	std::cout << "  Headers:" << std::endl;
	std::cout << "\t" << this->method.get_str() << " " << this->path << " HTTP/1.1" << std::endl;
	for (std::map<std::string, std::string>::const_iterator it = this->headers.begin(); it != this->headers.end(); it++) {
		std::cout << "\t" << it->first << ": " << it->second << std::endl;
	}
	if (this->body.size()) {
		std::cout << "  Body:" << std::endl;
		for (std::vector<std::string>::const_iterator it = this->body.begin(); it != this->body.end(); it++)
			std::cout << "\t" << *it << std::endl;
	}
	else {
		std::cout << "  No body" << std::endl;
	}
}

bool			Request::get_done() const { return this->done; }
std::string		Request::get_method() const { return this->method.get_str(); }
std::string		Request::get_path() const { return this->path; }
int				Request::get_status_code() const { return this->status_code; }
std::map<std::string, std::string>&	Request::get_headers() { return this->headers; }
std::vector<std::string>&	Request::get_body() { return this->body; }
