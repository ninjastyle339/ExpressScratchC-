#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

using Handler = std::function<HttpResponse(const HttpRequest&)>;
struct Route {
    std::vector<std::string> path;
    Handler handler;   
};
class Router {
    std::unordered_map<std::string, Route> routes;
public:
    
    void get(const std::string& path, Handler handler);
    void post(const std::string& path, Handler handler);

    HttpResponse route(HttpRequest& request);

};