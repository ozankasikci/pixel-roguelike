#pragma once

#include "editor/debug/CommandRegistry.h"

#include <string>
#include <vector>

class DebugServer {
public:
    DebugServer() = default;
    ~DebugServer();
    DebugServer(const DebugServer&) = delete;
    DebugServer& operator=(const DebugServer&) = delete;

    void init();
    void shutdown();
    void poll(CommandRegistry& registry);

    const std::string& socketPath() const { return socketPath_; }

private:
    struct ClientConnection {
        int fd = -1;
        std::string readBuffer;
    };

    void acceptNewConnections();
    void readClients(CommandRegistry& registry);
    void removeClient(std::size_t index);

    int listenFd_ = -1;
    std::string socketPath_;
    std::vector<ClientConnection> clients_;
};
