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

#include "XrdMon/XrdMonAPException.hh"
#include "XrdMon/XrdMonCommon.hh"
#include "XrdMon/XrdMonErrors.hh"
#include "XrdMon/XrdMonHeader.hh"
#include <netinet/in.h>
#include <sstream>
#include <iomanip>
using std::ostream;
using std::setw;
using std::stringstream;

void
XrdMonHeader::decode(const char* packet)
{
    memcpy(this, packet, sizeof(XrdMonHeader));
    _packetLen = ntohs(_packetLen);
    _time = ntohl(_time);    
    
    if (_packetType != PACKET_TYPE_TRACE &&
        _packetType != PACKET_TYPE_DICT  &&
        _packetType != PACKET_TYPE_ADMIN   ) {
        stringstream ss(stringstream::out);
        ss << "Invalid packet type " << _packetType;
        throw XrdMonAPException(ERR_INVPACKETTYPE, ss.str());
    }
    if ( _packetLen < HDRLEN ) {
        stringstream ss(stringstream::out);
        ss << "Invalid packet length " << _packetLen;
        throw XrdMonAPException(ERR_INVPACKETLEN, ss.str());
    }
}

ostream&
operator<<(ostream& o, const XrdMonHeader& header)
{
    o << "seq: "   << setw(3) << (int) header._seqNo 
      <<", type: " << static_cast<char>(header._packetType)
      << " len: "  << setw(4) << header._packetLen 
      << " time: " << header._time;
    return o;
}
