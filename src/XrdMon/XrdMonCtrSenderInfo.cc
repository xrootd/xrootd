/*****************************************************************************/
/*                                                                           */
/*                               XrdMonCtrSenderInfo.cc                               */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonAPException.hh"
#include "XrdMon/XrdMonErrors.hh"
#include "XrdMon/XrdMonCtrSenderInfo.hh"

#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>

map<uint64_t, uint16_t> XrdMonCtrSenderInfo::_ids;
vector<char*>           XrdMonCtrSenderInfo::_hps;

int
XrdMonCtrSenderInfo::convert2Id(struct sockaddr_in sAddr)
{
    // convert sAddr to myid. If not regiserted yet, 
    // register and also build <hostname>:<port> and register it
    uint64_t myhash = (sAddr.sin_addr.s_addr << 16) + sAddr.sin_port;

    map<uint64_t, uint16_t>::const_iterator itr = _ids.find(myhash);
    if ( itr != _ids.end() ) {
        return itr->second;
    }
    int id;
    id = _ids[myhash] = _hps.size();
    _hps.push_back( buildName(sAddr) );
    return id;
}

void
XrdMonCtrSenderInfo::destructStatics()
{
    int i, s = _hps.size();
    for (i=0 ; i<s ; ++i) {
        delete [] _hps[i];
    }
}

char*
XrdMonCtrSenderInfo::buildName(struct sockaddr_in sAddr)
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
        throw XrdMonAPException(ERR_INVALIDADDR, "Cannot resolve ip");
    }

    char* n = new char [strlen(hostName) + 8];
    sprintf(n, "%s:%d", hostName, ntohs(sAddr.sin_port));
    return n;
}

