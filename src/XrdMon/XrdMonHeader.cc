/*****************************************************************************/
/*                                                                           */
/*                              XrdMonHeader.cc                              */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonException.hh"
#include "XrdMon/XrdMonCommon.hh"
#include "XrdMon/XrdMonErrors.hh"
#include "XrdMon/XrdMonHeader.hh"
#include <netinet/in.h>
#include <iomanip>
using std::setw;

void
XrdMonHeader::decode(const char* packet)
{
    memcpy(&_header, packet, sizeof(XrdXrootdMonHeader));
    _header.plen = ntohs(_header.plen);
    _header.stod = ntohl(_header.stod);
    
    if (packetType() != PACKET_TYPE_TRACE &&
        packetType() != PACKET_TYPE_DICT  &&
        packetType() != PACKET_TYPE_ADMIN &&
        packetType() != PACKET_TYPE_USER     ) {
        char buf[64];
        sprintf(buf, "Invalid packet type %c", packetType());
        throw XrdMonException(ERR_INVPACKETTYPE, buf);
    }
    if ( packetLen() < HDRLEN ) {
        char buf[64];
        sprintf(buf, "Invalid packet length %d", packetLen());
        throw XrdMonException(ERR_INVPACKETLEN, buf);
    }
}

ostream&
operator<<(ostream& o, const XrdMonHeader& header)
{
    o << "seq: "   << setw(3) << (int) header.seqNo() 
      <<", type: " << static_cast<char>(header.packetType())
      << " len: "  << setw(4) << header.packetLen() 
      << " time: " << header.stod();
    return o;
}
