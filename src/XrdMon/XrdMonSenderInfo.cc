/*****************************************************************************/
/*                                                                           */
/*                            XrdMonSenderInfo.cc                            */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonException.hh"
#include "XrdMon/XrdMonErrors.hh"
#include "XrdMon/XrdMonSenderInfo.hh"

#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>

map<kXR_int64, kXR_unt16> XrdMonSenderInfo::_ids;
vector<char*>             XrdMonSenderInfo::_hps;

int
XrdMonSenderInfo::convert2Id(struct sockaddr_in sAddr)
{
    // convert sAddr to myid. If not regiserted yet, 
    // register and also build <hostname>:<port> and register it
    kXR_int64 myhash = (sAddr.sin_addr.s_addr << 16) + sAddr.sin_port;

    map<kXR_int64, kXR_unt16>::const_iterator itr = _ids.find(myhash);
    if ( itr != _ids.end() ) {
        return itr->second;
    }
    int id;
    id = _ids[myhash] = _hps.size();
    _hps.push_back( buildName(sAddr) );
    return id;
}

void
XrdMonSenderInfo::destructStatics()
{
    int i, s = _hps.size();
    for (i=0 ; i<s ; ++i) {
        delete [] _hps[i];
    }
}

char*
XrdMonSenderInfo::buildName(struct sockaddr_in sAddr)
{
    char hostName[256];
    char servInfo[256];
    memset((char*)hostName, 256, 0);
    memset((char*)servInfo, 256, 0);
            
    if ( 0 != getnameinfo((sockaddr*) &sAddr,
                          sizeof(sockaddr),
                          hostName,
                          256,
                          servInfo,
                          256,
                          0) ) {
        throw XrdMonException(ERR_INVALIDADDR, "Cannot resolve ip");
    }

    char* n = new char [strlen(hostName) + 8];
    sprintf(n, "%s:%d", hostName, ntohs(sAddr.sin_port));
    return n;
}

