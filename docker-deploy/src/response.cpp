#include "response.hpp"
#include <ctime>
#include <cstring>
#include <iostream>

void Response::parse(std::string response_line)
{
    size_t pos = response_line.find("Content-Length: ");
    if (pos != std::string::npos){
        //get content length:
        std::string content_length_string = response_line.substr(pos + 16, response_line.find("\r\n", pos) - pos - 16);
        content_length = std::stoi(content_length_string);
    }
    // get status_line: the first line of the response line
    std::size_t first_CRLF = response_line.find_first_of("\r\n");
    Status_line = response_line.substr(0, first_CRLF);

    // begin to parse header

    if (size_t find_no_cache = response_line.find("no-cache") != std::string::npos)
    {
        // need vaildate
        no_cache = true;
    }
    if (size_t find_no_store = response_line.find("no-store") != std::string::npos)
    {
        // cannot cache
        no_store = true;
    }
    std::string last_modified_temp("Last-Modified: ");
    size_t find_Last_Modified = response_line.find(last_modified_temp);
    if (find_Last_Modified != std::string::npos)
    {
        size_t len = last_modified_temp.length();
        size_t CRLF_after_modified = response_line.find("\r\n", find_Last_Modified + len);
        Last_Modified = response_line.substr(find_Last_Modified + len, CRLF_after_modified - find_Last_Modified - len);
    }
    std::string etag_temp("ETag: ");
    size_t find_etag = response_line.find(etag_temp);
    if (find_etag != std::string::npos)
    {
        size_t len = etag_temp.length();
        size_t CRLF_after_etag = response_line.find("\r\n", find_etag + len);
        Etag = response_line.substr(find_etag + len, CRLF_after_etag - find_etag - len);
    }
    std::string expire_temp("Expires: ");
    size_t find_expire = response_line.find(expire_temp);
    std::string http_expires;
    if (find_expire != std::string::npos)
    {
        size_t len = expire_temp.length();
        size_t CRLF_after_expire = response_line.find("\r\n", find_expire + len);
        std::string http_expires = response_line.substr(find_expire + len, CRLF_after_expire - find_expire - len);
        struct tm date_t_expire;
        //?????????????????????????????????????????????????????
        memset(&date_t_expire, 0, sizeof(date_t_expire));
        //convert HTTP date string to struct tm
        strptime(http_expires.c_str(), "%a, %e %h %Y %X", &date_t_expire);
        // time_t timestamp_expires = mktime(&date_t_expire);
        // struct tm * utc_expire = gmtime(&timestamp_expires);
        //
        
        //convert HTTP date string to struct tm
        Expires = asctime(&date_t_expire);
    }
    


    std::string max_age_temp("max-age=");
    size_t find_max_age = response_line.find(max_age_temp);
    if (find_max_age != std::string::npos)
    {
        std::string temp = response_line.substr(find_max_age + 8);
        max_age = atoi(temp.c_str());
    }

    std::string date_temp("Date: ");
    std::string http_date;
    size_t find_date = response_line.find(date_temp);
    if(find_date != std::string::npos){
        size_t len = date_temp.length();
        size_t CRLF_after_date = response_line.find("\r\n",find_date+len);
        std::string http_date = response_line.substr(find_date+len,CRLF_after_date-find_date-len);
        struct tm date_t;
        std::cout << "real date from website" <<  http_date << std::endl;
        //?????????????????????????????????????????????????????
        memset(&date_t, 0, sizeof(date_t));
        //convert HTTP date string to struct tm
        strptime(http_date.c_str(), "%a, %e %h %Y %X", &date_t);
        // time_t timestamp = mktime(&date_t);
        // struct tm * utc_date = gmtime(&timestamp);
        date = asctime(&date_t);
        std::cout << "date: qqqqqqqq" << date << "\n";
    }

    size_t find_Transfer_Encoding = response_line.find("Transfer-Encoding: ");
    if (find_Transfer_Encoding != std::string::npos)
    {
        if (response_line.find("chunked", find_Transfer_Encoding) != std::string::npos)
        {
            chunked = true;
        }
    }

    size_t head_end = response_line.find("\r\n\r\n");
    if(head_end != std::string::npos){
        std::string response_header = response_line.substr(0, head_end + 4);
        header_length = response_header.size();
    }
}

