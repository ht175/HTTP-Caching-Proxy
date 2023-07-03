#include <cstdio>
#include <cstdlib>
#include <string>

#include "request.hpp"
#include "response.hpp"
#include "function.hpp"
#include <vector>

//???????????????????
//buffer size: 8kb->8192
#define BUFFER_SIZE 65536

class proxy {
    private:
    const char * port;

    public:
    //constructor:
    proxy(const char * port) : port(port) {}
    void get_started();

    static void * request_handler(void * args);
    static void post_handler(int server_fd, int client_connection_fd, int req_len, Request * a_request, int id);
    static void get_handler(int server_fd, int client_connection_fd, int req_len, Request * a_request, int id);
    //static void connect_handler(int server_fd, int client_connection_fd, Request * a_request, int id);
    static void connect_handler(int server_fd, int client_connection_fd, int id);
    static std::string get_whole_content(int fd, std::string input, int remaining_len);
    static int get_remaining_len(std::string input, int len);
    static void use_cache(int client_connection_fd, int id, Response * a_response);
    static void get_server_response(Response * a_response, std::string req_string, int server_fd, int client_connection_fd, int id, int recv_len);
    static std::string get_expires(Response * a_response);
    static void revalidate(Response * a_response, std::string req_string, int id, int server_fd, int client_connection_fd);
    static bool is_fresh(Response * a_response);
    static void chunked_handler(Response * a_response, int server_fd, int client_connection_fd, int id, int recv_len);
    static void get_server_response2(int server_fd, int client_connection_fd, int id, Request *a_request);
    static void chunked_handler2( int server_fd, int client_connection_fd);
    static void revalidate2(Response *a_response, std::string req_string, int id, int server_fd, int client_connection_fd);



};

struct arg_tag{
    int id;
    int client_connection_fd;
    std::string ip;
};
typedef struct arg_tag arg_t;