/*****************************************************************************/
/*                                                                           */
/*                           XrdMonDecUserInfo.cc                            */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonException.hh"
#include "XrdMon/XrdMonDecUserInfo.hh"
#include "XrdMon/XrdMonErrors.hh"
#include "XrdMon/XrdMonUtils.hh"
#include "XrdMon/XrdMonDecTraceInfo.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include <netinet/in.h>
#include <sstream>
using std::cout;
using std::cerr;
using std::endl;
using std::ostream;
using std::stringstream;

XrdMonDecUserInfo::XrdMonDecUserInfo()
    : _myXrdId(0),
      _myUniqueId(0),
      _user("InvalidUser"),
      _pid(-1),
      _host("InvalidHost")
{}

XrdMonDecUserInfo::XrdMonDecUserInfo(dictid_t id,
                                     dictid_t uniqueId,
                                     const char* s, 
                                     int len)
    : _myXrdId(id),
      _myUniqueId(uniqueId)
{
    // uncomment all 3 below if you want to print the string
    //char*b = new char [len+1];strncpy(b, s, len);b[len] = '\0';
    //cout << "Decoding string " << b << endl;
    //delete [] b;
    
    // decode theString, format: <user>.<pid>:<fd>@<host>
    int x1 = 0, x2 = 0;
    char* buf = new char [len+1];

    x1 = doOne(s, buf, len, '.');
    if (x1 == -1 ) {
        delete [] buf;
        string es("Cannot find "); es+='.'; es+=" in "; es+=s;
        throw XrdMonException(ERR_INVDICTSTRING, es);
    }
    _user = buf;

    x2 += x1+1;
    x1 = doOne(s+x2, buf, len-x2, ':');
    if ( x1 == -1 ) {
        delete [] buf;
        string es("Cannot find "); es+=':'; es+=" in "; es+=s;
        throw XrdMonException(ERR_INVDICTSTRING, es);
    }
    _pid = atoi(buf);

    x2 += x1+1;
    x1 = doOne(s+x2, buf, len-x2, '@');
    if ( x1 == -1 ) {
        delete [] buf;
        string es("Cannot find "); es+='@'; es+=" in "; es+=s;
        throw XrdMonException(ERR_INVDICTSTRING, es);
    }
    //kXR_int16 fd = atoi(buf);

    x2 += x1+1;
    x1 = doOne(s+x2, buf, len-x2, '\0');
    if ( x1 == -1 ) {
        delete [] buf;
        string es("Cannot find "); es+='\n'; es+=" in "; es+=s;
        throw XrdMonException(ERR_INVDICTSTRING, es);
    }
    _host = buf;

    delete [] buf;
}

int 
XrdMonDecUserInfo::stringSize() const
{
    return sizeof(kXR_int32) +                // _myXrdId
           sizeof(kXR_int32) +                // _myUniqueId
           sizeof(kXR_int16) + _user.size() + // _user
           sizeof(kXR_int16) +                // _pid
           sizeof(kXR_int16) + _host.size();  // _host
}

// this goes to ascii file loaded to MySQL
string
XrdMonDecUserInfo::convert2string() const
{
    stringstream ss(stringstream::out);
    ss << _myUniqueId 
       << '\t' << _user
       << '\t' << _pid
       << '\t' << _host;
    return ss.str();
}

// this goes to real time log file
string
XrdMonDecUserInfo::convert2stringRT() const
{
    stringstream ss(stringstream::out);
    ss << ' ' << _user
       << ' ' << _host;
    return ss.str();
}

ostream& 
operator<<(ostream& o, const XrdMonDecUserInfo& m)
{
   o << ' ' << m._myXrdId
     << ' ' << m._myUniqueId
     << ' ' << m._user
     << ' ' << m._pid
     << ' ' << m._host;
      
    return o;
}
