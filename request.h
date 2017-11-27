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

    Request(Connection* connection, string sourceIP);

    vector<pair<string, string>> requestHeaders;
    vector<pair<string, string>> responseHeaders;

    void parseRequestHeader(boost::asio::streambuf &s);

    void writeOutGoingHeader(boost::asio::streambuf& buf);


    void parseResponseHeader(boost::asio::streambuf &s);

    void writeInComingHeader(boost::asio::streambuf& buf);

    /* for body inspection */

    void pushResponseBody(vector<char>& buf);

    void pullResponseBody(bool chunked, boost::asio::streambuf& buf);

    void pushRequestBody(vector<char>& buf);

    void pullRequestBody(bool chunked, boost::asio::streambuf& buf);

    static void setLogPath(string path);

    ~Request();
private:

    static unordered_map<string, int> logFileTracker;
    static mutex logFileTrackerLock;

    static string logPath;



    void createLogFile();

    deque<char> requestBody;
    deque<char> responseBody;

    Connection* _connection;
    string _sourceIP;

    fstream logFile;

    void parseUrl();
    void parseHeaderFields(boost::asio::streambuf& s, vector<pair<string, string>>& headers);

    void writeHeaderFields(boost::asio::streambuf& buf, vector<pair<string, string>> &headers);

};


#endif //PROXY_REQUEST_H
