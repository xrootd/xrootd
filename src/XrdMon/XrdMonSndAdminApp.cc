/*****************************************************************************/
/*                                                                           */
/*                           XrdMonSndAdminApp.cc                            */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonSndAdminEntry.hh"
#include "XrdMon/XrdMonSndCoder.hh"
#include "XrdMon/XrdMonSndTransmitter.hh"
#include <assert.h>

int main(int argc, char* argv[])
{
    const char* receiverHost = "127.0.0.1";

    if ( argc > 1 ) {
        if ( 0 == strcmp(argv[1], "-host") ) {
            if ( argc < 3 ) {
                cerr << "Expected argument after -host" << endl;
                return 1;
            }
            receiverHost = argv[2];
        }
    }

    XrdMonSndDebug::initialize();

    XrdMonSndCoder coder;
    XrdMonSndTransmitter transmitter;
    assert ( !transmitter.initialize(receiverHost, PORT) );

    XrdMonSndAdminEntry ae;
    ae.setShutdown();
    coder.prepare2Transfer(ae);
    transmitter(coder.packet());

    return 0;
}
