#include "TcpServer.h"
#include "SystemError.h"

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>

namespace Yam {
namespace Http {

namespace {

void EnableAddressReuse(SocketStream& socket) {
    int optValue = 1;
    if( -1 == ::setsockopt(socket.GetNativeHandle(),
                             SOL_SOCKET,
                             SO_REUSEADDR,
                             &optValue,
                             sizeof(optValue))) {
        throw SystemError{};
    }
}

void BindTo(SocketStream& socket, const IPEndpoint& ep) {
    auto addr = ep.GetAddrInfo();
    auto paddr = reinterpret_cast<::sockaddr*>(addr.get());
    if (-1 == ::bind(socket.GetNativeHandle(), paddr, sizeof(*addr)))
        throw SystemError{};
}

void Listen(SocketStream& socket) {
    if (-1 == ::listen(socket.GetNativeHandle(), SOMAXCONN))
        throw SystemError{};
}

} // unnamed namespace

TcpServer::TcpServer(const IPEndpoint& ep, std::shared_ptr<ThreadPool> tp) :
    _endpoint{ep},
    _threadPool{std::move(tp)},
    _stop{true} {}

TcpServer::~TcpServer() {
    Stop();
}

void TcpServer::ResetListenerSocketStream() {
    _socket = SocketStream{};

    auto fd = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

    if (fd == -1)
        throw SystemError();

    _socket = SocketStream{fd};

    EnableAddressReuse(_socket);
    BindTo(_socket, _endpoint);
    Listen(_socket);
}

std::future<void> TcpServer::Start(ConnectionHandler ch) {
    if (!_stop || _thread.joinable())
        throw std::logic_error("Start() called when TCP server is already running");

    // reset state
    ResetListenerSocketStream();
    _stop = false;
    _promise = std::promise<void>();

    // dispatch background worker thread
    _thread = std::thread([=] {
        AcceptLoop(std::move(ch));
    });

    return _promise.get_future();
}

void TcpServer::AcceptLoop(ConnectionHandler connectionHandler) {
    while (!_stop) {
        ::sockaddr_in saddr{0};
        ::socklen_t saddr_size = sizeof(saddr);

        // block until a new connection is accepted
        int ret = ::accept(_socket.GetNativeHandle(),
                            reinterpret_cast<::sockaddr*>(&saddr),
                            &saddr_size);

        if (ret == -1) {
            if (_stop) {
                // OK: we were supposed to stop
                _promise.set_value();
            } else {
                // Oops: something bad happened
                _stop = true;
                _promise.set_exception(std::make_exception_ptr(SystemError{}));
            }
            return;
        }

        // we've got a new connection at hand;
        // trigger connection handler callback.

        _threadPool->Post([=] {
            IPEndpoint endpoint{saddr};
            auto conn = std::make_shared<TcpConnection>(ret, endpoint);
            connectionHandler(std::move(conn));
        });
    }

    // all work is done. notify future.
    _promise.set_value();
}

void TcpServer::Stop() {
    bool reentrance = false;
    if (!_stop.compare_exchange_strong(reentrance, true))
        return;

    _socket = SocketStream{};

    if (_thread.joinable())
        _thread.join();
}

} // namespace Http
} // namespace Yam

