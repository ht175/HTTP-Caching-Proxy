#include <pthread.h>
#include <string>
#include <map>
#include <fstream>
#include "proxy.hpp"
#include <iostream>
#include <sys/socket.h>
#include <cstring>
#include <unistd.h>
#include <chrono>
#include <ctime>
#include <vector>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
///var/log/erss/proxy.log
std::ofstream *log_file = new std::ofstream("/var/log/erss/proxy.log");
std::map<std::string, Response> mycache;

void proxy::get_started()
{
    int id = 0;
    // close????????????
    // proxy: as a server
    // bulid a new server to wait for client connection request
    int proxy_fd = server(this->port);
    if (proxy_fd == -1)
    {
        std::cerr << "error: build server\n";
        std::exit(EXIT_FAILURE);
    }
    // multi-thread
    // proxy accept client connection request; handle client request and reponse
    while (true)
    {
        std::string ip;
        // proxy accept client connection request
        int client_connection_fd = accept_client_request(proxy_fd, &ip);
        // std::cout << client_connection_fd << "\n";
        // std::cout << "1 st" << ip << "\n";
        if (client_connection_fd == -1)
        {
            std::cerr << "error: connect client\n";
            std::exit(EXIT_FAILURE);
        }
        pthread_mutex_lock(&lock);
        arg_t *args = (arg_t *)malloc(sizeof(arg_t));
        memset(args, 0, sizeof(arg_t));
        args->id = id;
        args->client_connection_fd = client_connection_fd;
        args->ip = ip;

        // std::cout << "2 nd" << ip << "\n";

        id++;
        pthread_mutex_unlock(&lock);

        pthread_t thread;
        // handle client request and reponse
        pthread_create(&thread, NULL, request_handler, args);
    }
    close(proxy_fd);
}

// 为什么传出用void*/???????????????-> return NULL;
void *proxy::request_handler(void *args)
{
    arg_t *arg = (arg_t *)args;
    int id = arg->id;
    int client_connection_fd = arg->client_connection_fd;
    std::string client_ip = arg->ip;
    free(arg);
    //??????????????????????????????
    // buffer size: 8kb->8192
    char req_buffer[BUFFER_SIZE] = {0};
    // 1. Receive a new request from client -> get request input string
    //??????????????????????????MSG_WAITALL???????????????????????
    int req_len = recv(client_connection_fd, req_buffer, sizeof(req_buffer), 0);
    if (req_len <= 0)
    {
        std::cerr << "error: cannot receive request message from client.\n";
        return NULL;
    }
    std::string req_string = std::string(req_buffer, req_len);
    // 2. Parse request input string
    Request *a_request = new Request(req_string);

    if (a_request->get_req_method() != "POST" && a_request->get_req_method() != "GET" && a_request->get_req_method() != "CONNECT")
    {
        pthread_mutex_lock(&lock);
        // ID: Responding "RESPONSE": if you reply with an error (e.g. if you receive a malformed request).
        // log_file << id << ": ERROR " << "Our proxy do not support request methods other than GET, POST, and CONNECT" << std::endl;
        *log_file << id << ": Responding \""
                  << "HTTP/1.1 400 Bad Request"
                  << "\"" << std::endl;
        pthread_mutex_unlock(&lock);
        return NULL;
    }
    pthread_mutex_lock(&lock);
    // ID: "REQUEST" from IPFROM @ TIME: when receiving a new request
    //////////////a_request->line/////////////////////
    *log_file << id << ": \"" << a_request->get_req_line() << "\" from " << client_ip << " @ " << a_request->get_recv_time();
    pthread_mutex_unlock(&lock);

    // 3. As client, proxy connect original server
    int server_fd = client(a_request->get_host().c_str(), a_request->get_port().c_str());
    if (server_fd == -1)
    {
        std::cerr << "error: as client, proxy connect original server\n";
        return NULL;
    }
    // 4. Handle different request methods: GET, POST, and CONNECT
    if (a_request->get_req_method() == "POST")
    {
        // std::cout << "It's a post:" << std::endl;
        post_handler(server_fd, client_connection_fd, req_len, a_request, id);
        // std::cout << "post end." << std::endl;
    }
    else if (a_request->get_req_method() == "GET")
    {
        // std::cout << "It's a get:" << std::endl;
        get_handler(server_fd, client_connection_fd, req_len, a_request, id);
        // std::cout << "get end." << std::endl;
    }
    else
    {
        // CONNECT method:
        // std::cout << "It's a connect:" << std::endl;
        pthread_mutex_lock(&lock);
        *log_file << id << ": "
                  << "Requesting \"" << a_request->get_req_line() << "\" from " << a_request->get_host() << std::endl;
        pthread_mutex_unlock(&lock);
        // connect_handler(server_fd,  client_connection_fd, a_request,  id);
        connect_handler(server_fd, client_connection_fd, id);
        pthread_mutex_lock(&lock);
        *log_file << id << ": Tunnel closed" << std::endl;
        pthread_mutex_unlock(&lock);
        // std::cout << "connect end." << std::endl;
    }
    // delete: a_request object
    close(server_fd);
    close(client_connection_fd);
    return NULL;
}

