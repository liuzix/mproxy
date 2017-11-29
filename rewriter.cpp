//
// Created by zixiong on 11/27/17.
//

#include "rewriter.h"
#include "request.h"

static vector<Rewriter*> rewriterList;

void initRewriters() {
    rewriterList.push_back(new QueryRewriter("www.tumblr.com", R"(search\/(.+))", R"(search/ghost)"));
}

void getRewritersByHostName(const string& hostname, vector<Rewriter*>& container) {
    for (Rewriter* rw: rewriterList) {
        if (rw->matchHostName(hostname)) {
            container.push_back(rw);
        }
    }
}

QueryRewriter::QueryRewriter(const string& hostName, const string& regex, const string& replaceWith) :
    regex(regex),
    _replaceWith(replaceWith)
{
    hostNameList.push_back(hostName);
}

void QueryRewriter::rewriteQuery(UrlInfo &urlInfo) {
    urlInfo.query = boost::regex_replace(urlInfo.query, regex, _replaceWith);
}
