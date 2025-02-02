#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#ifndef WEBSOCKETCLIENT_HPP
#define WEBSOCKETCLIENT_HPP

#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <nlohmann/json.hpp>

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace websocket = boost::beast::websocket;
namespace beast = boost::beast;

class WebSocketClient {
public:
    WebSocketClient(const std::string& host, const std::string& port);
    ~WebSocketClient();
    void run();

private:
    std::string host;
    std::string port;
    net::io_context ioc;
    std::unique_ptr<websocket::stream<tcp::socket>> ws;
    std::thread reader_thread;
    bool is_connected = false;
    std::string username;
    std::string password;

    void connect();
    void send_message(const std::string& message);
    void read_messages();
};

#endif // WEBSOCKETCLIENT_HPP