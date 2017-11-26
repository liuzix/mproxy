//
// Created by zixiong on 11/24/17.
//

#include "connection.h"
#include "request.h"

#include <boost/bind.hpp>
#include <iostream>
#include <boost/array.hpp>
#include <boost/algorithm/string.hpp>

#include <string>
#include <vector>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;

#define SET_QUIT setQuit(); return

#define CHECK_QUIT if (checkQuit()) { \
                       return;        \
                   }                  \


Connection::Connection(boost::asio::ip::tcp::socket &&socket, boost::asio::io_service& io_service)
        : _in_socket(std::move(socket)),
          _out_socket(io_service),
          _resolver(io_service)
{
    _io_service = &io_service;
    spawn(io_service, boost::bind(&Connection::doReadFromClient, this, _1));

}

inline ssize_t getContentLength(vector<pair<string, string>>& headers) {
    for (auto& kv: headers) {
        if (kv.first == "Content-Length") {
            return atoi(kv.second.data());
        }
    }

    return -1;
}

bool checkChuncked(vector<pair<string, string>>& headers) {
    for (auto& kv: headers) {
        if (kv.first == "Transfer-Encoding") {
            string value = kv.second;
            vector<string> splitRes;
            boost::split(splitRes, value, boost::is_any_of(","));
            for (auto& v: splitRes) {
                boost::trim(v);
                if (v == "chunked")
                    return true;
            }
        }
    }

    return false;

}

void Connection::doReadFromClient(boost::asio::yield_context yield) {
    cout << "Start reading from client" << endl;
    boost::system::error_code ec;
    for (;;) {
        CHECK_QUIT;
        boost::asio::streambuf rbuf;
        async_read_until(_in_socket, rbuf, "\r\n\r\n", yield[ec]);
        if (ec) {
            cout << ec.message() << endl;
            break;
        }


        auto newRequest = new Request(this);
        try {
            newRequest->parseRequestHeader(rbuf);
        } catch (exception &e) {
            cout << "doReadFromClient: " << e.what() << endl;
            SET_QUIT;
        }

        if (connectionStatus == ConnectionStatus::created) {
            // we are not connected to the web server yet!
            doConnectToWebServer(yield, *newRequest);
        }

        pendingRequestsMutex.lock();
        pendingRequests.push_back(newRequest);
        pendingRequestsMutex.unlock();

        boost::asio::streambuf buf;
        newRequest->writeOutGoingHeader(buf);

        async_write(_out_socket, buf.data(), yield[ec]);

        if (ec) {
            cout << ec.message() << endl;
            SET_QUIT;
        }

        // now we expect the body

        ssize_t contentLength = getContentLength(newRequest->requestHeaders);

        if (contentLength > 0) {
            // we have a fixed length

        } else {
            // may be chuncked?
            if (checkChuncked(newRequest->requestHeaders)) {
                cout << "doReadFromClient: " << "Chunked not supported" << endl;
                SET_QUIT;
            }
        }

        // no body

    }
}

void Connection::doConnectToWebServer(boost::asio::yield_context yield, Request& request) {
    string port;
    if (!request.urlInfo.port.empty())
        port = request.urlInfo.port;
    else
        port = request.urlInfo.protocol;

    boost::system::error_code ec;
    tcp::resolver::query query(request.urlInfo.hostname, port);
    auto it = _resolver.async_resolve(query, yield[ec]);

    if (ec) {
        cout << "doConnectToWebServer: " << ec.message() << endl;
        SET_QUIT;
        return;
    }

    async_connect(_out_socket, it, yield[ec]);

    if (ec) {
        cout << "doConnectToWebServer: " << ec.message() << endl;
        SET_QUIT;
        return;
    }

    connectionStatus = ConnectionStatus::connected;
    spawn(*_io_service, boost::bind(&Connection::doReadFromServer, this, _1));
    cout << "Connected to server: " << _out_socket.remote_endpoint() << endl;
}


void Connection::writeToWebServer(boost::asio::yield_context yield, boost::asio::streambuf &buf) {
    boost::system::error_code ec;
    auto sendBuf = buf.data();
    size_t nbytes;
    nbytes = async_write(_out_socket, sendBuf, yield[ec]);

    if (ec) {
        cout << "doWriteToWebServer: " << ec.message() << endl;
        SET_QUIT;
        return;
    } else {
        buf.consume(nbytes);
    }
}


void Connection::doReadFromServer(boost::asio::yield_context yield) {
    cout << "Start reading from server" << endl;
    boost::system::error_code ec;

    for (;;) {
        CHECK_QUIT;

        boost::asio::streambuf rbuf;
        async_read_until(_out_socket, rbuf, "\r\n\r\n", yield[ec]);
        if (ec) {
            cout << ec.message() << endl;
            break;
        }

        pendingRequestsMutex.lock();
        if (pendingRequests.empty()) {
            cout << "No Pending Request !" << endl;
            pendingRequestsMutex.unlock();
            SET_QUIT;
        }
        auto request = pendingRequests.front();
        pendingRequests.pop_front();
        pendingRequestsMutex.unlock();

        try {
            request->parseResponseHeader(rbuf);
        } catch (exception& e) {
            cout << "doReadFromServer: " << e.what() << endl;
            SET_QUIT;
        }

        cout << "Response code: " << request->responseCode << endl;

        // now we expect the body

        ssize_t contentLength = getContentLength(request->requestHeaders);

        if (contentLength > 0) {
            // we have a fixed length


        } else {
            // may be chuncked?
            if (checkChuncked(request->responseHeaders)) {
                cout << "doReadFromServer: " << "Chunked not supported" << endl;
                SET_QUIT;
            }
        }
    }

}


/*
 * We are closing a duplex tunnel, which consists of two data flows.
 * The data flow where an exception occurs calls setQuit() and stop sending requests.
 * The other checks closing status by calling checkQuit(), and quit accordingly.
 */

void Connection::setQuit() {
    checkQuit();
    connectionStatus = ConnectionStatus::closing;
    if (_in_socket.is_open()) {
        _in_socket.shutdown(tcp::socket::shutdown_receive);
    }

    if (_out_socket.is_open()) {
        _out_socket.shutdown(tcp::socket::shutdown_send);
    }
}

bool Connection::checkQuit() {
    if (connectionStatus == ConnectionStatus::closing) {
        std::cout << "We are quitting!\n";
        if (_in_socket.is_open()) {
            _in_socket.close();
        }
        if (_out_socket.is_open()) {
            _out_socket.close();
        }
        return true;
    }
    return false;
}

void Connection::streamingReceive(boost::asio::ip::tcp::socket &socket, function<void(boost::asio::streambuf &)> func,
                                  boost::asio::yield_context yield)
{

}
