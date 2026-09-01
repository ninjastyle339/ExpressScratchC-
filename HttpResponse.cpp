#include <string>
#include "HttpResponse.hpp"
std::string serializeResponse(const HttpResponse& response){
    std::string statusText;
    if(response.status == 200){
        statusText = "OK";
    }
    else if(response.status == 404){
        statusText = "Not Found";
    }
    std::string result = "HTTP/1.1 " + std::to_string(response.status) + " " + statusText + "\r\n" 
                         "Content-Type: text/plain\r\n"
                         "Content-Length: " + std::to_string(response.body.size()) + "\r\n"
                         "Connection: keep-alive\r\n"
                         "\r\n"
                         "" + response.body;
    return result;

}