#pragma once

#include "TcpServerBase.h"
#include "Poller.h"

#include <memory>

namespace Yam {
namespace Http {

class TcpServer : public TcpServerBase {
public:
    TcpServer(const IPEndpoint&, std::shared_ptr<Poller>);

protected:
    void OnAccepted(std::shared_ptr<TcpConnection>);

private:
    std::shared_ptr<Poller> _poller;
};

} // namespace Http
} // namespace Yam

