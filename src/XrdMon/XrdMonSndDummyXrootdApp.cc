/*****************************************************************************/
/*                                                                           */
/*                        XrdMonSndDummyXrootdApp.cc                         */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonUtils.hh"
#include "XrdMon/XrdMonSndCoder.hh"
#include "XrdMon/XrdMonSndDebug.hh"
#include "XrdMon/XrdMonSndDictEntry.hh"
#include "XrdMon/XrdMonSndDummyXrootd.hh"
#include "XrdMon/XrdMonSndTraceCache.hh"
#include "XrdMon/XrdMonSndTraceEntry.hh"
#include "XrdMon/XrdMonSndTransmitter.hh"

#include <assert.h>
#include <unistd.h>  /* usleep */
#include <sys/time.h>

// known problems with 2 and 4
//const int64_t NOCALLS = 8640000;   24h worth
const int64_t NOCALLS = 1000000000;
const int16_t maxNoXrdMonSndPackets = 500;


void
doDictionaryXrdMonSndPacket(XrdMonSndDummyXrootd& xrootd, 
                            XrdMonSndCoder& coder,
                            XrdMonSndTransmitter& transmitter,
                            int64_t& noP)
{
    XrdMonSndDictEntry m = xrootd.newXrdMonSndDictEntry();
    cout << m << endl;

    XrdMonSndDictEntry::CompactEntry ce = m.code();
    
    if ( 0 == coder.prepare2Transfer(ce) ) {
        transmitter(coder.packet());
        coder.reset();
        ++noP;
    }
}

void
doTraceXrdMonSndPacket(XrdMonSndDummyXrootd& xrootd,
                       XrdMonSndCoder& coder, 
                       XrdMonSndTransmitter& transmitter,
                       XrdMonSndTraceCache& cache, 
                       int64_t& noP)
{
    XrdMonSndTraceEntry de = xrootd.newXrdMonSndTraceEntry();
    // add to buffer, perhaps transmit
    cache.add(de);
    if ( ! cache.bufferFull() ) {
        return;
    }
    
    if ( 0 == coder.prepare2Transfer(cache.getVector()) ) {
        cache.clear();
        transmitter(coder.packet());
        coder.reset();
        noP++;
    }
}

void
closeFiles(XrdMonSndDummyXrootd& xrootd,
           XrdMonSndCoder& coder, 
           XrdMonSndTransmitter& transmitter,
           int64_t& noP)
{
    vector<int32_t> closedFiles;
    xrootd.closeFiles(closedFiles);

    int s = closedFiles.size();
    int pos = 0;
    unsigned int i;
    while ( pos < s ) {
        vector<int32_t> v;
        for (i=0 ; i<XrdMonSndTraceCache::NODATAELEMS-2 && pos<s ; ++i, ++pos) {
            v.push_back(closedFiles.back());
            closedFiles.pop_back();
        }
        coder.prepare2Transfer(v);
        transmitter(coder.packet());
        noP++;
    }
}

// XrdMonSndDummyXrootd - main class

int main(int argc, char* argv[]) {

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
    
    const char* inputPathFile = "../others/paths.txt";

    int32_t seed = 12345;

    srand(seed);
    
    XrdMonSndDummyXrootd::NEWUSERFREQUENCY  =  200;
    XrdMonSndDummyXrootd::NEWPROCFREQUENCY  =   50;
    int16_t NEWDICTENTRYFREQUENCY =  8000;
    int16_t calls2NewXrdMonSndDictEntry    =  1;    
    
    XrdMonSndDebug::initialize();

    XrdMonSndDummyXrootd xrootd;
    assert ( !xrootd.initialize(inputPathFile) );
    
    XrdMonSndTraceCache cache;
    XrdMonSndCoder coder;
    XrdMonSndTransmitter transmitter;

    assert ( !transmitter.initialize(receiverHost, PORT) );
    int64_t noP = 0;

    while ( 0 != access("start.txt", F_OK) ) {
        static bool warned = false;
        if ( ! warned ) {
            cout << "Waiting for start.txt file\n";
            warned = true;
        }
        sleep(1);
    }
    
    for ( int64_t i=0 ; i<NOCALLS ; i++ ) {
        if ( ! --calls2NewXrdMonSndDictEntry ) {
            calls2NewXrdMonSndDictEntry = NEWDICTENTRYFREQUENCY;
            doDictionaryXrdMonSndPacket(xrootd, coder, transmitter, noP);
        } else {
            doTraceXrdMonSndPacket(xrootd, coder, transmitter, cache, noP);            
        }
        if ( noP >= maxNoXrdMonSndPackets-2 ) {
            break;
        }
        if ( 0 == access("stop.txt", F_OK) ) {
            break;
        }
        if ( i%1001 == 1000 ) {
            usleep(1);
        }
    }

    if ( XrdMonSndDebug::verbose(XrdMonSndDebug::Sending) ) {
        cout << "Flushing cache" << endl;
    }
    if ( 0 == coder.prepare2Transfer(cache.getVector()) ) {
        cache.clear();
        transmitter(coder.packet());
        coder.reset();
        noP++;
    }

    closeFiles(xrootd, coder, transmitter, noP);

    // set shutdown signal
    //XrdMonSndAdminEntry ae;
    //ae.setShutdown();
    //coder.prepare2Transfer(ae);
    //transmitter(coder.packet());

    transmitter.shutdown();

    coder.printStats();
    
    return 0;
}
