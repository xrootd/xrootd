/*****************************************************************************/
/*                                                                           */
/*                           XrdMonDecUserInfo.hh                            */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONDECUSERINFO_HH
#define XRDMONDECUSERINFO_HH

#include "XrdMon/XrdMonCommon.hh"
#include "XrdMon/XrdMonTypes.hh"
#include <iostream>
#include <string>
#include <strings.h>

using std::ostream;
using std::string;

class XrdMonDecTraceInfo;

class XrdMonDecUserInfo {
public:
    XrdMonDecUserInfo();
    XrdMonDecUserInfo(dictid_t id,
                      dictid_t uniqueId,
                      const char* theString,
                      int len);

    void setDisconnectInfo(kXR_int32 sec, time_t timestamp);
    
    dictid_t xrdId() const { return _myXrdId; }
    dictid_t uniqueId() const { return _myUniqueId; }
    
    int stringSize() const;
    string convert2string() const;
    string convert2stringRT() const;

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

    dictid_t _myXrdId;    // the one that come inside packet, not unique
    dictid_t _myUniqueId; // unique (across all dictIds for given xrd server)

    string    _user;
    kXR_int16 _pid;
    string    _host;

    kXR_int32 _sec;   // number of seconds that client was connected
    time_t    _dTime; // disconnect time
    
    friend ostream& operator<<(ostream& o, 
                               const XrdMonDecUserInfo& m);
};

#endif /* XRDMONDECUSERINFO_HH */
