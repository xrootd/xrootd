
/*****************************************************************************/
/*                                                                           */
/*                           XrdMonDecDictInfo.cc                            */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonException.hh"
#include "XrdMon/XrdMonDecDictInfo.hh"
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

XrdMonDecDictInfo::XrdMonDecDictInfo()
    : _myXrdId(0),
      _myUniqueId(0),
      _user("InvalidUser"),
      _pid(-1),
      _host("InvalidHost"),
      _path("InvalidPath"),
      _open(0),
      _close(0),
      _noTraces(0),
      _noRBytes(0),
      _noWBytes(0)
{}

XrdMonDecDictInfo::XrdMonDecDictInfo(dictid_t id,
                                     dictid_t uniqueId,
                                     const char* s, 
                                     int len)
    : _myXrdId(id),
      _myUniqueId(uniqueId),
      _open(0),
      _close(0),
      _noTraces(0),
      _noRBytes(0),
      _noWBytes(0)
{
    // uncomment all 3 below if you want to print the string
    //char*b = new char [len+1];strncpy(b, s, len);b[len] = '\0';
    //cout << "Decoding string " << b << endl;
    //delete [] b;
    
    // decode theString, format: <user>.<pid>:<fd>@<host>\n<path>
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
    x1 = doOne(s+x2, buf, len-x2, '\n');
    if ( x1 == -1 ) {
        delete [] buf;
        string es("Cannot find "); es+='\n'; es+=" in "; es+=s;
        throw XrdMonException(ERR_INVDICTSTRING, es);
    }
    _host = buf;

    x2 += x1+1;
    memcpy(buf, s+x2, len-x2);
    *(buf+len-x2) = '\0';
    _path = buf;

    delete [] buf;
}

// initialize self from a buffer. The buffer is a dump of the jnl file
XrdMonDecDictInfo::XrdMonDecDictInfo(const char* buf, int& pos)
{
    char b[256];
    kXR_int16 v16;
    kXR_int32 v32;
    
    memcpy(&v32, buf+pos, sizeof(kXR_int32));
    pos += sizeof(kXR_int32);
    _myXrdId = ntohl(v32);

    memcpy(&v32, buf+pos, sizeof(kXR_int32));
    pos += sizeof(kXR_int32);
    _myUniqueId = ntohl(v32);

    memcpy(&v16, buf+pos, sizeof(kXR_int16));
    pos += sizeof(kXR_int16);
    kXR_int16 userSize = ntohs(v16);
    memcpy(b, buf+pos, userSize);
    pos += userSize;
    *(b+userSize) = '\0';
    _user = b;
    
    memcpy(&v16, buf+pos, sizeof(kXR_int16));
    pos += sizeof(kXR_int16);
    _pid = ntohs(v16);

    memcpy(&v16, buf+pos, sizeof(kXR_int16));
    pos += sizeof(kXR_int16);
    kXR_int16 hostSize = ntohs(v16);
    memcpy(b, buf+pos, hostSize);
    pos += hostSize;
    *(b+hostSize) = '\0';
    _host = b;
    
    memcpy(&v16, buf+pos, sizeof(kXR_int16));
    pos += sizeof(kXR_int16);
    kXR_int16 pathSize = ntohs(v16);
    memcpy(b, buf+pos, pathSize);
    pos += pathSize;
    *(b+pathSize) = '\0';
    _path = b;

    memcpy(&v32, buf+pos, sizeof(kXR_int32));
    pos += sizeof(kXR_int32);
    _open = ntohl(v32);

    memcpy(&v32, buf+pos, sizeof(kXR_int32));
    pos += sizeof(kXR_int32);
    _close = ntohl(v32);

    memcpy(&v32, buf+pos, sizeof(kXR_int32));
    pos += sizeof(kXR_int32);
    _noTraces = ntohl(v32);

    kXR_int64 v64;
    memcpy(&v64, buf+pos, sizeof(kXR_int64));
    pos += sizeof(kXR_int64);
    _noRBytes = ntohll(v64);

    memcpy(&v64, buf+pos, sizeof(kXR_int64));
    pos += sizeof(kXR_int64);
    _noWBytes = ntohll(v64);

    cout << "JB " << *this << endl;
}

void
XrdMonDecDictInfo::openFile(time_t t)
{
    if ( 0 == _open ) {
        _open = t;
    } else {
        time_t useThis = t<_open? t : _open;
        cerr << "ERROR: Multiple attempts to open file "
             << *this << ", passed timestamp is "
             << timestamp2string(t) << ", will use " 
             << timestamp2string(useThis) << endl;
        _open = useThis;
    }
}

void
XrdMonDecDictInfo::closeFile(kXR_int64 bytesR, kXR_int64 bytesW, time_t t)
{
    if ( 0 == _close ) {
        _close = t;
    } else {
        time_t useThis = t>_close? t : _close;
        cerr << "ERROR: Multiple attempts to close file "
             << *this << ", passed timestamp is "
             << timestamp2string(t) << ", will use " 
             << timestamp2string(useThis) << endl;
        _close = useThis;
    }
    _noRBytes = bytesR;
    _noWBytes = bytesW;
}

