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

#include "XrdMon/XrdMonAPException.hh"
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
      _fd(-1),
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
        throw XrdMonAPException(ERR_INVDICTSTRING, es);
    }
    _user = buf;

    x2 += x1+1;
    x1 = doOne(s+x2, buf, len-x2, ':');
    if ( x1 == -1 ) {
        delete [] buf;
        string es("Cannot find "); es+=':'; es+=" in "; es+=s;
        throw XrdMonAPException(ERR_INVDICTSTRING, es);
    }
    _pid = atoi(buf);

    x2 += x1+1;
    x1 = doOne(s+x2, buf, len-x2, '@');
    if ( x1 == -1 ) {
        delete [] buf;
        string es("Cannot find "); es+='@'; es+=" in "; es+=s;
        throw XrdMonAPException(ERR_INVDICTSTRING, es);
    }
    _fd = atoi(buf);

    x2 += x1+1;
    x1 = doOne(s+x2, buf, len-x2, '\n');
    if ( x1 == -1 ) {
        delete [] buf;
        string es("Cannot find "); es+='\n'; es+=" in "; es+=s;
        throw XrdMonAPException(ERR_INVDICTSTRING, es);
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
    int16_t v16;
    int32_t v32;
    
    memcpy(&v32, buf+pos, sizeof(int32_t));
    pos += sizeof(int32_t);
    _myXrdId = ntohl(v32);

    memcpy(&v32, buf+pos, sizeof(int32_t));
    pos += sizeof(int32_t);
    _myUniqueId = ntohl(v32);

    memcpy(&v16, buf+pos, sizeof(int16_t));
    pos += sizeof(int16_t);
    int16_t userSize = ntohs(v16);
    memcpy(b, buf+pos, userSize);
    pos += userSize;
    *(b+userSize) = '\0';
    _user = b;
    
    memcpy(&v16, buf+pos, sizeof(int16_t));
    pos += sizeof(int16_t);
    _pid = ntohs(v16);

    memcpy(&v16, buf+pos, sizeof(int16_t));
    pos += sizeof(int16_t);
    _fd = ntohs(v16);

    memcpy(&v16, buf+pos, sizeof(int16_t));
    pos += sizeof(int16_t);
    int16_t hostSize = ntohs(v16);
    memcpy(b, buf+pos, hostSize);
    pos += hostSize;
    *(b+hostSize) = '\0';
    _host = b;
    
    memcpy(&v16, buf+pos, sizeof(int16_t));
    pos += sizeof(int16_t);
    int16_t pathSize = ntohs(v16);
    memcpy(b, buf+pos, pathSize);
    pos += pathSize;
    *(b+pathSize) = '\0';
    _path = b;

    memcpy(&v32, buf+pos, sizeof(int32_t));
    pos += sizeof(int32_t);
    _open = ntohl(v32);

    memcpy(&v32, buf+pos, sizeof(int32_t));
    pos += sizeof(int32_t);
    _close = ntohl(v32);

    memcpy(&v32, buf+pos, sizeof(int32_t));
    pos += sizeof(int32_t);
    _noTraces = ntohl(v32);

    int64_t v64;
    memcpy(&v64, buf+pos, sizeof(int64_t));
    pos += sizeof(int64_t);
    _noRBytes = ntohll(v64);

    memcpy(&v64, buf+pos, sizeof(int64_t));
    pos += sizeof(int64_t);
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
XrdMonDecDictInfo::closeFile(int64_t bytesR, int64_t bytesW, time_t t)
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
    return sizeof(int32_t) +                // _myXrdId
           sizeof(int32_t) +                // _myUniqueId
           sizeof(int16_t) + _user.size() + // _user
           sizeof(int16_t) +                // _pid
           sizeof(int16_t) +                // _fd
           sizeof(int16_t) + _host.size() + // _host
           sizeof(int16_t) + _path.size() + // _path
           sizeof(time_t)  +                // _open
           sizeof(time_t)  +                // _close
           sizeof(int32_t) +                // _noTraces
           sizeof(int64_t) +                // _noRBytes
           sizeof(int64_t);                 // _noWBytes
}

void
XrdMonDecDictInfo::writeSelf2buf(char* buf, int& pos) const
{
    int32_t v32 = htonl(_myXrdId);
    memcpy(buf+pos, &v32, sizeof(int32_t));
    pos += sizeof(int32_t);

    v32 = htonl(_myUniqueId);
    memcpy(buf+pos, &v32, sizeof(int32_t));
    pos += sizeof(int32_t);

    int16_t v16 = htons(_user.size());
    memcpy(buf+pos, &v16, sizeof(int16_t));
    pos += sizeof(int16_t);    
    memcpy(buf+pos, _user.c_str(), _user.size());
    pos += _user.size();

    v16 = htons(_pid);
    memcpy(buf+pos, &v16, sizeof(int16_t));
    pos += sizeof(int16_t);
    
    v16 = htons(_fd);
    memcpy(buf+pos, &v16, sizeof(int16_t));
    pos += sizeof(int16_t);

    v16 = htons(_host.size());
    memcpy(buf+pos, &v16, sizeof(int16_t));
    pos += sizeof(int16_t);
    memcpy(buf+pos, _host.c_str(), _host.size());
    pos += _host.size();
    
    v16 = htons(_path.size());
    memcpy(buf+pos, &v16, sizeof(int16_t));
    pos += sizeof(int16_t);
    memcpy(buf+pos, _path.c_str(), _path.size());
    pos += _path.size();
    
    v32 = htonl(_open);
    memcpy(buf+pos, &v32, sizeof(int32_t));
    pos += sizeof(int32_t);
    
    v32 = htonl(_close);
    memcpy(buf+pos, &v32, sizeof(int32_t));
    pos += sizeof(int32_t);

    v32 = htonl(_noTraces);
    memcpy(buf+pos, &v32, sizeof(int32_t));
    pos += sizeof(int32_t);

    int64_t v64 = htonll(_noRBytes);
    memcpy(buf+pos, &v64, sizeof(int64_t));
    pos += sizeof(int64_t);

    v64 = htonll(_noWBytes);
    memcpy(buf+pos, &v64, sizeof(int64_t));
    pos += sizeof(int64_t);

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
       << '\t' << _fd
       << '\t' << _host
       << '\t' << _path
       << '\t' << timestamp2string(_open)
       << '\t' << timestamp2string(_close)
       << '\t' << _noTraces
       << '\t' << _noRBytes
       << '\t' << _noWBytes;
    return ss.str();
}

ostream& 
operator<<(ostream& o, const XrdMonDecDictInfo& m)
{
   o << ' ' << m._myXrdId
     << ' ' << m._myUniqueId
     << ' ' << m._user
     << ' ' << m._pid
     << ' ' << m._fd
     << ' ' << m._host
     << ' ' << m._path
     << ' ' << timestamp2string(m._open)
     << ' ' << timestamp2string(m._close)
     << ' ' << m._noTraces
     << ' ' << m._noRBytes
     << ' ' << m._noWBytes;
      
    return o;
}
