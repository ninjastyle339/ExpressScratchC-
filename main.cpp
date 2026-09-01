#include <iostream>
#include <array>
#include <string>
#include <unordered_map>
#include <sstream>

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "Router.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <cerrno>
#include <cstring>

constexpr int BUFFER_SIZE = 1024;
constexpr int MAX_EVENTS = 100;

// struct HttpRequest {
//     std::string method;
//     std::string path;
//     std::string version;

//     std::unordered_map<std::string, std::string> headers;
//     std::string body;
// };
// struct HttpResponse {
//     int status;
//     std::string body;
// };


HttpRequest parseRequest(const std::string& data){
    HttpRequest request;

    std::size_t position = data.find("\r\n");
    std::string firstLine = data.substr(0, position);
    std::istringstream stream(firstLine);
    stream >> request.method >> request.path >> request.version;
    while(true){
        std::size_t lineEnd = data.find("\r\n", position + 1);
        std::string line = data.substr(position + 2, lineEnd - (position + 2));
        if(line.empty()) 
            break;
        
        std::size_t colon = line.find(":");
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        value.erase(0, value.find_first_not_of(' '));

        request.headers[key] = value;

        position = lineEnd;
    }

    return request;
}


int set_socket_nonblocking(int clientSocket){
    int flags = fcntl(clientSocket, F_GETFL, 0);
    if(flags < 0) return -1;

    return fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);
}

int sendStream(int clientSocket, const std::array<char, BUFFER_SIZE>& buffer, ssize_t bytesReceived){
    ssize_t totalSent = 0;
    while(totalSent < bytesReceived){
        ssize_t bytesSent = send(clientSocket, buffer.data() + totalSent, bytesReceived - totalSent, 0);
        if(bytesSent < 0){
            std::cerr << "send failed\n";
            return -1;
        }
        if(bytesSent == 0){
            std::cerr << "sendStream sent 0 bytes in one instance\n";
            return -1;
        }
        totalSent += bytesSent;
    }
    return 0;
}

