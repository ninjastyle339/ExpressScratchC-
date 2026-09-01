#pragma once

#include <string>

struct HttpResponse {
    int status;
    std::string body;
};
std::string serializeResponse(const HttpResponse& response);