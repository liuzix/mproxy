//
// Created by zixiong on 11/24/17.
//

#include "request.h"

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <exception>
#include <regex>
#include <boost/lexical_cast.hpp>

#include "connection.h"

using namespace std;

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

    parseUrl();
}

void Request::parseUrl() {
    regex urlRegex(R"((.+?):\/\/(.+?)(?::(\d+?))?\/(.*))");
    smatch match;
    if (regex_search(url, match, urlRegex)) {
        urlInfo.protocol = match[1];
        urlInfo.hostname = match[2];
        urlInfo.port = match[3];
        urlInfo.query = match[4];
    } else {
        throw runtime_error("Bad Request URL");
    }

    if (urlInfo.protocol != "http" && urlInfo.protocol != "https") {
        throw runtime_error("Unsupported Protocol");
    }

}

void Request::parseHeaderFields(boost::asio::streambuf &s, vector<pair<string, string>>& headers) {
    istream is(&s);
    string line;

    while (!is.eof()) {
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

Request::Request(Connection *connection) : _connection(connection){

}

void Request::writeHeaderFields(boost::asio::streambuf& buf, vector<pair<string, string>> &headers) {
    ostream os(&buf);

    for(auto& kv: headers) {
        os << kv.first << ": " << kv.second << "\r\n";
    }

    os << "\r\n";

}

void Request::writeOutGoingHeader(boost::asio::streambuf &buf) {
    ostream os(&buf);

    os << method << " " << "/" << urlInfo.query << " " << protocol << "\r\n";

    writeHeaderFields(buf, requestHeaders);
}

void Request::parseResponseHeader(boost::asio::streambuf &s) {
    istream is(&s);
    string line;

    getline(is, line);
    boost::trim(line);

    // start parsing status line
    vector<string> splitResult;
    boost::split(splitResult, line, boost::is_space());

    if (splitResult.size() != 3)
        throw runtime_error("Bad Status Line");

    protocol = std::move(splitResult[0]);
    responseCode = boost::lexical_cast<int>(splitResult[1]);
    reasonPharse = std::move(splitResult[2]);

    parseHeaderFields(s, responseHeaders);

}

void Request::writeInComingHeader(boost::asio::streambuf &buf) {
    ostream os(&buf);
    os << protocol << " " << responseCode << " " << reasonPharse << "\r\n";
    writeHeaderFields(buf, responseHeaders);
}
