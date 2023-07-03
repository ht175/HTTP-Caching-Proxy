#include "function.hpp"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <vector>
#include <arpa/inet.h>
//Functions:
/*
The implementstions of server(), accept_client_request(), and client() 
refer to the resource "tcp_example.zip" provided by ECE 650.
*/
int server(const char * port_num){
    int status;
    int socket_fd;
    struct addrinfo host_info;
    struct addrinfo *host_info_list;
    const char *hostname = NULL;
    const char *port = port_num;

    memset(&host_info, 0, sizeof(host_info));

    host_info.ai_family = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;
    host_info.ai_flags = AI_PASSIVE;

    status = getaddrinfo(hostname, port, &host_info, &host_info_list);
    if (status != 0) {
        std::cerr << "Error: cannot get address info for host" << "\n";
        std::cerr << "  (" << hostname << "," << port << ")" << "\n";
        return -1;
    }

    if (strcmp(port, "") == 0) {
        //???????????????????????
        //player: no port->assign port
        struct sockaddr_in *addr = (struct sockaddr_in *)(host_info_list->ai_addr);
        addr->sin_port = 0;
    }

    socket_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
    if (socket_fd == -1) {
        std::cerr << "Error: cannot create socket" << "\n";
        std::cerr << "  (" << hostname << "," << port << ")" << "\n";
        return -1;
    } //if

    int yes = 1;
    status = setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
    status = bind(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
        std::cerr << "Error: cannot bind socket" << "\n";
        std::cerr << "  (" << hostname << "," << port << ")" << "\n";
        return -1;
    } //if

    status = listen(socket_fd, 100);
    if (status == -1) {
        std::cerr << "Error: cannot listen on socket" << "\n"; 
        std::cerr << "  (" << hostname << "," << port << ")" << "\n";
        return -1;
    } //if

    //free objects allocated on the heap: freeaddrinfo() after getaddrinfo()
    freeaddrinfo(host_info_list);
    return socket_fd;
}

//server calls accept() to receive the client's connection request:
int accept_client_request(int socket_fd, std::string * ip){
    struct sockaddr_storage socket_addr;
    socklen_t socket_addr_len = sizeof(socket_addr);
    int client_connection_fd;
    client_connection_fd = accept(socket_fd, (struct sockaddr *)&socket_addr, &socket_addr_len);
    if (client_connection_fd == -1) {
        std::cerr << "Error: cannot accept connection on socket" << "\n";
        return -1;
    } //if
    //?????????????????????
    struct sockaddr_in * sockaddr = (struct sockaddr_in *)&socket_addr;
    //std::cout << "11111111111\n";
    *ip = inet_ntoa(sockaddr->sin_addr);
    return client_connection_fd;
}

int client(const char * hostname, const char * port_num){
    int status;
    int socket_fd;
    struct addrinfo host_info;
    struct addrinfo *host_info_list;
    const char *port = port_num;

    memset(&host_info, 0, sizeof(host_info));

    host_info.ai_family = AF_UNSPEC;
    host_info.ai_socktype = SOCK_STREAM;

    status = getaddrinfo(hostname, port, &host_info, &host_info_list);
    if (status != 0) {
        std::cerr << "Error: cannot get address info for host" << "\n";
        std::cerr << "  (" << hostname << "," << port << ")" << "\n";
        return -1;
    } //if

    socket_fd = socket(host_info_list->ai_family, host_info_list->ai_socktype, host_info_list->ai_protocol);
    if (socket_fd == -1) {
        std::cerr << "Error: cannot create socket" << "\n";
        std::cerr << "  (" << hostname << "," << port << ")" << "\n";
        return -1;
    } //if
    
    status = connect(socket_fd, host_info_list->ai_addr, host_info_list->ai_addrlen);
    if (status == -1) {
        std::cerr << "Error: cannot connect to socket" << "\n";
        std::cerr << "  (" << hostname << "," << port << ")" << "\n";
        return -1;
    } //if

    //free objects allocated on the heap: freeaddrinfo() after getaddrinfo()
    freeaddrinfo(host_info_list);
    return socket_fd;
}