// returns true if trace is ok
bool
XrdMonDecDictInfo::addTrace(const XrdMonDecTraceInfo& trace)
{
    if ( 0 == _open ) {
        cerr << "ERROR: attempt to add trace for not-open file. "
             << "(dictId = " << _myXrdId << ", uniqueId = " << _myUniqueId << "). "
             << "Will use timestamp of the trace ("
             << timestamp2string(trace.timestamp())
             << ") as openfile timestamp." << endl;
        _open = trace.timestamp();
    }
    if ( 0 != _close ) {
        cerr << "ERROR: attempt to add trace for closed file. "
             << "Will ignore this trace: " << trace << endl;
        return false;
    }
    
    ++_noTraces;
    
    if ( trace.isRead() ) {
        _noRBytes += trace.length();
    } else {
        _noWBytes += trace.length();
    }
    return true;
}

int 
XrdMonDecDictInfo::stringSize() const
{
    return sizeof(kXR_int32) +                // _myXrdId
           sizeof(kXR_int32) +                // _myUniqueId
           sizeof(kXR_int16) + _user.size() + // _user
           sizeof(kXR_int16) +                // _pid
           sizeof(kXR_int16) + _host.size() + // _host
           sizeof(kXR_int16) + _path.size() + // _path
           sizeof(time_t)  +                // _open
           sizeof(time_t)  +                // _close
           sizeof(kXR_int32) +                // _noTraces
           sizeof(kXR_int64) +                // _noRBytes
           sizeof(kXR_int64);                 // _noWBytes
}

void
XrdMonDecDictInfo::writeSelf2buf(char* buf, int& pos) const
{
    kXR_int32 v32 = htonl(_myXrdId);
    memcpy(buf+pos, &v32, sizeof(kXR_int32));
    pos += sizeof(kXR_int32);

    v32 = htonl(_myUniqueId);
    memcpy(buf+pos, &v32, sizeof(kXR_int32));
    pos += sizeof(kXR_int32);

    kXR_int16 v16 = htons(_user.size());
    memcpy(buf+pos, &v16, sizeof(kXR_int16));
    pos += sizeof(kXR_int16);    
    memcpy(buf+pos, _user.c_str(), _user.size());
    pos += _user.size();

    v16 = htons(_pid);
    memcpy(buf+pos, &v16, sizeof(kXR_int16));
    pos += sizeof(kXR_int16);
    
    v16 = htons(_host.size());
    memcpy(buf+pos, &v16, sizeof(kXR_int16));
    pos += sizeof(kXR_int16);
    memcpy(buf+pos, _host.c_str(), _host.size());
    pos += _host.size();
    
    v16 = htons(_path.size());
    memcpy(buf+pos, &v16, sizeof(kXR_int16));
    pos += sizeof(kXR_int16);
    memcpy(buf+pos, _path.c_str(), _path.size());
    pos += _path.size();
    
    v32 = htonl(_open);
    memcpy(buf+pos, &v32, sizeof(kXR_int32));
    pos += sizeof(kXR_int32);
    
    v32 = htonl(_close);
    memcpy(buf+pos, &v32, sizeof(kXR_int32));
    pos += sizeof(kXR_int32);

    v32 = htonl(_noTraces);
    memcpy(buf+pos, &v32, sizeof(kXR_int32));
    pos += sizeof(kXR_int32);

    kXR_int64 v64 = htonll(_noRBytes);
    memcpy(buf+pos, &v64, sizeof(kXR_int64));
    pos += sizeof(kXR_int64);

    v64 = htonll(_noWBytes);
    memcpy(buf+pos, &v64, sizeof(kXR_int64));
    pos += sizeof(kXR_int64);

    cout<< "JB " << *this << endl;
}

// this goes to ascii file loaded to MySQL
string
XrdMonDecDictInfo::convert2string() const
{
    stringstream ss(stringstream::out);
    ss << _myUniqueId 
       << '\t' << _user
       << '\t' << _pid
       << '\t' << _host
       << '\t' << _path
       << '\t' << timestamp2string(_open)
       << '\t' << timestamp2string(_close)
       << '\t' << _noTraces
       << '\t' << _noRBytes
       << '\t' << _noWBytes;
    return ss.str();
}

// this goes to real time log file
string
XrdMonDecDictInfo::convert2stringRT() const
{
    stringstream ss(stringstream::out);
    ss << _myUniqueId 
       << ' ' << _user
       << ' ' << _host
       << ' ' << _path
       << ' ' << timestamp2string(_open);
    return ss.str();
}

ostream& 
operator<<(ostream& o, const XrdMonDecDictInfo& m)
{
   o << ' ' << m._myXrdId
     << ' ' << m._myUniqueId
     << ' ' << m._user
     << ' ' << m._pid
     << ' ' << m._host
     << ' ' << m._path
     << ' ' << timestamp2string(m._open)
     << ' ' << timestamp2string(m._close)
     << ' ' << m._noTraces
     << ' ' << m._noRBytes
     << ' ' << m._noWBytes;
      
    return o;
}
