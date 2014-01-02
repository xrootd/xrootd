#include <string>
#include <iostream>
#include <stdio.h>
#include "XrdOss/XrdOss.hh"

class XrdSysError;

namespace XrdFileCache
{

class Decision {

public:
    virtual bool Decide(std::string &, XrdOss &) const = 0;
    virtual ~Decision() {}

    virtual bool
    ConfigDecision(const char*) { return true; }
};

}

