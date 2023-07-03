
#include <string>
#include <ostream>
#include <iostream>
class Response{
    public:
    std::string response_string;
    std::string Status_line;
    //cache-related attributes
    bool no_cache;
    bool no_store;
    std::string Last_Modified;
    std::string Etag;
    std::string Expires;
    int max_age;
    //
    std::string date;
    bool chunked;
    //std::string Body_message;
    int content_length;
    int header_length;

    Response(){};

    Response(std::string response_line): response_string(response_line), content_length(-1), header_length(-1), no_cache(false), no_store(false), chunked(false), max_age(-1){
        parse(response_line);
        std::cout << "max-age" << max_age <<std::endl;
    }
    Response & operator=(const Response & rhs){
        if(this != &rhs){
            response_string = rhs.response_string;
            Status_line = rhs.Status_line;
            no_cache = rhs.no_cache;
            no_store = rhs.no_store;
            Last_Modified = rhs.Last_Modified;
            Etag = rhs.Etag;
            Expires = rhs.Expires;
            max_age = rhs.max_age;
            date = rhs.date;
            chunked = rhs.chunked;
            content_length = rhs.content_length;
            header_length = rhs.header_length;
        }
        return *this;
    }
    void parse(std::string response_line);
};
