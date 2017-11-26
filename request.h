//
// Created by zixiong on 11/24/17.
//

#ifndef PROXY_REQUEST_H
#define PROXY_REQUEST_H

#include <string>
#include <vector>
#include <boost/asio.hpp>


using namespace std;

class Connection;

struct Request {
    string method;
    string url;
    string protocol;

    int responseCode;
    string reasonPharse;

    struct UrlInfo {
        string protocol;
        string hostname;
        string port;
        string query;
    } urlInfo;

    Request(Connection* connection);

    vector<pair<string, string>> requestHeaders;
    vector<pair<string, string>> responseHeaders;

    void parseRequestHeader(boost::asio::streambuf &s);

    void writeOutGoingHeader(boost::asio::streambuf& buf);


    void parseResponseHeader(boost::asio::streambuf &s);

    void writeInComingHeader(boost::asio::streambuf& buf);
private:
    vector<char> requestBody;
    vector<char> responseBody;

    Connection* _connection;

    void parseUrl();
    void parseHeaderFields(boost::asio::streambuf& s, vector<pair<string, string>>& headers);

    static void writeHeaderFields(boost::asio::streambuf& buf, vector<pair<string, string>> &headers);

};


#endif //PROXY_REQUEST_H
