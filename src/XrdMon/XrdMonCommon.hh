/*****************************************************************************/
/*                                                                           */
/*                              XrdMonCommon.hh                              */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONCOMMON_HH
#define XRDMONCOMMON_HH

#include "XrdMon/XrdMonTypes.hh"

// common settings for UDP transmitter and receiver

const int32_t MAXPACKETSIZE  = 65536; // [bytes], (16 bits for length in hdr)
const int16_t HDRLEN         =     8; // [bytes]
const int16_t TRACEELEMLEN   =    16; // [bytes]

// size for data inside packet. 2*int16_t is used
// by packet type and number of elements
const int16_t TRACELEN       =    16;

const int16_t PORT           =  9930;

const char PACKET_TYPE_ADMIN = 'A';
const char PACKET_TYPE_DICT  = 'd';
const char PACKET_TYPE_TRACE = 't';

//const char XROOTD_MON_CLOSE         = 0xc0;
//const char XROOTD_MON_OPEN          = 0x80;
//const char XROOTD_MON_WINDOW        = 0xe0;
const char XROOTD_MON_RWREQUESTMASK = 0x80;
// why 0x80: anything that is < 0x7f is rwrequest
// 0x7f = 01111111, so !(x & 10000000), 1000 0000=0x80

enum AdminCommand {
    c_shutdown = 1000
};

#endif /* XRDMONCOMMON_HH */
