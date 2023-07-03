#include <string>
class Request {
    private:
    std::string req_method;
    std::string host;
    std::string port;
    //example: GET www.bbc.co.uk/ HTTP/1.1
    std::string req_line;
    //request input string (whole request messages):
    std::string req_string;
    //time received (TIME):
    std::string recv_time;

    public:
    Request(std::string req_string) : req_string(req_string){
        recv_time = get_curr_time();
        parse_request();
    }
    std::string get_port();
    std::string get_host();
    std::string get_req_method();
    std::string get_recv_time();
    std::string get_req_line();
    std::string get_req_string();

    std::string get_curr_time();
    void parse_request();
    ~Request(){}
};