//
// Created by zixiong on 11/24/17.
//

#ifndef PROXY_REQUEST_H
#define PROXY_REQUEST_H

#include <string>
#include <vector>
#include <boost/asio.hpp>
#include <deque>
#include <fstream>
#include <unordered_map>
#include <mutex>

#include "rewriter.h"
#include "httpBody.h"


using namespace std;

class Connection;

struct UrlInfo {
    string protocol;
    string hostname;
    string port;
    string query;
};

struct Request {
    string method;
    string url;
    string protocol;

    int responseCode;
    string reasonPharse;

    UrlInfo urlInfo;

    Request(Connection* connection, string sourceIP);

    vector<pair<string, string>> requestHeaders;
    vector<pair<string, string>> responseHeaders;

    void parseRequestHeader(boost::asio::streambuf &s);

    void writeOutGoingHeader(boost::asio::streambuf& buf);


    void parseResponseHeader(boost::asio::streambuf &s);

    void writeInComingHeader(boost::asio::streambuf& buf);

    /* for body inspection */

    void pushResponseBody(vector<char>& buf);

    void pullResponseBody(bool fin, boost::asio::streambuf& buf);

    void pushRequestBody(vector<char>& buf);

    void pullRequestBody(bool fin, boost::asio::streambuf& buf);

    static void setLogPath(string path);

    ~Request();
private:

    static unordered_map<string, int> logFileTracker;
    static mutex logFileTrackerLock;

    static string logPath;



    void createLogFile();

    HttpBody requestBody;
    HttpBody responseBody;

    Connection* _connection;
    string _sourceIP;

    fstream logFile;

    void parseUrl();
    void parseHeaderFields(boost::asio::streambuf& s, vector<pair<string, string>>& headers);

    void writeHeaderFields(boost::asio::streambuf& buf, vector<pair<string, string>> &headers);

    /* Rewriting related stuff */
    vector<Rewriter*> rewriters;



};


#endif //PROXY_REQUEST_H
