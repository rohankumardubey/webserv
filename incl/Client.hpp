/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   Client.hpp                                         :+:    :+:            */
/*                                                     +:+                    */
/*   By: tbruinem <tbruinem@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2021/02/03 15:24:51 by tbruinem      #+#    #+#                 */
/*   Updated: 2021/03/28 11:08:29 by tbruinem      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENT_HPP
# define CLIENT_HPP

#include "Server.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include <queue>

class Client
{
	private:
		Server*						server;
		struct sockaddr_in			address;
		socklen_t					addr_len;
		Request						*req;
		std::queue<Request>			requests;
		std::queue<Response>		responses;
		ssize_t						fd;
		std::string					remainder;
		Client();
		Client& operator = (const Client& other);
		Client(const Client& other);
	public:
		int		receive(const std::map<Server*, std::vector<std::string> >& server_names);
		int		send();
		int		getFd();
		Client(Server* server);
		~Client();
};

#endif
