#include "WebSocketServer.hpp"
#include <memory>

WebSocketServer::WebSocketServer(const std::string& port) : port(port), db(nullptr) {
    int rc = sqlite3_open("users.db", &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
        sqlite3_close(db);
        db = nullptr;
        return;
    }
    const char* create_table_sql =
        "CREATE TABLE IF NOT EXISTS users ("
        "username TEXT PRIMARY KEY,"
        "password TEXT NOT NULL);";
    char* errMsg = nullptr;
    rc = sqlite3_exec(db, create_table_sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}

WebSocketServer::~WebSocketServer() {
    if (db) {
        sqlite3_close(db);
    }
}

void WebSocketServer::broadcast(std::shared_ptr<websocket::stream<tcp::socket>> sender_ws, const std::string& message) {
    std::lock_guard<std::mutex> lock(client_mutex);
    auto it = clients.begin();
    while (it != clients.end()) {
        auto ws = *it;
        if (ws == sender_ws || !ws->is_open()) {
            ++it;
            continue;
        }
        try {
            ws->write(net::buffer(message));
            ++it;
        }
        catch (...) {
            it = clients.erase(it);
        }
    }
}

void WebSocketServer::remove_client(std::shared_ptr<websocket::stream<tcp::socket>> ws) {
    std::lock_guard<std::mutex> lock(client_mutex);
    clients.erase(ws);
}

void WebSocketServer::session(tcp::socket socket) {
    auto ws = std::make_shared<websocket::stream<tcp::socket>>(std::move(socket));
    try {
        ws->accept();
        {
            std::lock_guard<std::mutex> lock(client_mutex);
            clients.insert(ws);
        }

        beast::flat_buffer buffer;
        while (true) {
            buffer.clear();
            ws->read(buffer);
            std::string msg = beast::buffers_to_string(buffer.data());

            try {
                auto json = nlohmann::json::parse(msg);
                std::string type = json["type"];

                if (type == "register") {
                    std::string username = json["username"];
                    std::string password = json["password"];

                    sqlite3_stmt* stmt;
                    const char* check_sql = "SELECT username FROM users WHERE username = ?;";
                    int rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, nullptr);
                    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
                    rc = sqlite3_step(stmt);
                    bool exists = (rc == SQLITE_ROW);
                    sqlite3_finalize(stmt);

                    nlohmann::json response;
                    if (exists) {
                        response["status"] = "error";
                        response["message"] = "Username already exists";
                    }
                    else {
                        const char* insert_sql = "INSERT INTO users (username, password) VALUES (?, ?);";
                        rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr);
                        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
                        sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
                        rc = sqlite3_step(stmt);
                        sqlite3_finalize(stmt);
                        if (rc != SQLITE_DONE) {
                            throw std::runtime_error("Failed to insert user");
                        }
                        response["status"] = "success";
                        response["message"] = "Registration successful";
                    }
                    ws->write(net::buffer(response.dump()));
                }
                else if (type == "message") {
                    std::string username = json["username"];
                    std::string password = json["password"];
                    std::string content = json["content"];

                    sqlite3_stmt* stmt;
                    const char* auth_sql = "SELECT username FROM users WHERE username = ? AND password = ?;";
                    int rc = sqlite3_prepare_v2(db, auth_sql, -1, &stmt, nullptr);
                    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
                    rc = sqlite3_step(stmt);
                    bool authenticated = (rc == SQLITE_ROW);
                    sqlite3_finalize(stmt);

                    nlohmann::json response;
                    if (authenticated) {
                        std::string broadcast_msg = username + ": " + content;
                        broadcast(ws, broadcast_msg);
                        response["status"] = "success";
                    }
                    else {
                        response["status"] = "error";
                        response["message"] = "Authentication failed";
                    }
                    ws->write(net::buffer(response.dump()));
                }
                else {
                    nlohmann::json response;
                    response["status"] = "error";
                    response["message"] = "Invalid request type";
                    ws->write(net::buffer(response.dump()));
                }
            }
            catch (const std::exception& e) {
                nlohmann::json response;
                response["status"] = "error";
                response["message"] = "Invalid request format";
                ws->write(net::buffer(response.dump()));
            }
        }
    }
    catch (...) {
        remove_client(ws);
    }
}

void WebSocketServer::run() {
    try {
        net::io_context ioc;
        tcp::acceptor acceptor(ioc, tcp::endpoint(tcp::v4(), std::stoi(port)));
        std::cout << "Server running on port " << port << "..." << std::endl;

        while (true) {
            tcp::socket socket(ioc);
            acceptor.accept(socket);
            std::thread(&WebSocketServer::session, this, std::move(socket)).detach();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Server Error: " << e.what() << std::endl;
    }
}