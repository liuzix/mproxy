//
// Created by zixiong on 11/24/17.
//

#include "connection.h"
#include "request.h"
#include "cert.h"

#include <boost/bind.hpp>
#include <iostream>
#include <boost/array.hpp>
#include <boost/algorithm/string.hpp>

#include <string>
#include <vector>
#include <boost/lexical_cast.hpp>

using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;

#define SET_QUIT setQuit(); return

#define CHECK_QUIT if (checkQuit()) { \
                       return;        \
                   }                  \


#define STREAM_IN(func, ...) (isSSL ? func(*_in_sslSock,  __VA_ARGS__) : func(_in_socket, __VA_ARGS__))

#define STREAM_OUT(func, ...) (isSSL ? func(*_out_sslSock,  __VA_ARGS__) : func(_out_socket, __VA_ARGS__))


Connection::Connection(boost::asio::ip::tcp::socket &&socket, boost::asio::io_service& io_service)
        : _in_socket(std::move(socket)),
          _out_socket(io_service),
          _resolver(io_service),
          _ctx(ssl::context::sslv23),
          _in_sslSock(nullptr),
          _out_sslSock(nullptr)

{
    _io_service = &io_service;

}

void Connection::go() {
    spawn(*_io_service, boost::bind(&Connection::doReadFromClient, shared_from_this(), _1));
}


inline ssize_t getContentLength(vector<pair<string, string>>& headers) {
    for (auto& kv: headers) {
        if (kv.first == "Content-Length" || kv.first == "content-length") {
            return boost::lexical_cast<ssize_t>(kv.second.data());
        }
    }

    return -1;
}

