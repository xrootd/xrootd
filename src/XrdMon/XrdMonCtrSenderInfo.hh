/*****************************************************************************/
/*                                                                           */
/*                          XrdMonCtrSenderInfo.hh                           */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONCTRSENDERINFO_HH
#define XRDMONCTRSENDERINFO_HH

#include "XrdMon/XrdMonTypes.hh"
#include <netinet/in.h>
#include <map>
#include <vector>
using std::map;
using std::vector;

class XrdMonCtrSenderInfo {
public:
    static int convert2Id(struct sockaddr_in sAddr);
    static const char* hostPort(struct sockaddr_in sAddr) {
        return hostPort(convert2Id(sAddr));
    }
    static const char* hostPort(uint16_t id) {
        if ( id >= _hps.size() ) {
            return "Error: invalid offset!";
        }
        return _hps[id];
    }
    static void destructStatics();
    
private:
    static char* buildName(struct sockaddr_in sAddr);

private:
    // Maps hash of sockaddr_in --> id.
    // Used as offset in various vectors
    static map<uint64_t, uint16_t> _ids;
    static vector<char*>           _hps; // <host>:<port>
};

#endif /* XRDMONCTRSENDERINFO_HH */
