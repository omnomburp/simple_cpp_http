#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <fcntl.h>

class SimpleHttp {

public:
    struct HttpRequest {
        std::string method;
        std::string path;
        std::string http_version;
        std::unordered_map<std::string,std::string> headers;
        std::string body;
    };

    enum ContentType {
        TEXT_PLAIN,
        TEXT_HTML,
        APPLICATION_JSON,
        APPLICATION_XML,
    };

    using HttpHandleFunc = void(*)(const int, HttpRequest);

    std::vector<char> received_data;

    SimpleHttp(int serverport, int connections): port(serverport), max_connections(connections) {}

    // kick starts everything and listens on a seperate thread
    bool init()
    {
        listening_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (listening_socket == -1)
        {
            std::cout << "Failed to start socket" << std::endl;
            return false;
        }

        sockaddr_in server_address;
        server_address.sin_family = AF_INET;
        server_address.sin_port = htons(this->port);
        server_address.sin_addr.s_addr = INADDR_ANY;

        if (bind(listening_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1)
        {
            std::cout << "Socket binding failed" << std::endl;
            return false;
        }

        accepting_thread = std::thread(&SimpleHttp::startListening, this, listening_socket);
        accepting_thread.detach();

        return true;
    }

    // register all the functions associated to your endpoint here
    void registerFunc(std::string endpoint, HttpHandleFunc func)
    {
        func_map[endpoint] = func;
    }

    static bool sendResponse(const int client, const std::string& message, int response_number,
     ContentType content_type = ContentType::TEXT_PLAIN)
    {
        std::ostringstream oss;  
        oss << "HTTP/1.1 " << std::to_string(response_number) << "\r\n";
        oss << "Access-Control-Allow-Origin: *\r\n";
        oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
        oss << "Access-Control-Allow-Headers: Content-Type\r\n";

        switch(content_type) {
            case ContentType::TEXT_PLAIN:
                oss << "Content-Type: text/plain\r\n";
                break;
            case ContentType::TEXT_HTML:
                oss << "Content-Type: text/html\r\n";
                break;
            case ContentType::APPLICATION_XML:
                oss << "Content-Type: application/xml\r\n";
                break;
            case ContentType::APPLICATION_JSON:
                oss << "Content-Type: application/json\r\n";
                break;
        }
        oss << "Content-Length: " << message.length() << "\r\n";
        oss << "\r\n";
        oss << message;
        std::string response(oss.str());
        ssize_t sent_bytes = send(client, response.c_str(), response.length(), 0);
        if (sent_bytes == -1)
        {
            std::cout << "Failed to send message" << std::endl;
            return false;
        }
        else 
        {
            std::cout << "Message sent" << std::endl;
        }
        return true;
    }



private:
    std::string address;
    int port;
    int max_connections;
    int listening_socket;
    std::mutex mtx;
    std::thread accepting_thread;
    std::vector<std::thread> connected_threads;
    std::unordered_map<std::string, HttpHandleFunc> func_map;

    std::string trim(const std::string& str) const
    {
        size_t first = str.find_first_not_of(" \t\n\r");
        size_t last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, last - first + 1);
    }   

    // parses the http request string into the HttpRequest struct
    HttpRequest parseHttpReq(const std::string& reqstr) const
    {
        HttpRequest request;
        std::istringstream iss(reqstr);
        std::string line;
        std::getline(iss, line);
        std::istringstream req_line_stream(line);
        req_line_stream >> request.method >> request.path >> request.http_version;
        while(std::getline(iss, line) && !line.empty())
        {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos)
            {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                request.headers[trim(key)] = trim(value);
            }
        }

        std::ostringstream body_stream;
        body_stream << line;
        request.body = body_stream.str();

        return request;
    }

    void startListening(const int listener)
    {
        while (true)
        {
            if (listen(listener, max_connections) == -1)
            {
                std::cout << "Failed to start listening" << std::endl;
                continue;
            }

            int client_socket = accept(listener, nullptr, nullptr);
            if (client_socket == -1)
            {
                std::cout << "Error accepting connection" << std::endl;
                continue;
            }
            int flags = fcntl(client_socket, F_GETFL, 0);
            flags |= O_NONBLOCK;

            std::thread new_thread(&SimpleHttp::receiveMessage, this, client_socket);
            new_thread.detach();
            connected_threads.push_back(std::move(new_thread));
        }
    }

    // receive and parse http req then invokes the registered function
    void receiveMessage(const int client)
    {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(client, &read_fds);
        ssize_t bytes_read;
        std::ostringstream request_stream;
        const size_t buffer_size = 1024;
        std::vector<char> buffer(buffer_size);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  
        while (true)
        {
            int select_result = select(client + 1, &read_fds, NULL, NULL, &timeout);
            if (select_result == -1) 
            {
                return;
            } 
            else if (select_result == 0) 
            {
                break;
            } 
            else 
            {
                bytes_read = read(client, buffer.data(), buffer.size());
                request_stream.write(buffer.data(), bytes_read);
            }
        }   
        std::string request_str = request_stream.str();
        HttpRequest request = parseHttpReq(request_str);
        if (func_map.find(request.path) != func_map.end()) 
        {
            func_map[request.path](client, request);
        } 
        else 
        {
            sendResponse(client, "Not Found", 404, ContentType::TEXT_PLAIN);
        }
        close(client);
    }
};