bool checkChuncked(vector<pair<string, string>>& headers) {
    for (auto& kv: headers) {
        if (kv.first == "Transfer-Encoding" || kv.first == "transfer-encoding") {
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


// this is the main loop of the conncection

void Connection::doReadFromClient(boost::asio::yield_context yield) {
    try {
        cout << "Start reading from client" << endl;
        boost::system::error_code ec;
        for (;;) {
            CHECK_QUIT;
            currentRequest = new Request(this, _in_socket.remote_endpoint().address().to_string());
            boost::asio::streambuf rbuf;

            SSL_retry:

            STREAM_IN(async_read_until, rbuf, "\r\n\r\n", yield[ec]);
            if (ec) {
                cout << ec.message() << endl;
                break;
            }


            try {
                currentRequest->parseRequestHeader(rbuf);
            } catch (exception &e) {
                cout << "doReadFromClient: " << e.what() << endl;
                SET_QUIT;
            }

            if (currentRequest->method == "CONNECT") {
                upgradeToSSL(yield);
                connectionStatus = ConnectionStatus::created;
                goto SSL_retry;
            }

            if (currentRequest->urlInfo.hostname != currentHost) {
                connectionStatus = ConnectionStatus::created;
                cout << "Change of host, reconnecting" << endl;
            }


            if (connectionStatus == ConnectionStatus::created) {
                // we are not connected to the web server yet!
                doConnectToWebServer(yield, *currentRequest);

                if (connectionStatus != ConnectionStatus::connected) {
                    SET_QUIT;
                }
                currentHost = currentRequest->urlInfo.hostname;
            }

            boost::asio::streambuf buf;
            currentRequest->writeOutGoingHeader(buf);

            STREAM_OUT(async_write, buf.data(), yield[ec]);

            if (ec) {
                cout << ec.message() << endl;
                SET_QUIT;
            }

            // now we expect the body

            ssize_t contentLength = getContentLength(currentRequest->requestHeaders);

            if (contentLength > 0) {
                // we have a fixed length
                cout << "Sending post data length: " << contentLength << endl;
                STREAM_IN(streamingReceive, (size_t) contentLength, rbuf, [=](vector<char> &buf) {
                    currentRequest->pushRequestBody(buf);
                    boost::asio::streambuf tempBuf;
                    currentRequest->pullRequestBody(false, tempBuf);  // pull chunked data
                    STREAM_OUT(async_write, tempBuf, yield);
                    //write(_in_socket, tempBuf);
                }, yield);

            } else {
                // may be chuncked?
                if (checkChuncked(currentRequest->requestHeaders)) {
                    cout << "doReadFromClient: " << "Chunked not supported" << endl;
                    SET_QUIT;
                }
            }

            doReadFromServer(yield);

        }
    } catch (exception& e) {
        cout << "Fatal error: " << e.what() << endl;
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

    if (isSSL) {
        _out_sslSock = new ssl::stream<tcp::socket&>(_out_socket, _ctx);
        _out_sslSock->async_handshake(ssl::stream<tcp::socket>::client, yield[ec]);

        if (ec) {
            cout << "doConnectToWebServer: " << ec.message() << endl;
            SET_QUIT;
        }

        cout << "Connected to remote via SSL" << endl;
    }

    connectionStatus = ConnectionStatus::connected;
    //spawn(*_io_service, boost::bind(&Connection::doReadFromServer, this, _1));

    cout << "Connected to server: " << _out_socket.remote_endpoint() << endl;
}


void Connection::writeToWebServer(boost::asio::yield_context yield, boost::asio::streambuf &buf) {
    boost::system::error_code ec;
    auto sendBuf = buf.data();
    size_t nbytes;
    nbytes = STREAM_OUT(async_write, sendBuf, yield[ec]);

    if (ec) {
        cout << "doWriteToWebServer: " << ec.message() << endl;
        SET_QUIT;
    } else {
        buf.consume(nbytes);
    }
}


void Connection::doReadFromServer(boost::asio::yield_context yield) {
    cout << "Start reading from server" << endl;
    boost::system::error_code ec;

    CHECK_QUIT;

    boost::asio::streambuf rbuf;
    STREAM_OUT(async_read_until, rbuf, "\r\n\r\n", yield[ec]);
    if (ec) {
        cout << ec.message() << endl;
    }


    try {
        currentRequest->parseResponseHeader(rbuf);
    } catch (exception& e) {
        cout << "doReadFromServer: " << e.what() << endl;
        SET_QUIT;
    }

    cout << "Response code: " << currentRequest->responseCode << endl;

    boost::asio::streambuf buf;
    currentRequest->writeInComingHeader(buf);

    STREAM_IN(async_write, buf.data(), yield[ec]);

    if (ec) {
        cout << ec.message() << endl;
        SET_QUIT;
    }

    // now we expect the body

    ssize_t contentLength = getContentLength(currentRequest->responseHeaders);

    try {
        if (contentLength > 0) {
            // we have a fixed length
            STREAM_OUT(streamingReceive, (size_t) contentLength, rbuf, [=](vector<char> &buf) {
                currentRequest->pushResponseBody(buf);
                boost::asio::streambuf tempBuf;
                currentRequest->pullResponseBody(false, tempBuf);  // pull chunked data
                STREAM_IN(async_write, tempBuf, yield);
            }, yield);


        } else {
            // may be chuncked?
            if (checkChuncked(currentRequest->responseHeaders)) {
                STREAM_OUT(streamingReceive, rbuf, [=](vector<char> &buf) {
                    //cout.write(buf.data(), buf.size());
                    currentRequest->pushResponseBody(buf);
                    boost::asio::streambuf tempBuf;
                    currentRequest->pullResponseBody(true, tempBuf);  // pull chunked data
                    STREAM_IN(async_write, tempBuf, yield);
                }, yield);

                boost::asio::streambuf tempBuf;
                currentRequest->pullResponseBody(true, tempBuf);  // pull chunked data
                STREAM_IN(async_write, tempBuf, yield);
            }
        }
    } catch (exception& e) {
        cout << "doReadFromServer: " << e.what() << endl;
    }

    delete currentRequest;

}



void Connection::setQuit() {
    checkQuit();
    connectionStatus = ConnectionStatus::closing;

    if (_in_socket.is_open()) {
        //_in_socket.shutdown(tcp::socket::shutdown_receive);
        _in_socket.close();
    }

    if (_out_socket.is_open()) {
        //_out_socket.shutdown(tcp::socket::shutdown_send);
        _out_socket.close();
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


/*
 * Chunked reception of http body
 * func is called for each chunk
 */
template <typename stream_type>
void Connection::streamingReceive(stream_type& socket,
                                  boost::asio::streambuf& rbuf,
                                  function<void(vector<char>&)> func,
                                  boost::asio::yield_context yield)
{
    boost::system::error_code ec;
    for (;;) {

        size_t nbytes;
        nbytes = async_read_until(socket, rbuf, "\r\n", yield[ec]);

        if (ec) {
            cout << "streamingReceive: " << ec.message() << endl;
            SET_QUIT;
        }



        size_t chunkSize;
        istream is(&rbuf);
        is >> std::hex >> chunkSize;

        rbuf.consume(2); // \r\n

        if (chunkSize == 0) {   // end of body
            cout << "End of body!" << endl;
            //vector<char> chunkBytes(0);
            //func(chunkBytes);
            if (rbuf.size() < 2) {
                nbytes = async_read(socket, rbuf, transfer_exactly(2), yield[ec]);   // \r\n
                //assert(nbytes == 2);
                if (ec) {
                    cout << "streamingReceive: " << ec.message() << endl;
                    SET_QUIT;
                }
            }

            break;
        }

        ssize_t readSize = chunkSize - rbuf.size();
        if (readSize > 0) {
            nbytes = async_read(socket, rbuf, transfer_exactly(readSize + 2), yield[ec]);
            //assert(nbytes == readSize);

            if (ec) {
                cout << "streamingReceive: " << ec.message() << endl;
                SET_QUIT;
            }

        }

        vector<char> chunkBytes(chunkSize);
        is.read(chunkBytes.data(), chunkSize);
        rbuf.consume(2); // \r\n

        func(chunkBytes);  // call the callback

    }
}

#define DEFAULT_CHUNK_SIZE 8192

/* Fixed length reception of http body */
template <typename stream_type>
void Connection::streamingReceive(stream_type& socket,
                                  size_t length,
                                  boost::asio::streambuf &rbuf,
                                  function<void(vector<char>&)> func,
                                  boost::asio::yield_context yield) {
    boost::system::error_code ec;
    size_t remaining = length;
    istream is(&rbuf);

    if (rbuf.size() > 0) {  // clear the remaining data;
        size_t chunkSize = min(rbuf.size(), remaining);
        vector<char> chunkBytes(chunkSize);
        is.read(chunkBytes.data(), chunkSize);
        func (chunkBytes);
        remaining -= chunkSize;
    }

    while (remaining > 0) {
        size_t readSize = min((size_t)DEFAULT_CHUNK_SIZE, remaining);

        async_read(socket, rbuf, transfer_exactly(readSize), yield[ec]);   // \r\n
        if (ec) {
            cout << "streamingReceive: " << ec.message() << endl;
            SET_QUIT;
        }

        vector<char> chunkBytes(readSize);
        is.read(chunkBytes.data(), readSize);

        func (chunkBytes);

        remaining -= readSize;
    }

}

void Connection::upgradeToSSL(boost::asio::yield_context yield) {
    assert(currentRequest->method == "CONNECT");


    boost::system::error_code ec;

    boost::asio::streambuf sbuf;
    ostream os(&sbuf);

    os << "HTTP/1.1 200 OK\r\n\r\n";

    async_write(_in_socket, sbuf, yield[ec]);

    if (ec) {
        cout << "upgradeToSSL: " << ec.message() << endl;
        SET_QUIT;
    }


    cout << "Send 200 OK for CONNECT" << endl;

    generateCertForDomain(this->currentRequest->urlInfo.hostname.c_str());

    _ctx.use_certificate_chain_file("certs/" + this->currentRequest->urlInfo.hostname + ".pem");
    _ctx.use_private_key_file("certs/server.key", boost::asio::ssl::context::pem);
    _ctx.use_tmp_dh_file("certs/dh512.pem");

    try {
        _in_sslSock = new ssl::stream<tcp::socket &>(_in_socket, _ctx);
    } catch (exception &e) {
        cout << "upgradeToSSL: " << e.what() << endl;
        SET_QUIT;
    }
    cout << "Trying to handshake with client" << endl;
    _in_sslSock->async_handshake(ssl::stream<tcp::socket>::server, yield[ec]);


    if (ec) {
        cout << "upgradeToSSL: " << ec.message() << endl;
        SET_QUIT;
    }

    isSSL = true;
}




