/* ************************************************************************** */
/*                                                                            */
/*                                                        ::::::::            */
/*   WebServer.hpp                                      :+:    :+:            */
/*                                                     +:+                    */
/*   By: tbruinem <tbruinem@student.codam.nl>         +#+                     */
/*                                                   +#+                      */
/*   Created: 2021/02/03 14:16:49 by tbruinem      #+#    #+#                 */
/*   Updated: 2021/03/25 17:22:09 by tbruinem      ########   odam.nl         */
/*                                                                            */
/* ************************************************************************** */

#ifndef WEB_SERVER_HPP
# define WEB_SERVER_HPP

# include "Configuration.hpp"
# include "Client.hpp"
# include "Server.hpp"
# include "Context.hpp"
# include "Request.hpp"
# include "Response.hpp"
# include "Properties.hpp"

# include <queue>
# include <vector>
# include <map>
# include <sys/select.h>
# include <sys/time.h>
# include <sys/types.h>
# include <unistd.h>

# define DEFAULT_CONFIG "./config/default.conf"

// enum ResponseCode
// {
// 	200 = OK,
// 	400 = ERROR,

// };

//Heart of the program, contains the main loop that handles all the requests/responses

class WebServer : public Context
{
	private:
		friend class Configuration;
		friend class Context;

		WebServer();
		std::map<int, Server*>							servers;
		std::map<int, Client*>							clients;

		std::map<int, std::queue<Request> >				requests;
		std::map<int, std::queue<Response> >			responses;

		fd_set											read_sockets;
		fd_set											write_sockets;
		std::map<Server*, std::vector<std::string> >	server_names;

		void	deleteClient(int fd);
		void	addNewClients(fd_set& read_set);
		void	readRequests();
		void	writeResponses();
		static void	closeSignal(int status);
	public:
		WebServer(char *config_path);
		WebServer(const WebServer& other);
		void	run();
		WebServer& operator = (const WebServer& other);
		~WebServer();
};

#endif