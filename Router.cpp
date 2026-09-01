
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Router.hpp"

using Handler = std::function<HttpResponse(const HttpRequest&)>;

std::vector<std::string> parseRoute(const std::string& key){
    std::vector<std::string> path;
    std::string cur;
    for(int i{}; i < key.size(); ++i){
        if(key[i] == '/'){
            path.push_back(cur);
            cur = "";
            continue;
        }
        cur += key[i];
    }
    path.push_back(cur);
    return path;
}
void Router::get(const std::string& path, Handler handler) {
    std::string key = "GET " + path;
    Route route;
    route.path = parseRoute(key);
    route.handler = handler;
    routes[key] = std::move(route);
}
void Router::post(const std::string& path, Handler handler){
    std::string key = "POST " + path;
    Route route;
    route.path = parseRoute(key);
    route.handler = handler;
    routes[key] = std::move(route);
}

HttpResponse Router::route(HttpRequest& request) {
    //standard way no dynamic routing
    // std::string key = request.method + " " + request.path;
    // auto it = routes.find(key);
    // if(it == routes.end()){
    //     return {404, ""};
    // }
    // return it->second(request);
    //now standard way with dynamic routing
    std::string key = request.method + " " + request.path;
    std::vector<std::string> path = parseRoute(key);
    
    for(const auto& [route, value] : routes){
        std::vector<std::string> routePath = value.path;
        std::unordered_map<std::string, std::string> params;
        if(routePath.size() != path.size()) continue;
        bool matched = true;
        for(std::size_t i{}; i < routePath.size(); ++i){
            if(routePath[i][0] == ':') {
                std::string paramKey = routePath[i].substr(1);
                params[paramKey] = path[i];
            }
            else if(path[i] != routePath[i]){
                matched = false;
                break;
            }
        }
        if(matched){
            //request.params = params;
            //not much of a difference since we don't have a lot of data in
            //params anyway
            request.params = std::move(params);
            return value.handler(request);
        }
    }
    return {404, "not found"};
}
