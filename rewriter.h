//
// Created by zixiong on 11/27/17.
//

#ifndef PROXY_REWRITER_H
#define PROXY_REWRITER_H

#include <string>
#include <vector>
#include <algorithm>
#include <boost/regex.hpp>

using namespace std;

struct UrlInfo;

/* The base class for rewriting requests or response*/
class Rewriter {
public:
    bool matchHostName(const string& hostName) {
        auto it = std::find(hostNameList.begin(), hostNameList.end(), hostName);
        return (it != hostNameList.end());
    }

    virtual void rewriteQuery(UrlInfo& urlInfo) { }

    virtual void rewritePostData(deque<char>& data) { }

    virtual void rewriteResponseData(deque<char>& data) { }

    virtual bool getHijackRequest(UrlInfo& urlInfo) {
        return false;
    }

protected:
    vector<string> hostNameList;
};

//extern vector<Rewriter*> rewriterList;

void initRewriters();

void getRewritersByHostName(const string& hostname, vector<Rewriter*>& container);

/* Here begins all the custom rewriters */

class QueryRewriter: public Rewriter {
public:
    QueryRewriter(const string& hostName, const string& regex, const string& replaceWith);
    void rewriteQuery(UrlInfo& urlInfo) override;
private:
    boost::regex regex;
    string _replaceWith;

};

#endif //PROXY_REWRITER_H
