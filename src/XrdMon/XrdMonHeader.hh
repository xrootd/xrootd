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
#include "XrdXrootd/XrdXrootdMonData.hh"

#include <iostream>
#include <sys/time.h>
using std::ostream;

class XrdMonHeader {
public:
    packet_t    packetType() const { return _header.code; }
    sequen_t    seqNo()      const { return _header.pseq; }
    packetlen_t packetLen()  const { return _header.plen; }
    time_t      time()       const { return _header.stod; }
    
    void decode(const char* packet);

private:
    XrdXrootdMonHeader _header;

    friend ostream& operator<<(ostream& o, 
                               const XrdMonHeader& header);
};

#endif /* XRDMONHEADER_HH */

