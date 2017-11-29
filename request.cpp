//
// Created by zixiong on 11/24/17.
//

#include "request.h"

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <exception>
#include <regex>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>


#include "connection.h"

using namespace std;

unordered_map<string, int> Request::logFileTracker;
mutex Request::logFileTrackerLock;
string Request::logPath;

void Request::parseRequestHeader(boost::asio::streambuf &s) {
    istream is(&s);
    string line;

    getline(is, line);
    boost::trim(line);

    // start parsing status line
    vector<string> splitResult;
    boost::split(splitResult, line, boost::is_space());

    if (splitResult.size() != 3)
        throw runtime_error("Bad Status Line");

    method = std::move(splitResult[0]);
    url = std::move(splitResult[1]);
    protocol = std::move(splitResult[2]);



    parseHeaderFields(s, requestHeaders);

    for (auto& kv: requestHeaders) {
        if (kv.first == "Accept-Encoding")
            kv.second = "gzip;q=0,deflate;q=0";

        /* tentative support of keep-alive */
        //if (kv.first == "Connection")
        //    kv.second = "close";

        if (kv.first == "Host") {
            string hostPort = kv.second;
            const auto index = hostPort.find_first_of(':');
            if (string::npos == index) {
                urlInfo.hostname = hostPort;
            } else {
                urlInfo.hostname = hostPort.substr(0, index);
                urlInfo.port = hostPort.substr(index + 1);
            }
        }
    }

    parseUrl();

    // create the log file
    createLogFile();


}

void Request::parseUrl() {

    if (method == "CONNECT") {
        urlInfo.protocol = "https";

        const auto index = url.find_first_of(':');
        if (string::npos == index) {
            throw runtime_error("Bad CONNECT Line");
        }
        urlInfo.hostname = url.substr(0, index);
        urlInfo.port = url.substr(index + 1);

        return;

    }


    boost::regex urlRegex(R"((http):\/\/(.+?)(?::(\d+?))?\/(.*))");
    boost::smatch match;
    if (boost::regex_match(url, match, urlRegex)) {
        urlInfo.protocol = match[1];
        urlInfo.hostname = match[2];
        urlInfo.port = match[3];
        urlInfo.query = match[4];
    } else {
        if (!url.empty() && url[0] == '/') {
            urlInfo.query = url.substr(1);
            urlInfo.protocol = "https"; // this line has no use
        } else {
            throw runtime_error("Bad Request URL");
        }
    }

    if (urlInfo.protocol != "http" && urlInfo.protocol != "https") {
        throw runtime_error("Unsupported Protocol");
    }

    getRewritersByHostName(urlInfo.hostname, this->rewriters);

}

void Request::parseHeaderFields(boost::asio::streambuf &s, vector<pair<string, string>>& headers) {
    headers.clear();

    istream is(&s);
    string line;

    while (true) {
        getline(is, line);
        boost::trim(line);
        if (line.length() == 0) {
            break;
        }

        string key;
        string value;

        const auto index = line.find_first_of(':');
        if (string::npos == index) {
            throw runtime_error("Bad Header Line");
        }

        key = line.substr(0, index);
        value = line.substr(index + 1);

        boost::trim(key);
        boost::trim(value);

        headers.emplace_back(key, value);
    }
}

Request::Request(Connection *connection, string sourceIP) : _connection(connection), _sourceIP(std::move(sourceIP)){

}

void Request::writeHeaderFields(boost::asio::streambuf& buf, vector<pair<string, string>> &headers) {
    ostream os(&buf);

    for(auto& kv: headers) {
        os << kv.first << ": " << kv.second << "\r\n";
        logFile << kv.first << ": " << kv.second << "\r\n";
    }

    os << "\r\n";
    logFile << "\r\n";

}

void Request::writeOutGoingHeader(boost::asio::streambuf &buf) {
    for (Rewriter* rw: rewriters) {
        rw->rewriteQuery(urlInfo);
    }
    ostream os(&buf);

    os << method << " " << "/" << urlInfo.query << " " << protocol << "\r\n";
    logFile << method << " " << "/" << urlInfo.query << " " << protocol << "\r\n";

    writeHeaderFields(buf, requestHeaders);
}

void Request::parseResponseHeader(boost::asio::streambuf &s) {
    istream is(&s);
    string line;

    is >> protocol;
    is >> responseCode;
    if (responseCode > 1000)
        throw runtime_error("Bad response code");
    getline(is, reasonPharse);
    boost::trim(reasonPharse);

    parseHeaderFields(s, responseHeaders);



}

void Request::writeInComingHeader(boost::asio::streambuf &buf) {
    ostream os(&buf);
    os << protocol << " " << responseCode << " " << reasonPharse << "\r\n";
    logFile << protocol << " " << responseCode << " " << reasonPharse << "\r\n";
    writeHeaderFields(buf, responseHeaders);
}

void Request::pushResponseBody(vector<char> &buf) {
    if (!buf.empty())
        responseBody.push(buf);
    else
        responseBody.fin();
}

void Request::pullResponseBody(bool fin, boost::asio::streambuf &buf) {
    for (auto rw: rewriters)
        rw->rewriteResponseData(responseBody.getBuffer());

    if (fin)
        responseBody.drain(buf);
    else
        responseBody.pull(buf);
}

void Request::pushRequestBody(vector<char> &buf) {
    if (!buf.empty())
        requestBody.push(buf);
    else
        requestBody.fin();
}

void Request::pullRequestBody(bool fin, boost::asio::streambuf &buf) {
    for (auto rw: rewriters)
        rw->rewritePostData(requestBody.getBuffer());

    if (fin)
        requestBody.drain(buf);
    else
        requestBody.pull(buf);
}

void Request::createLogFile() {
    if (logFile.is_open())
        return;
    assert(!urlInfo.hostname.empty());
    lock_guard<mutex> guard(logFileTrackerLock);

    string identifier = _sourceIP + "_" + urlInfo.hostname;
    if (logFileTracker.find(identifier) == logFileTracker.end()) {
        logFileTracker[identifier] = 0;
    } else {
        logFileTracker[identifier]++;
    }

    logFile.open(logPath + "/" + to_string(logFileTracker[identifier]) + "_" + identifier,
                 ios_base::in | fstream::out | ios_base::trunc);

    cout << "logfile: " << logPath + "/" + to_string(logFileTracker[identifier]) + "_" + identifier << endl;

    requestBody.setLogFile(logFile);
    responseBody.setLogFile(logFile);
}

void Request::setLogPath(string path) {
    logPath = std::move(path);
}

Request::~Request() {
    logFile.close();
}