void proxy::get_handler(int server_fd, int client_connection_fd, int req_len, Request *a_request, int id)
{
    if (mycache.find((a_request->get_req_line())) != mycache.end())
    {
        // if in cache:
        Response a_response = mycache[a_request->get_req_line()];
        if (a_response.no_cache)
        {
            // need revalidate:
            // ID: in cache, requires validation:
            pthread_mutex_lock(&lock);
            *log_file << id << ": in cache, requires validation" << std::endl;
            pthread_mutex_unlock(&lock);
            revalidate2(&a_response, a_request->get_req_string(), id, server_fd, client_connection_fd);
        }
        else
        {
            // is fresh?
            if (is_fresh(&a_response))
            {
                // ID: in cache, valid:
                pthread_mutex_lock(&lock);
                *log_file << id << ": in cache, valid" << std::endl;
                pthread_mutex_unlock(&lock);
                use_cache(client_connection_fd, id, &a_response);
            }
            else
            {
                // need vaildate
                // ID: in cache, but expired at EXPIREDTIME:
                pthread_mutex_lock(&lock);
                *log_file << id << ": in cache, but expired at " << get_expires(&a_response);
                pthread_mutex_unlock(&lock);
                revalidate2(&a_response, a_request->get_req_string(), id, server_fd, client_connection_fd);
            }
        }
    }
    else
    {
        // if not in cache
        // ID: not in cache
        pthread_mutex_lock(&lock);
        *log_file << id << ": not in cache" << std::endl;
        pthread_mutex_unlock(&lock);

        // send request to server
        char req_str[BUFFER_SIZE] = {0};
        strcpy(req_str, (a_request->get_req_string()).c_str());
        // std::cout << "send start." << std::endl;

        // const char * req_str = (a_request->get_req_string()).c_str();
        send(server_fd, req_str, req_len, 0);
        // std::cout << "send ok." << std::endl;

        // ID: Requesting "REQUEST" from SERVER:
        // When proxy needs to contact the origin server about the request:
        pthread_mutex_lock(&lock);
        *log_file << id << ": "
                  << "Requesting \"" << a_request->get_req_line() << "\" from " << a_request->get_host() << std::endl;
        pthread_mutex_unlock(&lock);
        get_server_response2(server_fd, client_connection_fd, id, a_request);
    }
}

