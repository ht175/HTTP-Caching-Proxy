#include <string>
//Function prototypes:
/*
The implementstions of server(), accept_client_request(), and client() 
refer to the resource "tcp_example.zip" provided by ECE 650.
*/
int server(const char * port_num);

int accept_client_request(int socket_fd, std::string * ip);

int client(const char * hostname, const char * port_num);
