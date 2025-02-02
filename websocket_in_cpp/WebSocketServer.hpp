#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#ifndef WEBSOCKETSERVER_HPP
#define WEBSOCKETSERVER_HPP

#include <iostream>
#include <thread>
#include <mutex>
#include <unordered_set>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/websocket.hpp>
#include <sqlite3.h>
#include <nlohmann/json.hpp>

namespace net = boost::asio;
using tcp = net::ip::tcp;
namespace websocket = boost::beast::websocket;
namespace beast = boost::beast;

class WebSocketServer {
public:
    WebSocketServer(const std::string& port);
    ~WebSocketServer();
    void run();

private:
    std::string port;
    std::unordered_set<std::shared_ptr<websocket::stream<tcp::socket>>> clients;
    std::mutex client_mutex;
    sqlite3* db;

    void session(tcp::socket socket);
    void broadcast(std::shared_ptr<websocket::stream<tcp::socket>> sender_ws, const std::string& message);
    void remove_client(std::shared_ptr<websocket::stream<tcp::socket>> ws);
};

#endif // WEBSOCKETSERVER_HPP