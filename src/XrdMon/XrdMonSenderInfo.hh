/*****************************************************************************/
/*                                                                           */
/*                            XrdMonSenderInfo.hh                            */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONSENDERINFO_HH
#define XRDMONSENDERINFO_HH

#include "XrdMon/XrdMonTypes.hh"
#include <netinet/in.h>
#include <map>
#include <vector>
using std::map;
using std::vector;

class XrdMonSenderInfo {
public:
    static int convert2Id(struct sockaddr_in sAddr);
    static const char* hostPort(struct sockaddr_in sAddr) {
        return hostPort(convert2Id(sAddr));
    }
    static const char* hostPort(kXR_unt16 id) {
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
    static map<kXR_int64, kXR_unt16> _ids;
    static vector<char*>           _hps; // <host>:<port>
};

#endif /* XRDMONSENDERINFO_HH */
