#include "WebSocketClient.hpp"
#include <boost/system/error_code.hpp>

WebSocketClient::WebSocketClient(const std::string& host, const std::string& port)
    : host(host), port(port), ws(std::make_unique<websocket::stream<tcp::socket>>(ioc)) {
}

WebSocketClient::~WebSocketClient() {
    if (reader_thread.joinable()) reader_thread.join();
}

void WebSocketClient::send_message(const std::string& message) {
    if (!is_connected) return;
    try {
        ws->write(net::buffer(message));
    }
    catch (const std::exception& e) {
        std::cerr << "Send Error: " << e.what() << std::endl;
    }
}

void WebSocketClient::read_messages() {
    try {
        beast::flat_buffer buffer;
        while (is_connected && ws->is_open()) {
            ws->read(buffer);
            std::string msg = beast::buffers_to_string(buffer.data());
            try {
                auto json = nlohmann::json::parse(msg);
                std::cout << "\n[Server] " << json.dump(4) << "\n> ";
            }
            catch (...) {
                std::cout << "\n[Received] " << msg << "\n> ";
            }
            buffer.consume(buffer.size());
        }
    }
    catch (...) {
        is_connected = false;
    }
}

void WebSocketClient::connect() {
    try {
        tcp::resolver resolver(ioc);
        auto const results = resolver.resolve(host, port);

        net::connect(ws->next_layer(), results.begin(), results.end());
        ws->handshake(host, "/");
        is_connected = true;

        std::cout << "Enter username: ";
        std::getline(std::cin, username);
        std::cout << "Enter password: ";
        std::getline(std::cin, password);

        reader_thread = std::thread(&WebSocketClient::read_messages, this);

        std::string input;
        std::cout << "Enter messages (type '/register' to register or 'exit' to quit):\n";
        while (is_connected) {
            std::cout << "> ";
            std::getline(std::cin, input);
            if (input == "exit") break;

            if (input == "/register") {

                nlohmann::json reg_json;
                reg_json["type"] = "register";
                reg_json["username"] = username;
                reg_json["password"] = password;
                send_message(reg_json.dump());
            }
            else {
                nlohmann::json msg_json;
                msg_json["type"] = "message";
                msg_json["username"] = username;
                msg_json["password"] = password;
                msg_json["content"] = input;
                send_message(msg_json.dump());
            }
        }

        ws->close(websocket::close_code::normal);
    }
    catch (const std::exception& e) {
        std::cerr << "Connection Error: " << e.what() << std::endl;
        is_connected = false;
    }
}

void WebSocketClient::run() {
    connect();
}