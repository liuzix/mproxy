//
// Created by zixiong on 11/24/17.
//

#ifndef PROXY_CONNECTION_H
#define PROXY_CONNECTION_H

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <list>
#include <mutex>
#include <iostream>
#include <unordered_map>

#include "request.h"

class Connection : public std::enable_shared_from_this<Connection> {
public:
    // no copying
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    explicit Connection(boost::asio::ip::tcp::socket &&socket, boost::asio::io_service& io_service);

    void go();

    ~Connection() {
        //delete _in_sslSock;
        //delete _out_sslSock;
        cout << "Connection destroyed!" << endl;
    }
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


    string currentHost;

    //Request* currentRequest;
    unique_ptr<Request> currentRequest;

    bool isSSL = false;



    void doReadFromClient(boost::asio::yield_context yield);

    void doReadFromServer(boost::asio::yield_context yield);

    void doConnectToWebServer(boost::asio::yield_context yield, Request& request);

    //void writeToWebServer(boost::asio::yield_context yield, boost::asio::streambuf& buf);


    /* Chunked Receiving */
    template <typename stream_type>
    void streamingReceive(stream_type& socket,
                          boost::asio::streambuf& rbuf,
                          function<void(vector<char>&)> func,
                          boost::asio::yield_context yield);

    template <typename stream_type>
    void streamingReceive(stream_type& socket,
                          size_t length,
                          boost::asio::streambuf& rbuf,
                          function<void(vector<char>&)> func,
                          boost::asio::yield_context yield);


    /* SSL related */
    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket&> ssl_stream_t;
    boost::asio::ssl::context _ctx;
    //boost::asio::ssl::stream<boost::asio::ip::tcp::socket&>* _in_sslSock;
    //boost::asio::ssl::stream<boost::asio::ip::tcp::socket&>* _out_sslSock;
    unique_ptr<ssl_stream_t> _in_sslSock;
    unique_ptr<ssl_stream_t> _out_sslSock;


    void upgradeToSSL(boost::asio::yield_context yield);



};




#endif //PROXY_CONNECTION_H
