//
// Created by zixiong on 11/28/17.
//

#include "httpBody.h"

#include <fstream>

void HttpBody::push(vector<char> &input) {
    buffer.insert(buffer.end(), input.begin(), input.end());
    blockSizes.push_back(input.size());
}

void HttpBody::fin() {
    blockSizes.push_back(0);
}

void HttpBody::pull(boost::asio::streambuf &buf) {
    size_t lastSize = blockSizes.front();
    blockSizes.pop_front();
    if (blockSizes.empty())
        return; // nothing to do

    size_t outChunkSize = min(lastSize, buffer.size() / 2);

    outputChunk(buffer.begin(), buffer.begin() + outChunkSize, buf);

    if (outChunkSize == 0)
        sentTerminator = true;

}

void HttpBody::outputChunk(deque<char>::iterator begin, deque<char>::iterator end, boost::asio::streambuf &buf) {
    ostream os(&buf);
    ostream_iterator<char> outIt(os);
    ostream_iterator<char> outItLog(*logFile);

    os << std::hex << std::distance(begin, end) << "\r\n";

    std::copy(begin, end, outIt);
    std::copy(begin, end, outItLog);

    os << "\r\n";

    buffer.erase(begin, end);
}

void HttpBody::drain(boost::asio::streambuf &buf) {
    outputChunk(buffer.begin(), buffer.end(), buf);
    blockSizes.clear();

    if (sentTerminator)
        return;



    ostream os(&buf);
    os << "0\r\n\r\n";
}