int main(){
    
    int serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(serverSocket < 0) {
        std::cerr << "Socket creation failed\n";
        return 1;
    }
    std::cout << "Socket created\n";

    int option = 1;

    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR,
                &option, sizeof(option)) < 0)
    {
        std::cerr << "setsockopt failed\n";
        close(serverSocket);
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(bind(serverSocket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0){
        std::cerr << "Bind failed\n";
        close(serverSocket);
        return 1;
    }
    std::cout << "Socket bound\n";

    if(listen(serverSocket, 100) < 0){
        std::cerr << "Listen failed\n";
        close(serverSocket);
        return 1;
    }
    if(set_socket_nonblocking(serverSocket) < 0){
        std::cerr << "Server socket set non-blocking error\n";
        close(serverSocket);
        return 1;
    }
    

    //create epoll instance
    int epollFd = epoll_create1(0);
    if(epollFd < 0){
        std::cerr << "epoll_create1 failed\n";
        close(serverSocket);
        return 1;
    }

    //adding server socket to epoll
    epoll_event serverEvent{};
    serverEvent.events = EPOLLIN;
    serverEvent.data.fd = serverSocket;

    if(epoll_ctl(epollFd, EPOLL_CTL_ADD, serverSocket, &serverEvent) < 0){
        std::cerr << "epoll_ctl failed\n";
        close(epollFd);
        close(serverSocket);
        return 1;
    }
    epoll_event events[MAX_EVENTS];
    std::unordered_map<int, std::string> requestBuffers;

    std::cout << "Server successfully listening on port 8080\n";

    //testing router
    Router router;
    router.get("/hello", [](const HttpRequest& request){
        return HttpResponse{200, "Hello from c++!"};
    });
    router.get("/users", [](const HttpRequest& request){
        return HttpResponse{200, "Here are the users"};
    });
    router.post("/users", [](const HttpRequest& request){
        return HttpResponse{200, "created user: " + request.body};
    });
    // router.get("/", [](const HttpRequest& request){
    //     return HttpResponse{200, "Hello firefox!"};
    // });

    while(true){
        int eventCount = epoll_wait(epollFd, events, MAX_EVENTS, -1);
        if(eventCount < 0){
            if(errno == EINTR)
                continue;
            std::cerr << "epoll_wait failed\n";
            break;
        }
        //handle every socket that became ready
        for(int i = 0; i < eventCount; ++i){
            int socket = events[i].data.fd;
            if(socket == serverSocket){
                int clientSocket = accept(serverSocket, nullptr, nullptr);
                if(clientSocket < 0){
                    if(errno == EAGAIN || errno == EWOULDBLOCK)
                        continue;
                    std::cerr << "accept failed\n";
                    continue;
                }
                if(set_socket_nonblocking(clientSocket) < 0){
                    std::cerr << "Couldn't make client non-blocking\n";
                    close(clientSocket);
                    continue;
                }
                epoll_event clientEvent{};
                clientEvent.events = EPOLLIN;
                clientEvent.data.fd = clientSocket;
                if(epoll_ctl(epollFd, EPOLL_CTL_ADD, clientSocket, &clientEvent) < 0){
                    std::cerr << "Couldn't add client to epoll\n";
                    close(clientSocket);
                    continue;
                }
                requestBuffers[clientSocket] = "";
                //std::cout << "Client connected: " << clientSocket << '\n';
            } 
            //existing client has data
            else {
                std::array<char, BUFFER_SIZE> buffer{};
                while(true){
                    ssize_t bytesReceived = recv(socket, buffer.data(), buffer.size(), 0);
                    if(bytesReceived < 0){
                        if(errno == EAGAIN || errno == EWOULDBLOCK)
                            break;
                        //std::cerr << "recv failed: " << strerror(errno) << '\n';
                        epoll_ctl(epollFd, EPOLL_CTL_DEL, socket, nullptr);
                        requestBuffers.erase(socket);
                        close(socket);
                        break;
                    }
                    if(bytesReceived == 0){
                        //std::cout << "Client disconnected: " << socket << '\n';
                        epoll_ctl(epollFd, EPOLL_CTL_DEL, socket, nullptr);
                        requestBuffers.erase(socket);
                        close(socket);
                        break;
                    }
                    // std::cout << "Received " << bytesReceived << " bytes from "
                    //           << socket << ": ";
                    // //std::cout.write(buffer.data(), bytesReceived);
                    // std::cout << '\n';
                    requestBuffers[socket].append(buffer.data(), bytesReceived);
                    std::string& requestData = requestBuffers[socket];
                    std::size_t headerEnd = requestData.find("\r\n\r\n");
                    while(headerEnd != std::string::npos){
                        //parse http request
                        HttpRequest request = parseRequest(requestData);
                        std::size_t contentLength = 0;
                        auto it = request.headers.find("Content-Length");
                        if(it != request.headers.end()){
                            contentLength = std::stoul(it->second);
                        }
                        std::size_t bodyStart = headerEnd + 4;
                        std::size_t bodyLength = requestData.size() - bodyStart;
                        if(bodyLength < contentLength)
                            break;
                        request.body = requestData.substr(bodyStart, contentLength);
                        // std::cout << "Method: " << request.method << '\n';
                        // std::cout << "Path: " << request.path << '\n';
                        // std::cout << "Version: " << request.version << '\n';
                        // for(const auto& [key, value] : request.headers){
                        //     std::cout << key << ": " << value << '\n';
                        // }
                        //send http response
                        
                        HttpResponse response = router.route(request);
                        std::string responseData = serializeResponse(response);
                        send(socket, responseData.data(), responseData.size(), 0);

                        // epoll_ctl(epollFd, EPOLL_CTL_DEL, socket, nullptr);
                        // requestBuffers.erase(socket);
                        // close(socket);
                        requestBuffers[socket].erase(0, bodyStart + contentLength);
                        
                        headerEnd = requestData.find("\r\n\r\n");
                    }
                }
                
            }
            
        }
    }
    close(epollFd);
    close(serverSocket);
    return 0;
}