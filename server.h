//
// Created by zixiong on 11/24/17.
//

#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>

#include "connection.h"
#include "request.h"

using namespace std;

class Server {
public:
    // no copying
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    explicit Server(int port, int numworker, int timeout, string logPath);

private:
    boost::asio::io_service _io_service;
    boost::asio::ip::tcp::acceptor _acceptor;
    vector<std::thread> _threadPool;
    vector<shared_ptr<Connection>> _connections;
    mutex _connections_mutex;

    void doAccept(boost::asio::yield_context yield);
    void onNewConnection(boost::asio::ip::tcp::socket& socket);
};


#endif //PROXY_SERVER_H
