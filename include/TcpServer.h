#pragma once

#include "SocketStream.h"
#include "TcpConnection.h"

#include <atomic>
#include <future>
#include <memory>
#include <thread>

namespace Yam {
namespace Http {

class TcpServer {
public:
    TcpServer(const IPEndpoint&);
    virtual ~TcpServer();

    std::future<void> Start();
    void Stop();

    const IPEndpoint& GetEndpoint() const;

protected:
    virtual void OnAccepted(std::shared_ptr<TcpConnection>) = 0;

private:
    void ResetListenerSocket();
    void AcceptLoop();

    IPEndpoint _endpoint;
    SocketStream _socket;
    std::promise<void> _promise;
    std::thread _thread;
    std::atomic_bool _stop;
};

inline const IPEndpoint& TcpServer::GetEndpoint() const {
    return _endpoint;
}

} // namespace Http
} // namespace Yam

