/*****************************************************************************/
/*                                                                           */
/*                              XrdMonHeader.hh                              */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONHEADER_HH
#define XRDMONHEADER_HH

#include "XrdMon/XrdMonCommon.hh"
#include "XrdMon/XrdMonTypes.hh"

#include <iostream>
#include <sys/time.h>
using std::ostream;

class XrdMonHeader {
public:
    packet_t    packetType() const { return _packetType; }
    sequen_t    seqNo()      const { return _seqNo; }
    packetlen_t packetLen()  const { return _packetLen; }
    time_t      time()       const { return _time; }
    
    void decode(const char* packet);

private:
    packet_t    _packetType;
    sequen_t    _seqNo;
    packetlen_t _packetLen;
    time_t      _time;

    friend ostream& operator<<(ostream& o, 
                               const XrdMonHeader& header);
};

#endif /* XRDMONHEADER_HH */

