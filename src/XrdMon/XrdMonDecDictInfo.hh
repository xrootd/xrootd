/*****************************************************************************/
/*                                                                           */
/*                           XrdMonDecDictInfo.hh                            */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONDECDICTINFO_HH
#define XRDMONDECDICTINFO_HH

#include "XrdMon/XrdMonCommon.hh"
#include "XrdMon/XrdMonTypes.hh"
#include <iostream>
#include <string>
#include <strings.h>

using std::ostream;
using std::string;

class XrdMonDecTraceInfo;

class XrdMonDecDictInfo {
public:
    XrdMonDecDictInfo();
    XrdMonDecDictInfo(dictid_t id,
                      dictid_t uniqueId,
                      const char* theString,
                      int len);
    XrdMonDecDictInfo(const char* buf, int& pos);
    
    dictid_t xrdId() const { return _myXrdId; }
    dictid_t uniqueId() const { return _myUniqueId; }
    
    bool isClosed() const   { return 0 != _close; }
    int stringSize() const;
    string convert2string() const;
    void writeSelf2buf(char* buf, int& pos) const;
    
    void openFile(time_t t);
    void closeFile(int64_t bytesR, int64_t bytesW, time_t t);
    bool addTrace(const XrdMonDecTraceInfo& trace);

private:
    int doOne(const char* s, char* buf, int len, char delim) {
        int x = 0;
        while ( x < len && *(s+x) != delim ) {
            ++x;
        }
        if ( x >= len ) {
            return -1;
        }
        
        memcpy(buf, s, x);
        *(buf+x) = '\0';
        return x;
    }

    dictid_t _myXrdId;     // the one that come inside packet, not unique
    dictid_t _myUniqueId; // unique (across all dictIds for given xrd server)

    string  _user;
    int16_t _pid;
    int16_t _fd;
    string  _host;
    string  _path;
    time_t  _open;
    time_t  _close;
    
    int32_t _noTraces;
    int64_t _noRBytes;  // no bytes read
    int64_t _noWBytes;  // no bytes writen
    
    friend ostream& operator<<(ostream& o, 
                               const XrdMonDecDictInfo& m);
};

#endif /* XRDMONDECDICTINFO_HH */