bool proxy::is_fresh(Response *a_response)
{
    std::string expire_utc = get_expires(a_response);
    struct tm expire_time;
    memset(&expire_time, 0, sizeof(expire_time));
    strptime(expire_utc.c_str(), "%a %b %d %H:%M:%S %Y", &expire_time);
    time_t expire_timestamp = mktime(&expire_time);
    // get current timestamp

    // get the current time:
    auto now = std::chrono::system_clock::now();
    std::time_t curr_t = std::chrono::system_clock::to_time_t(now);
    // convert to UTC time:
    std::tm *curr_utc = std::gmtime(&curr_t);
    std::cout << "current utc time is" << std::asctime(curr_utc) << std::endl;
    time_t current_timestamp = mktime(curr_utc);
    if (expire_timestamp > current_timestamp)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void proxy::revalidate2(Response *a_response, std::string req_string, int id, int server_fd, int client_connection_fd)
{
    Request a_request0(req_string);
    Request *a_request = &a_request0;
    if (((a_response->Last_Modified).empty()) && ((a_response->Etag).empty()))
    {
        // according to the case of "no in cache":
        // send request to server
        char req_str[(a_request->get_req_string()).length() + 1];
        strcpy(req_str, (a_request->get_req_string()).c_str());
        // const char * req_str = (a_request->get_req_string()).c_str();
        send(server_fd, req_str, sizeof(req_str), MSG_NOSIGNAL);
        // ID: Requesting "REQUEST" from SERVER:
        // When proxy needs to contact the origin server about the request:
        pthread_mutex_lock(&lock);
        *log_file << id << ": "
                  << "Requesting \"" << a_request->get_req_line() << "\" from " << a_request->get_host() << std::endl;
        pthread_mutex_unlock(&lock);
        // determine whether this response can be cached or not, and then send the response to client
        get_server_response2(server_fd, client_connection_fd, id, a_request);
    }
    std::string new_req_string = req_string;
    // Last-modified in response <--> If-Modified-Since in request:
    if (!((a_response->Last_Modified).empty()))
    {
        new_req_string = new_req_string.insert(new_req_string.length() - 2, ("If-Modified-Since: " + a_response->Last_Modified + "\r\n"));
    }
    // Etag in response <--> If-None-Match in request:
    if (!((a_response->Etag).empty()))
    {
        new_req_string = new_req_string.insert(new_req_string.length() - 2, ("If-None-Match: " + a_response->Etag + "\r\n"));
    }
    // char *req_sent = const_cast<char *>(new_req_string.c_str());
    char req_sent[new_req_string.length() + 1];
    strcpy(req_sent, new_req_string.c_str());
    // std:: cout <<"req........." <<req_sent << std::endl;
    int send_len=send(server_fd, req_sent, sizeof(req_sent), MSG_NOSIGNAL);
    std::cout<<"size of req_sent" << sizeof(req_sent)<<std::endl;
    std::cout<< "send #########" << send_len << std::endl;
    Request req_sent_obj = Request(req_sent);
    // ID: Requesting "REQUEST" from SERVER:
    // When proxy needs to contact the origin server about the request:
    char new_response[BUFFER_SIZE] = {0};
    int new_response_len = recv(server_fd, &new_response, sizeof(new_response), 0);
    if (new_response_len <= 0)
    {
        std::cerr << "Error: recv" << std::endl;
    }
    std::string new_response_string(new_response, new_response_len);
    Response new_response_obj = Response(new_response_string);
    std::cout<< "received #########" << new_response_len << std::endl;
    std::cout<<"received #########" << new_response << std::endl;


    if (new_response_string.find("200 OK") != std::string::npos)
    {
        pthread_mutex_lock(&lock);
        *log_file << id << ": "
                  << "Requesting \"" << a_request->get_req_line() << "\" from " << a_request->get_host() << std::endl;
        pthread_mutex_unlock(&lock);
        send(server_fd, req_sent, sizeof(req_sent), MSG_NOSIGNAL);
        get_server_response2(server_fd, client_connection_fd, id, a_request);
    }
    else if (new_response_string.find("304 Not Modified") != std::string::npos)
    {
        // use cache
        use_cache(client_connection_fd, id, a_response);
    }
}

void proxy::revalidate(Response *a_response, std::string req_string, int id, int server_fd, int client_connection_fd)
{
 
    if (((a_response->Last_Modified).empty()) && ((a_response->Etag).empty()))
    {
        // according to the case of "no in cache":

        Request a_request0(req_string);
        Request *a_request = &a_request0;
        // send request to server
        char req_str[(a_request->get_req_string()).length() + 1];
        strcpy(req_str, (a_request->get_req_string()).c_str());
        // const char * req_str = (a_request->get_req_string()).c_str();
        send(server_fd, req_str, sizeof(req_str), MSG_NOSIGNAL);
        // ID: Requesting "REQUEST" from SERVER:
        // When proxy needs to contact the origin server about the request:
        pthread_mutex_lock(&lock);
        *log_file << id << ": "
                  << "Requesting \"" << a_request->get_req_line() << "\" from " << a_request->get_host() << std::endl;
        pthread_mutex_unlock(&lock);
        // rec response from server
        char response_string[BUFFER_SIZE] = {0};
        int response_len = recv(server_fd, &response_string, sizeof(response_string), MSG_WAITALL);
        std::string a_response_str(response_string, response_len);
        Response a_response = Response(a_response_str);
        // ID: Received "RESPONSE" from SERVER
        // when proxy receives the response from the origin server
        pthread_mutex_lock(&lock);
        *log_file << id << ": Received \"" << a_response.Status_line << "\" from " << a_request->get_host() << std::endl;
        pthread_mutex_unlock(&lock);
        // determine whether this response can be cached or not, and then send the response to client
        get_server_response(&a_response, a_request->get_req_string(), server_fd, client_connection_fd, id, response_len);
    }
    std::string new_req_string = req_string;
    // Last-modified in response <--> If-Modified-Since in request:
    if (!((a_response->Last_Modified).empty()))
    {
        // example: GET /index.html HTTP/1.0\r\n \r\n (after head, there are 2 "\r\n")
        new_req_string = new_req_string.insert(new_req_string.length() - 2, ("If-Modified-Since: " + a_response->Last_Modified + "\r\n"));
    }
    // Etag in response <--> If-None-Match in request:
    if (!((a_response->Etag).empty()))
    {
        new_req_string = new_req_string.insert(new_req_string.length() - 2, ("If-None-Match: " + a_response->Etag + "\r\n"));
    }
    char *req_sent = const_cast<char *>(new_req_string.c_str());
 
    send(server_fd, req_sent, sizeof(req_sent), MSG_NOSIGNAL);
    Request req_sent_obj = Request(req_sent);
    //?????????????????????????不晓得啥时候log写???????????????????????????????????????????
    // ID: Requesting "REQUEST" from SERVER:
    // When proxy needs to contact the origin server about the request:
    pthread_mutex_lock(&lock);
    *log_file << id << ": "
              << "Requesting \"" << req_sent_obj.get_req_line() << "\" from " << req_sent_obj.get_host() << std::endl;
    pthread_mutex_unlock(&lock);
    /*
    if(len == -1){
        std::cerr << "Error: send"<<std::endl;
        return;
    }
    */
    char new_response[BUFFER_SIZE] = {0};
    int new_response_len = recv(server_fd, &new_response, sizeof(new_response), MSG_WAITALL);
    std::cout<< "received #########" << new_response_len << std::endl;
    if (new_response_len <= 0)
    {
        std::cerr << "Error: recv" << std::endl;
    }
    // If it is modified, the status code 200 will be returned.
    // If it is not modified, the status code 304 will be returned.
    std::string new_response_string(new_response, new_response_len);
    Response new_response_obj = Response(new_response_string);
    std::cout<< "received #########" << new_response_string << std::endl;

    // ID: Received "RESPONSE" from SERVER
    // when proxy receives the response from the origin server
    pthread_mutex_lock(&lock);
    *log_file << id << ": Received \"" << new_response_obj.Status_line << "\" from " << req_sent_obj.get_host() << std::endl;
    pthread_mutex_unlock(&lock);

    if (new_response_string.find("200 OK") != std::string::npos)
    {
        get_server_response(&new_response_obj, req_string, server_fd, client_connection_fd, id, new_response_len);
    }
    else if (new_response_string.find("304 Not Modified") != std::string::npos)
    {
        // use cache
        use_cache(client_connection_fd, id, a_response);
    }
}

void proxy::get_server_response2(int server_fd, int client_connection_fd, int id, Request *a_request)
{
    char response_string[BUFFER_SIZE] = {0};
    // std::vector<char> response_string(BUFFER_SIZE, 0);
    int response_len = recv(server_fd, &response_string, sizeof(response_string), 0);
    // std::cout << "response_len:" << response_len << std::endl;
    if (response_len <= 0)
    {
        return;
    }

    std::string a_response_str(response_string, response_len);
    Response a_response = Response(a_response_str);
    pthread_mutex_lock(&lock);
    *log_file << id << ": Received \"" << a_response.Status_line << "\" from " << a_request->get_host() << std::endl;
    pthread_mutex_unlock(&lock);
    if (a_response.chunked)
    {
        send(client_connection_fd, response_string, response_len, 0);
        int len;
        pthread_mutex_lock(&lock);
        *log_file << id << ": Responding \"" << a_response.Status_line << "\"" << std::endl;
        pthread_mutex_unlock(&lock);
        std::cout << ": Responding \"" << a_response.Status_line << "\"" << std::endl;
        chunked_handler2(server_fd, client_connection_fd);
        return;
    }
    if (a_response.content_length + a_response.header_length <= BUFFER_SIZE)
    {
        if (a_response.no_store)
        {
            pthread_mutex_lock(&lock);
            // ID: not cacheable because REASON:
            *log_file << id << ": not cacheable because no store" << std::endl;
            pthread_mutex_unlock(&lock);
        }
        else
        {
            Request req_obj = Request(a_request->get_req_string());
            mycache[req_obj.get_req_line()] = a_response;
            if (a_response.no_cache)
            {
                // ID: cached, but requires re-validation
                pthread_mutex_lock(&lock);
                *log_file << id << ": cached, but requires re-validation" << std::endl;
                pthread_mutex_unlock(&lock);
            }
            else
            {
                // ID: cached, expires at EXPIRES
                pthread_mutex_lock(&lock);
                // ID: not cacheable because REASON:
                std::cout << "expires at" << get_expires(&a_response) << std::endl;
                *log_file << id << ": cached, expires at " << get_expires(&a_response);
                pthread_mutex_unlock(&lock);
            }
        }
    }
    else
    {
        while (true)
        {
            char more_response[BUFFER_SIZE] = {0};
            int more_response_len = recv(server_fd, &more_response, sizeof(more_response), MSG_WAITALL);
            std::string more_response_string(more_response, more_response_len);
            a_response.response_string += more_response_string;
            if (a_response.response_string.length() == a_response.content_length + a_response.header_length)
            {
                break;
            }
        }
        if (a_response.no_store)
        {
            pthread_mutex_lock(&lock);
            // ID: not cacheable because REASON:
            *log_file << id << ": not cacheable because no store" << std::endl;
            pthread_mutex_unlock(&lock);
            /// send(client_connection_fd, sp, total, 0);
        }
        else
        {
            Request req_obj = Request(a_request->get_req_string());
            mycache[req_obj.get_req_line()] = a_response;
            if (a_response.no_cache)
            {
                // ID: cached, but requires re-validation
                pthread_mutex_lock(&lock);
                *log_file << id << ": cached, but requires re-validation" << std::endl;
                pthread_mutex_unlock(&lock);
            }
            else
            {
                // ID: cached, expires at EXPIRES
                pthread_mutex_lock(&lock);
                // ID: not cacheable because REASON:
                *log_file << id << ": cached, expires at " << get_expires(&a_response);
                pthread_mutex_unlock(&lock);
            }
        }
    }
    pthread_mutex_lock(&lock);
    // ID: Responding "RESPONSE"
    // When proxy responds to the client
    *log_file << id << ": Responding \"" << a_response.Status_line << "\"" << std::endl;
    pthread_mutex_unlock(&lock);

    char res_str[(a_response.response_string).length() + 1];
    strcpy(res_str, (a_response.response_string).c_str());
    // const char * req_str = (a_request->get_req_string()).c_str();
    // send(server_fd, req_str, sizeof(req_str), MSG_NOSIGNAL);
    send(client_connection_fd, res_str, sizeof(res_str), MSG_NOSIGNAL);
}

// Directly get response from origin server and cache if cacheable
void proxy::get_server_response(Response *a_response, std::string req_string, int server_fd, int client_connection_fd, int id, int recv_len)
{
    if (a_response->chunked == true)
    {
        chunked_handler(a_response, server_fd, client_connection_fd, id, recv_len);
        return;
    }
    if (a_response->content_length + a_response->header_length <= BUFFER_SIZE)
    {
        if (a_response->no_store)
        {
            pthread_mutex_lock(&lock);
            // ID: not cacheable because REASON:
            *log_file << id << ": not cacheable because no store" << std::endl;
            pthread_mutex_unlock(&lock);
        }
        else
        {
            Request req_obj = Request(req_string);
            mycache[req_obj.get_req_line()] = *a_response;
            if (a_response->no_cache)
            {
                // ID: cached, but requires re-validation
                pthread_mutex_lock(&lock);
                *log_file << id << ": cached, but requires re-validation" << std::endl;
                pthread_mutex_unlock(&lock);
            }
            else
            {
                // ID: cached, expires at EXPIRES
                pthread_mutex_lock(&lock);
                // ID: not cacheable because REASON:
                *log_file << id << ": cached, expires at " << get_expires(a_response);
                pthread_mutex_unlock(&lock);
            }
        }
    }
    else
    {
        while (true)
        {
            char more_response[BUFFER_SIZE] = {0};
            int more_response_len = recv(server_fd, &more_response, sizeof(more_response), MSG_WAITALL);
            std::string more_response_string(more_response, more_response_len);
            a_response->response_string += more_response_string;
            if (a_response->response_string.length() == a_response->content_length + a_response->header_length)
            {
                break;
            }
        }
        if (a_response->no_store)
        {
            pthread_mutex_lock(&lock);
            // ID: not cacheable because REASON:
            *log_file << id << ": not cacheable because no store" << std::endl;
            pthread_mutex_unlock(&lock);
            /// send(client_connection_fd, sp, total, 0);
        }
        else
        {
            Request req_obj = Request(req_string);
            mycache[req_obj.get_req_line()] = *a_response;
            if (a_response->no_cache)
            {
                // ID: cached, but requires re-validation
                pthread_mutex_lock(&lock);
                *log_file << id << ": cached, but requires re-validation" << std::endl;
                pthread_mutex_unlock(&lock);
            }
            else
            {
                // ID: cached, expires at EXPIRES
                pthread_mutex_lock(&lock);
                // ID: not cacheable because REASON:
                *log_file << id << ": cached, expires at " << get_expires(a_response);
                pthread_mutex_unlock(&lock);
            }
        }
    }
    pthread_mutex_lock(&lock);
    // ID: Responding "RESPONSE"
    // When proxy responds to the client
    *log_file << id << ": Responding \"" << a_response->Status_line << "\"" << std::endl;
    pthread_mutex_unlock(&lock);

    char res_str[(a_response->response_string).length() + 1];
    strcpy(res_str, (a_response->response_string).c_str());
    // const char * req_str = (a_request->get_req_string()).c_str();
    // send(server_fd, req_str, sizeof(req_str), MSG_NOSIGNAL);
    send(client_connection_fd, res_str, sizeof(res_str), MSG_NOSIGNAL);
}

std::string proxy::get_expires(Response *a_response)
{
    if (!((a_response->Expires).empty()))
    {
        return a_response->Expires;
    }
    else
    {
        // use max age and date in response object to get real expires

        if (a_response->max_age != -1)
        {
            struct tm utc_time_t;
            memset(&utc_time_t, 0, sizeof(utc_time_t));
            // convert UTC time string to struct tm
            strptime(a_response->date.c_str(), "%a %b %d %H:%M:%S %Y", &utc_time_t);
            utc_time_t.tm_sec += a_response->max_age;
            std::mktime(&utc_time_t);
            return asctime(&utc_time_t);
        }
        else
        {
            // no max age  and no expire
            struct tm utc_time_t;
            memset(&utc_time_t, 0, sizeof(utc_time_t));
            // convert UTC time string to struct tm
            strptime(a_response->date.c_str(), "%a %b %d %H:%M:%S %Y", &utc_time_t);
            utc_time_t.tm_sec += 86400;
            std::mktime(&utc_time_t);
            return asctime(&utc_time_t);
        }
    }
}

void proxy::chunked_handler2(int server_fd, int client_connection_fd)
{

    while (true)
    {
        std::vector<char> data_buff(65536, 0);
        int data_rec = 0;
        data_rec = recv(server_fd, &data_buff.data()[0], 65536, 0);
        data_buff.resize(data_rec);
        // chunk end
        if (data_buff.size() <= 0)
        {
            break;
        }
        int size = send(client_connection_fd, &data_buff.data()[0], data_rec, 0);
    }
}

void proxy::chunked_handler(Response *a_response, int server_fd, int client_connection_fd, int id, int recv_len)
{
    pthread_mutex_lock(&lock);
    // ID: Responding "RESPONSE"
    // When proxy responds to the client
    *log_file << id << ": Responding \"" << a_response->Status_line << "\"" << std::endl;
    pthread_mutex_unlock(&lock);
    std::cout << "life suckssssss" << a_response->response_string << std::endl;

    char res_str[(a_response->response_string).length() + 1] = {0};
    strcpy(res_str, (a_response->response_string).c_str());
    // const char * req_str = (a_request->get_req_string()).c_str();

    std::cout << "@@@@@@" << send(client_connection_fd, res_str, recv_len, 0) << std::endl;
    int len;

    while (true)
    {
        std::vector<char> data_buff(65536, 0);
        int data_rec = 0;
        data_rec = recv(server_fd, &data_buff.data()[0], 65536, 0);
        data_buff.resize(data_rec);
        // chunk end
        if (data_buff.size() <= 0)
        {
            break;
        }
        std::cout << "！！！！！size" << data_rec << std::endl;
        // whenever receive more data, send back to client;
        int size = send(client_connection_fd, &data_buff.data()[0], data_rec, 0);
        std::cout << "！！！！！size" << size << std::endl;
    }
    /*
    while(true){
        char more_response[BUFFER_SIZE] = {0};
        int more_response_len = recv(server_fd, more_response, sizeof(more_response), 0);
        //chunk end
        if(more_response_len<=0){
            std::cout << "11111 aaaaaaaaa"<< std::endl;
            break;
        }
        //std::cout << "am here can ypu listen meeeë" << std::endl;
        std::cout << "！！！！！size" << more_response_len << std::endl;
        //whenever receive more data, send back to client;
        int size = send(client_connection_fd, more_response, (size_t)more_response_len, 0);
        std::cout << "******size " << size << std::endl;
    }
    */
}

void proxy::use_cache(int client_connection_fd, int id, Response *a_response)
{
    char *response = const_cast<char *>((a_response->response_string).c_str());
    send(client_connection_fd, response, strlen(response), MSG_NOSIGNAL);
    pthread_mutex_lock(&lock);
    // ID: Responding "RESPONSE"
    // When proxy responds to the client
    *log_file << id << ": Responding \"" << a_response->Status_line << "\"" << std::endl;
    pthread_mutex_unlock(&lock);
}

void proxy::post_handler(int server_fd, int client_connection_fd, int req_len, Request *a_request, int id)
{
    int post_remaining_len = get_remaining_len(a_request->get_req_string(), req_len);
    std::cout << "post here" << std::endl;
    if (post_remaining_len != -1)
    {
        std::string whole_req0 = get_whole_content(client_connection_fd, a_request->get_req_string(), post_remaining_len);

        char whole_req1[whole_req0.length() + 1];
        strcpy(whole_req1, whole_req0.c_str());
        int send_len = send(server_fd, whole_req1, sizeof(whole_req1), 0);
        std::cout << "post send lennnnnnnn" << send_len << "\n";
        pthread_mutex_lock(&lock);
        *log_file << id << ": "
                  << "Requesting \"" << a_request->get_req_line() << "\" from " << a_request->get_host() << std::endl;
        pthread_mutex_unlock(&lock);
        char response_buffer[BUFFER_SIZE] = {0};
        int response_len = recv(server_fd, response_buffer, sizeof(response_buffer), 0);

        std::cout << "post recived lennnnnnnnn" << response_len
                  << "\n";
        std::string response = std::string(response_buffer, response_len);
        Response a_response = Response(response);
        pthread_mutex_lock(&lock);
        *log_file << id << ": Received \"" << a_response.Status_line << "\" from " << a_request->get_host() << std::endl;
        pthread_mutex_unlock(&lock);
        int senagain_len = send(client_connection_fd, response_buffer, response_len, MSG_NOSIGNAL);
        std::cout << "post send again lennnnnnnnn" << senagain_len
                  << "\n";
        pthread_mutex_lock(&lock);
        *log_file << id << ": Responding \"" << a_response.Status_line << std::endl;
        pthread_mutex_unlock(&lock);
    }
}

// request/reponse buffer size is limited, so the body of the POST request/GET response may not be fully stored in the buffer.
// This function will get the remaining length of the body of the POST request/GET response
int proxy::get_remaining_len(std::string input, int len)
{

    std::cout << "get_remaining_len()"
              << "\n";

    size_t pos = input.find("Content-Length: ");
    if (pos != std::string::npos)
    {
        // get content length:
        std::string content_length_string = input.substr(pos + 16, input.find("\r\n", pos) - pos - 16);
        int content_length = std::stoi(content_length_string);
        // get the length of body stored in buffer
        size_t head_end = input.find("\r\n\r\n");
        int body_stored = len - static_cast<int>(head_end) - 4;
        // get the length of remaining body
        std::cout << input << std::endl;
        std::cout << content_length_string << std::endl;
        return content_length - body_stored;
    }
    else
    {
        return -1;
    }
}

// This function is used to get the whole message of the POST request/GET response:
std::string proxy::get_whole_content(int fd, std::string input, int remaining_len)
{
    int total_len = 0;
    while (total_len < remaining_len)
    {
        char new_buffer[BUFFER_SIZE] = {0};
        int len = recv(fd, new_buffer, sizeof(new_buffer), 0);
        if (len <= 0)
        {
            break;
        }
        total_len += len;
        std::string part_string(new_buffer, len);
        // Concatenate the already stored message and the remaining body parts
        input += part_string;
    }
    return input;
}

void proxy::connect_handler(int server_fd, int client_connection_fd, int id)
{
    // std::cout << "connect_handler: " << server_fd << " " << client_connection_fd << " " << id << std::endl;
    send(client_connection_fd, "HTTP/1.1 200 OK\r\n\r\n", 19, 0);
    fd_set rdfds;
    int nfds = server_fd > client_connection_fd ? server_fd : client_connection_fd;
    pthread_mutex_lock(&lock);
    *log_file << id << ": Responding \"HTTP/1.1 200 OK\"" << std::endl;
    pthread_mutex_unlock(&lock);
    while (true)
    {
        int len;
        FD_ZERO(&rdfds);
        FD_SET(server_fd, &rdfds);
        FD_SET(client_connection_fd, &rdfds);
        select(nfds + 1, &rdfds, NULL, NULL, NULL);
        int fds[2] = {server_fd, client_connection_fd};
        for (int i = 0; i < 2; i++)
        {
            char buffer[BUFFER_SIZE] = {0};
            if (FD_ISSET(fds[i], &rdfds))
            {
                len = recv(fds[i], buffer, sizeof(buffer), 0);
                if (len <= 0)
                {
                    // std::cout << "ZZQ:recv error1." << std::endl;
                    return;
                }
                int another_fd = fds[i] == client_connection_fd ? server_fd : client_connection_fd;
                if (send(another_fd, buffer, len, 0) <= 0)
                {
                    return;
                }
            }
        }
    }
}
