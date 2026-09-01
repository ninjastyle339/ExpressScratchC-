#pragma once
#include <string>
#include <unordered_map>
struct HttpRequest{
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::unordered_map<std::string, std::string> params;
    std::string body;
};
