/*****************************************************************************/
/*                                                                           */
/*                           XrdMonCtrMainApp.cc                             */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonTypes.hh"
#include "XrdMon/XrdMonUtils.hh"
#include "XrdMon/XrdMonCtrArchiver.hh"
#include "XrdMon/XrdMonCtrDebug.hh"
#include "XrdMon/XrdMonCtrCollector.hh"

#include <iostream>
using std::cerr;
using std::endl;

int main(int argc, char* argv[])
{
    XrdMonCtrDebug::initialize();

    // FIXME this block should really be configurable
    bool rtDec = true; // real time decoding on by default
    const kXR_int64 MAXFILESIZE = 1024*1024*1024; // 1GB

    const char* BASEDIR         = "./logs";
    const char* COLLECTORLOGDIR = "./logs/collector";
    const char* DECODERLOGDIR   = "./logs/decoder";
    const char* RTLOGDIR        = "./logs/rt";
    const int   DECFLUSHDELAY   =  600; // [sec] 
    // end of to-be configurable block

    mkdirIfNecessary(BASEDIR);
    mkdirIfNecessary(COLLECTORLOGDIR);
    mkdirIfNecessary(DECODERLOGDIR);
    mkdirIfNecessary(RTLOGDIR);

    if ( rtDec ) {
        XrdMonCtrArchiver::_decFlushDelay = DECFLUSHDELAY;
    }
    
    // start thread for receiving data
    pthread_t recThread;
    if ( 0 != pthread_create(&recThread, 
                             0, 
                             receivePackets,
                             0) ) {
        cerr << "Failed to create a collector thread" << endl;
        return 1;
    }

    // store received packets until admin packet with sigterm arrives
    XrdMonCtrArchiver archiver(COLLECTORLOGDIR, 
                               DECODERLOGDIR,
                               RTLOGDIR,
                               MAXFILESIZE, 
                               rtDec);
    archiver();

    return 0;
}
