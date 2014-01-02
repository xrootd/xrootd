
#include "Decision.hh"

#include "XrdSys/XrdSysError.hh"

/*
   The simplest example of a decision plugin - always allow the file
   to be fetched to the cache.
 */

class AllowDecision : public XrdFileCache::Decision
{

public:

    virtual bool
    Decide(std::string &, XrdOss &) const {return true; }

};

/******************************************************************************/
/*                          XrdFileCacheGetDecision                           */
/******************************************************************************/

// Return a decision object to use.
extern "C"
{
XrdFileCache::Decision *
XrdFileCacheGetDecision(XrdSysError &)
{
    return new AllowDecision();
}
}

