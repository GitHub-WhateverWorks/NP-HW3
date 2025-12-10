#ifndef PACKET_HPP
#define PACKET_HPP

#include "json.hpp"
#include "protocol.hpp"
#include <string>
#include <iostream>

struct Packet {
    PacketType type;
    nlohmann::json data;

    std::string serialize() const {
        nlohmann::json j;
        j["type"] = (int)type;
        j["data"] = data;
        return j.dump() + "\n";
    }

    static Packet deserialize(const std::string &s) {
        auto j = nlohmann::json::parse(s);
        //std::cout<< "Packet received "<<j.dump()<<"\n";
        Packet p;
        p.type = (PacketType)j["type"].get<int>();
        p.data = j["data"];
        return p;
    }
};

#endif
