//
// Created by zixiong on 11/28/17.
//

#ifndef PROXY_HTTPBODY_H
#define PROXY_HTTPBODY_H

#include <vector>
#include <deque>
#include <list>
#include <boost/asio.hpp>

using namespace std;


class HttpBody {
public:
    void push(vector<char>& input);

    void fin();

    void pull(boost::asio::streambuf &buf);

    void drain(boost::asio::streambuf &buf);

    deque<char>& getBuffer() {
        return buffer;
    }

    void setLogFile(fstream& fs) {
        logFile = &fs;
    }
private:
    deque<char> buffer;
    list<size_t> blockSizes;
    void outputChunk(deque<char>::iterator begin, deque<char>::iterator end, boost::asio::streambuf &buf);

    fstream* logFile = nullptr;

    bool sentTerminator = false;
};


#endif //PROXY_HTTPBODY_H
