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
#include <stdio.h>
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
      _host("InvalidHost"),
      _sec(0),
      _dTime(0)
{}

XrdMonDecUserInfo::XrdMonDecUserInfo(dictid_t id,
                                     dictid_t uniqueId,
                                     const char* s, 
                                     int len)
    : _myXrdId(id),
      _myUniqueId(uniqueId),
      _sec(0),
      _dTime(0)
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
    memcpy(buf, s+x2, len-x2);
    *(buf+len-x2) = '\0';
    _host = buf;

    delete [] buf;
}

void
XrdMonDecUserInfo::setDisconnectInfo(kXR_int32 sec,
                                     kXR_int32 timestamp)
{
    _sec   = sec;
    _dTime = timestamp;
}

// this goes to ascii file loaded to MySQL
string
XrdMonDecUserInfo::convert2string() const
{
    stringstream ss(stringstream::out);
    ss <<         _user
       << '\t' << _pid
       << '\t' << _host
       << '\t' << _sec
       << '\t' << timestamp2string(_dTime);
    return ss.str();
}

// this goes to real time log file
const char*
XrdMonDecUserInfo::writeRT2Buffer(TYPE t, string& senderHost) const
{
    static char buf[512];
    
    if ( t == CONNECT ) {
        sprintf(buf, "u\t%i\t%s\t%i\t%s\t%s\n", 
                _myUniqueId, _user.c_str(), _pid, _host.c_str(), 
                senderHost.c_str());
    } else {
        static char b[24];
        timestamp2string(_dTime, b);
        sprintf(buf, "d\t%i\t%i\t%s\n", _myUniqueId, _sec, b);
    }
    return buf;
}

// this is for debugging
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
