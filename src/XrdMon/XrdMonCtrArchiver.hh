/*****************************************************************************/
/*                                                                           */
/*                          XrdMonCtrArchiver.hh                             */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONCTRARCHIVER_HH
#define XRDMONCTRARCHIVER_HH

#include "XrdMon/XrdMonTypes.hh"
#include "pthread.h"
#include <vector>
using std::vector;

class XrdMonCtrPacket;
class XrdMonCtrWriter;
class XrdMonDecPacketDecoder;

// Class responsible for archiving packets in log files.
// Manages heartbeat for writers (writers inactive for 24 hours
// are closed). It does not interpret data inside packet.

class XrdMonCtrArchiver {
public:
    XrdMonCtrArchiver(const char* cBaseDir, 
                      const char* dBaseDir,
                      const char* rtLogDir,
                      kXR_int64 maxFileSize,
                      bool rtDec);
    ~XrdMonCtrArchiver();
    void operator()();

    void reset();
    
    static int _decFlushDelay; // #sec between flushes of decoded data to disk

private:
    void check4InactiveSenders();
    void archivePacket(XrdMonCtrPacket* p);
    static void* decFlushHeartBeat(void* arg);
    void deleteWriters();
    
private:
    enum { TIMESTAMP_FREQ = 10000,   // re-take time every X packets
           MAX_INACTIVITY = 60*60*24 // kill writer if no activity for 24 hours
    };
    
    vector<XrdMonCtrWriter*> _writers;

    XrdMonDecPacketDecoder* _decoder;
    pthread_t               _decFlushThread;

    long _currentTime;
    int  _heartbeat; // number of packets since the last time check
};

#endif /*  XRDMONCTRARCHIVER_HH */
