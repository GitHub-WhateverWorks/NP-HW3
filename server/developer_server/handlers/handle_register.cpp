#include "../developer_server.hpp"
#include "../../database/db.hpp"
#include <iostream>


void handle_register(TCPConnection &conn, const json &d) {
    std::cout << "[DEBUG][SERVER] handle_register() called\n";
    std::cout << "[DEBUG][SERVER] Incoming JSON: " << d.dump() << "\n";

    if (!d.contains("username") || !d.contains("password")) {
        std::cout << "[DEBUG][SERVER] Missing fields in packet\n";
        Packet res;
        res.type = PacketType::SERVER_RESPONSE;
        res.data["ok"] = false;
        res.data["kind"] = "DEV_REGISTER";
        res.data["msg"] = "Missing fields";
        conn.sendPacket(res);
        return;
    }

    std::string user = d["username"].get<std::string>();
    std::string pass = d["password"].get<std::string>();

    std::cout << "[DEBUG][SERVER] Register attempt: user=" << user 
              << " pass=" << pass << "\n";

    auto &db = Database::instance();

    int new_id = db.createDeveloper(user, pass);

    std::cout << "[DEBUG][SERVER] DB register result: dev_id=" << new_id << "\n";

    Packet res;
    res.type = PacketType::SERVER_RESPONSE;
    res.data["kind"] = "DEV_REGISTER";

    if (new_id < 0) {
        res.data["ok"] = false;
        res.data["msg"] = "Username already exists";
        std::cout << "[DEBUG][SERVER] Register failed — existing username\n";
    } else {
        res.data["ok"] = true;
        res.data["dev_id"] = new_id;
        res.data["msg"] = "Register OK";
        std::cout << "[DEBUG][SERVER] Register succeeded\n";
    }

    conn.sendPacket(res);
    std::cout << "[DEBUG][SERVER] Response sent\n";
}

