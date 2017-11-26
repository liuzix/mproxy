//
// Created by zixiong on 11/24/17.
//

#ifndef PROXY_CONNECTION_H
#define PROXY_CONNECTION_H

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <list>
#include <mutex>

#include "request.h"

class Connection {
public:
    // no copying
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    explicit Connection(boost::asio::ip::tcp::socket &&socket, boost::asio::io_service& io_service);

private:
    // the state of the class object
    enum class ConnectionStatus {created, connected, closing, error};
    enum class SingleSideStatus {idle, header, body};

    boost::asio::ip::tcp::socket _in_socket, _out_socket;
    boost::asio::ip::tcp::resolver _resolver;
    boost::asio::io_service* _io_service;

    ConnectionStatus connectionStatus = ConnectionStatus::created;
    SingleSideStatus clientStatus = SingleSideStatus::idle;
    SingleSideStatus serverStatus = SingleSideStatus::idle;

    std::list<Request*> pendingRequests;
    std::mutex pendingRequestsMutex;

    void doReadFromClient(boost::asio::yield_context yield);

    void doReadFromServer(boost::asio::yield_context yield);

    void doConnectToWebServer(boost::asio::yield_context yield, Request& request);

    void writeToWebServer(boost::asio::yield_context yield, boost::asio::streambuf& buf);

    void streamingReceive(boost::asio::ip::tcp::socket& socket,
                          function<void(boost::asio::streambuf&)> func,
                          boost::asio::yield_context yield);
    void setQuit();

    bool checkQuit();

};




#endif //PROXY_CONNECTION_H
