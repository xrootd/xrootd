#include "Decision.hh"

#include "XrdSys/XrdSysError.hh"

#include <vector>
#include <sstream>

#include <iostream>
#include <stdio.h>
#include "XrdSys/XrdSysError.hh"
/*
   A plugin which excludes paths whcih are given in configuration
   parameters.
 */

class PathsDecision : public XrdFileCache::Decision
{

public:
    PathsDecision(XrdSysError & log) : m_log(log)
    {
    }

    virtual
    ~PathsDecision() {}

    virtual bool
    Decide(std::string &url, XrdOss &) const
    {
        size_t split_loc = url.rfind("//");
        split_loc += 1;
        const char* path = &url[split_loc];

        if (split_loc == url.npos)
            return true;

        for(std::vector<std::string>::const_iterator i=m_excludes.begin(); i != m_excludes.end(); ++i)
        {
            if (strncmp(i->c_str(), path, i->size()) == 0)
            {
                // m_log.Emsg( "Rejecting ", "url ", url.c_str());
                return false;
            }
        }
        return true;
    }

    virtual bool
    ConfigDecision(const char* parameters)
    {
        std::istringstream is(parameters);
        std::string part;
        while (getline(is, part, ' '))
        {
            m_excludes.push_back(part);
            m_log.Emsg( "Exclude ", "paths ", part.c_str());
        }

        return !m_excludes.empty();
    }
   
private:
    std::vector<std::string> m_excludes;
    XrdSysError& m_log;

};

/******************************************************************************/
/*                          XrdFileCacheGetDecision                           */
/******************************************************************************/

// Return a decision object to use.
extern "C"
{
XrdFileCache::Decision *
XrdFileCacheGetDecision(XrdSysError & log)
{
    return new PathsDecision(log);
}
}

