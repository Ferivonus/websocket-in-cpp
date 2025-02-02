#include <iostream>
#include "WebSocketServer.hpp"
#include "WebSocketClient.hpp"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [server|client]\n";
        return 1;
    }

    std::string mode = argv[1];
    if (mode == "server") {
        WebSocketServer server("8080");
        server.run();
    }
    else if (mode == "client") {
        WebSocketClient client("127.0.0.1", "8080");
        client.run();
    }
    else {
        std::cerr << "Invalid mode! Use 'server' or 'client'.\n";
        return 1;
    }
    return 0;
}