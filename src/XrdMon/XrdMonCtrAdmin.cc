/*****************************************************************************/
/*                                                                           */
/*                        XrdMonCtrAdmin.cc                                  */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonCtrAdmin.hh"
#include "XrdMon/XrdMonErrors.hh"
#include "XrdMon/XrdMonAPException.hh"
#include <netinet/in.h> /* ntohs */

#include <iostream>
using std::cout;
using std::endl;

void
XrdMonCtrAdmin::doIt(int16_t command, int16_t arg)
{
    switch (command) {
        case c_shutdown: {
            throw XrdMonAPException(SIG_SHUTDOWNNOW);
        }
        default: {
            cout << "Invalid admin command: " << command << " ignored" << endl;
            throw XrdMonAPException(ERR_UNKNOWN);
        }
    }
}

void
XrdMonCtrAdmin::decodeAdminPacket(const char* packet,
                                  int16_t& command,
                                  int16_t& arg)
{
    int16_t x16;
    int8_t offset = HDRLEN;
    memcpy(&x16, packet+offset, sizeof(int16_t));
    offset += sizeof(int16_t);
    command = ntohs(x16);

    memcpy(&x16, packet+offset, sizeof(int16_t));
    offset += sizeof(int16_t);
    arg = ntohs(x16);
}
