#ifndef TCP_HPP
#define TCP_HPP

#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <functional>
#include <thread>
#include <atomic>
#include <cerrno>

#include "packet.hpp"

class TCPConnection {
    int sock;

public:
    void *owner;   

    TCPConnection() : sock(-1), owner(nullptr) {}
    explicit TCPConnection(int existingSock)
        : sock(existingSock), owner(nullptr) {}

    TCPConnection(const TCPConnection &) = delete;
    TCPConnection &operator=(const TCPConnection &) = delete;

    TCPConnection(TCPConnection &&other) noexcept
        : sock(other.sock), owner(other.owner) {
        other.sock = -1;
        other.owner = nullptr;
    }

    TCPConnection &operator=(TCPConnection &&other) noexcept {
        if (this != &other) {
            if (sock != -1) {
                ::close(sock);
            }
            sock = other.sock;
            owner = other.owner;
            other.sock = -1;
            other.owner = nullptr;
        }
        return *this;
    }

    ~TCPConnection() {
        if (sock != -1) {
            ::close(sock);
        }
    }

    bool connectToServer(const std::string &host, int port) {
        sock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("socket");
            return false;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(port);
        if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
            perror("inet_pton");
            ::close(sock);
            sock = -1;
            return false;
        }

        if (::connect(sock,
                      reinterpret_cast<sockaddr*>(&addr),
                      sizeof(addr)) < 0) {
            perror("connect");
            ::close(sock);
            sock = -1;
            return false;
        }
        return true;
    }

    bool sendPacket(const Packet &p) {
        std::string out = p.serialize();
        if (out.empty() || out.back() != '\n')
            out.push_back('\n');

        ssize_t sent = ::send(sock, out.c_str(), out.size(), 0);
        if (sent != (ssize_t)out.size()) {
            perror("send");
            return false;
        }
        return true;
    }

    bool recvLine(std::string &line) {
        line.clear();
        char c;
        while (true) {
            ssize_t n = ::recv(sock, &c, 1, 0);
            if (n <= 0)
                return false;
            if (c == '\n')
                break;
            line.push_back(c);
        }
        return true;
    }

    bool recvPacket(Packet &p) {
        std::string line;
        if (!recvLine(line)) {
            std::cerr << "[TCP] recvLine failed\n";
            return false;
        }
        if (line.empty()) {
            std::cerr << "[TCP] recvPacket: got empty line, ignoring\n";

        }
        try {
            p = Packet::deserialize(line);
        } catch (std::exception &e) {
            std::cerr << "[TCP] Packet parse error: " << e.what() << "\n";
            return false;
        }
        return true;
    }

    int fd() const { return sock; }
    int raw() const { return sock; }
};

class TCPServer {
public:
    TCPServer() : m_sock(-1), m_running(false) {}
    ~TCPServer() {
        stop();
    }

    bool start(int port, std::function<void(TCPConnection)> handler) {
        if (m_running) {
            std::cerr << "[TCPServer] Already running\n";
            return false;
        }

        m_sock = ::socket(AF_INET, SOCK_STREAM, 0);
        if (m_sock < 0) {
            perror("socket");
            return false;
        }

        int opt = 1;
        ::setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(port);

        if (::bind(m_sock,
                   reinterpret_cast<sockaddr*>(&addr),
                   sizeof(addr)) < 0) {
            perror("bind");
            return false;
        }

        if (::listen(m_sock, 16) < 0) {
            perror("listen");
            return false;
        }

        m_running = true;

        m_acceptThread = std::thread([this, handler]() {
            while (m_running) {
                sockaddr_in caddr{};
                socklen_t clen = sizeof(caddr);

                int csock = ::accept(
                    m_sock,
                    reinterpret_cast<sockaddr*>(&caddr),
                    &clen
                );

                if (csock < 0) {
                    if (errno == EINTR) continue;
                    perror("accept");
                    break;
                }

                TCPConnection conn(csock);
                handler(std::move(conn));
            }
        });

        return true;
    }

    void stop() {
        if (!m_running) return;
        m_running = false;
        if (m_sock != -1) {
            ::close(m_sock);
            m_sock = -1;
        }
        if (m_acceptThread.joinable())
            m_acceptThread.join();
    }
    bool sendRawPacket(int fd, const Packet &p) {
        std::string line = p.serialize();

        ssize_t n = ::send(fd, line.c_str(), line.size(), 0);
        return (n == (ssize_t)line.size());
    }
private:
    int m_sock;
    std::atomic<bool> m_running;
    std::thread m_acceptThread;
};

#endif
