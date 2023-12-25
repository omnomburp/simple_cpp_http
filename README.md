# simple_cpp_http
Simple Http server library implemented in c++, meant to be similar to net/http in Golang

Create a instance of SimpleHttp and call init() on it.
After that you can call registerFunc() to register a function with an endpoint.
The function signature is void func(const int, HttpRequest)
Use the static function sendResponse to send a response to the Http Client.
