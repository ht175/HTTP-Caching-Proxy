#include "request.hpp"
#include <chrono>
#include <ctime>
#include <iostream>
//create object: constructor: get current time

std::string Request::get_port(){
    return this->port;
}

std::string Request::get_host(){
    return this->host;
}

std::string Request::get_req_method(){
    return this->req_method;
}

std::string Request::get_recv_time(){
    return this->recv_time;
}

std::string Request::get_req_line(){
    return this->req_line;
}

std::string Request::get_req_string(){
    return this->req_string;
}

std::string Request::get_curr_time(){
    //get the current time:
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    //convert to UTC time:
    std::tm * utc = std::gmtime(&t);
    //format time using asctime()
    const char * time_string = std::asctime(utc);
    return std::string(time_string);
}

//this funcrion is used to parse request message string (req_string)
void Request::parse_request(){
    //request method:
    req_method = req_string.substr(0, req_string.find(" "));
    //req_line:
    req_line = req_string.substr(0, req_string.find("\r\n"));
    //host & port:
    size_t host_pos = req_string.find("Host: ") + 6;
    if(host_pos != std::string::npos){
        std::string after_host = req_string.substr(host_pos);
        std::string host_line = after_host.substr(0, after_host.find("\r\n"));
        size_t port_pos = host_line.find(':');
        if(port_pos != std::string::npos){
            host = host_line.substr(0, port_pos);
            port = host_line.substr(port_pos + 1);
        }else{
            host = host_line;
            //default HTTP port
            port = "80";
        }
    }else{
        //????????????????????????????? try, exception?
        std::cerr << "No host\n";
        host = "";
        port = "";
        return;
    }
}
