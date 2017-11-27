//
// Created by zixiong on 11/24/17.
//

#include "server.h"
#include <iostream>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

using namespace std;
using namespace boost::asio;

Server::Server(int port, int numworker, int timeout, string logPath)
    : _acceptor(_io_service, ip::tcp::endpoint(ip::tcp::v4(), (unsigned short)port))
{
    Request::setLogPath(logPath);

    spawn(_io_service, boost::bind(&Server::doAccept, this, _1));

    for (int i = 0; i < numworker; i++) {
        std::thread t([this] {
            this->_io_service.run();
        });
        _threadPool.push_back(std::move(t));
    }

    for (int i = 0; i < numworker; i++)
        _threadPool[i].join();
}

void Server::onNewConnection(boost::asio::ip::tcp::socket& socket) {
    ip::tcp::socket newSocket = std::move(socket);
    cout << "New Connection from " << newSocket.remote_endpoint() << endl;

    //_connections_mutex.lock();
    //_connections.push_back(std::make_shared<Connection>(std::move(newSocket), _io_service));
    //_connections_mutex.unlock();
    std::make_shared<Connection>(std::move(newSocket), _io_service)->go();

}

void Server::doAccept(boost::asio::yield_context yield) {
    ip::tcp::socket acceptSocket(_io_service);
    boost::system::error_code ec;
    for (;;) {
        _acceptor.async_accept(acceptSocket, yield[ec]);

        if (ec) {
            cerr << "doAccept: " << ec.message() << endl;
            continue;
        }

        onNewConnection(acceptSocket);
    }
}
