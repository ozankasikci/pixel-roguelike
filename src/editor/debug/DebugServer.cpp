#include "editor/debug/DebugServer.h"

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#endif

#include <cstdio>
#include <string>

DebugServer::~DebugServer() {
    shutdown();
}

void DebugServer::init() {
#if defined(__unix__) || defined(__APPLE__)
    char pathBuf[128];
    std::snprintf(pathBuf, sizeof(pathBuf), "/tmp/pixel-roguelike-editor-%d.sock",
                  static_cast<int>(getpid()));
    socketPath_ = pathBuf;

    listenFd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        spdlog::warn("DebugServer: failed to create socket: {}", strerror(errno));
        return;
    }

    // Set non-blocking on the listen fd
    int flags = fcntl(listenFd_, F_GETFL, 0);
    if (flags < 0 || fcntl(listenFd_, F_SETFL, flags | O_NONBLOCK) < 0) {
        spdlog::warn("DebugServer: failed to set O_NONBLOCK: {}", strerror(errno));
    }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, socketPath_.c_str(), sizeof(addr.sun_path) - 1);

    // Remove stale socket file if it exists
    unlink(socketPath_.c_str());

    if (bind(listenFd_, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0) {
        spdlog::warn("DebugServer: bind failed: {}", strerror(errno));
        close(listenFd_);
        listenFd_ = -1;
        return;
    }

    if (listen(listenFd_, 2) < 0) {
        spdlog::warn("DebugServer: listen failed: {}", strerror(errno));
        close(listenFd_);
        listenFd_ = -1;
        return;
    }

    spdlog::info("DebugServer: listening on {}", socketPath_);
#else
    spdlog::warn("DebugServer: Unix sockets not supported on this platform");
#endif
}

void DebugServer::shutdown() {
#if defined(__unix__) || defined(__APPLE__)
    for (auto& client : clients_) {
        if (client.fd >= 0) {
            close(client.fd);
        }
    }
    clients_.clear();

    if (listenFd_ >= 0) {
        close(listenFd_);
        listenFd_ = -1;
    }

    if (!socketPath_.empty()) {
        unlink(socketPath_.c_str());
        socketPath_.clear();
    }
#endif
}

void DebugServer::poll(CommandRegistry& registry) {
#if defined(__unix__) || defined(__APPLE__)
    if (listenFd_ < 0) {
        return;
    }

    acceptNewConnections();
    readClients(registry);
#endif
}

void DebugServer::acceptNewConnections() {
#if defined(__unix__) || defined(__APPLE__)
    while (true) {
        sockaddr_un clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(listenFd_, reinterpret_cast<sockaddr*>(&clientAddr), &clientLen);
        if (clientFd < 0) {
            // EAGAIN / EWOULDBLOCK means no pending connection — normal for non-blocking
            break;
        }

        // Set non-blocking on client fd
        int flags = fcntl(clientFd, F_GETFL, 0);
        if (flags >= 0) {
            fcntl(clientFd, F_SETFL, flags | O_NONBLOCK);
        }

        clients_.push_back(ClientConnection{clientFd, {}});
        spdlog::debug("DebugServer: client connected (fd={})", clientFd);
    }
#endif
}

void DebugServer::readClients(CommandRegistry& registry) {
#if defined(__unix__) || defined(__APPLE__)
    char buf[4096];

    for (std::size_t i = 0; i < clients_.size();) {
        ClientConnection& client = clients_[i];

        ssize_t n = recv(client.fd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            client.readBuffer.append(buf, static_cast<std::size_t>(n));

            // Process all complete newline-delimited messages
            std::size_t pos = 0;
            std::size_t nl;
            while ((nl = client.readBuffer.find('\n', pos)) != std::string::npos) {
                std::string line = client.readBuffer.substr(pos, nl - pos);
                pos = nl + 1;

                if (line.empty()) {
                    continue;
                }

                try {
                    auto req = nlohmann::json::parse(line);
                    std::string cmd = req.value("cmd", "");
                    nlohmann::json args = req.contains("args") ? req["args"] : nlohmann::json::object();
                    nlohmann::json response = registry.dispatch(cmd, args);

                    // Attach request id
                    if (req.contains("id")) {
                        response["id"] = req["id"];
                    }

                    std::string responseStr = response.dump() + "\n";
                    send(client.fd, responseStr.c_str(), responseStr.size(), 0);
                } catch (const std::exception& e) {
                    std::string errMsg = nlohmann::json({{"ok", false}, {"error", e.what()}}).dump() + "\n";
                    send(client.fd, errMsg.c_str(), errMsg.size(), 0);
                }
            }

            // Keep remaining incomplete data
            client.readBuffer = client.readBuffer.substr(pos);
            ++i;
        } else if (n == 0) {
            // Client disconnected cleanly
            spdlog::debug("DebugServer: client disconnected (fd={})", client.fd);
            removeClient(i);
            // Don't increment i — removeClient swaps this slot
        } else {
            // n < 0
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available right now — normal for non-blocking
                ++i;
            } else {
                spdlog::debug("DebugServer: recv error on fd {}: {}", client.fd, strerror(errno));
                removeClient(i);
            }
        }
    }
#endif
}

void DebugServer::removeClient(std::size_t index) {
#if defined(__unix__) || defined(__APPLE__)
    if (index >= clients_.size()) {
        return;
    }
    close(clients_[index].fd);
    clients_[index] = std::move(clients_.back());
    clients_.pop_back();
#endif
}
