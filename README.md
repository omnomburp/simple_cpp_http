# Simple Http

A C++ single-file header-only HTTP library, currently only usable on Unix like systems. This was made to be similar to net/http that you would find in Go. Winsock support will probably never be added.

### How to use
The constructor takes in the port that it listens on and the maximum connections allowed.
```c++
SimpleHttp::SimpleHttp svr(int port, int max_connections);
```
The callback functions that are registered to an endpoint have to be in this signature `void function_name(const int, HttpRequest)`.

`HttpRequest` is the struct that contains the parsed Http Request and is structured like this.
```c++
struct HttpRequest {
    std::string                                 method;
    std::string                                 path;
    std::string                                 http_version;
    std::unordered_map<std::string,std::string> headers;
    std::string                                 body;
}
```
Responses can be sent using `bool sendResponse(int client, string message, int response_number, ContentType content_type)`, `int client` will be the int from the first argument passed into your callback function.

After defining the callback function, it can be registered using `registerFunc(std::string your_endpoint, your_callback_function)`.

Finally call `init()` and the server will start listening.
