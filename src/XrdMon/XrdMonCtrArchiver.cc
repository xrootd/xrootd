/*****************************************************************************/
/*                                                                           */
/*                           XrdMonCtrArchiver.cc                            */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonCommon.hh"
#include "XrdMon/XrdMonAPException.hh"
#include "XrdMon/XrdMonCtrArchiver.hh"
#include "XrdMon/XrdMonCtrBuffer.hh"
#include "XrdMon/XrdMonErrors.hh"
#include "XrdMon/XrdMonCtrPacket.hh"
#include "XrdMon/XrdMonCtrSenderInfo.hh"
#include "XrdMon/XrdMonCtrWriter.hh"
#include <sys/time.h>

#include <iostream>
using std::cout;
using std::endl;

XrdMonCtrArchiver::XrdMonCtrArchiver()
    : _currentTime(0),
      _heartbeat(1) // force taking timestamp first time
{}

XrdMonCtrArchiver::~XrdMonCtrArchiver()
{
    // go through all writers and shut them down
    int i, s = _writers.size();
    for (i=0 ; i<s ; i++) {
        delete _writers[i];
    }
}

void
XrdMonCtrArchiver::operator()()
{
    XrdMonCtrBuffer* pb = XrdMonCtrBuffer::instance();
    while ( 1 ) {
        try {
            if ( 0 == --_heartbeat ) {
                check4InactiveSenders();
            }
            XrdMonCtrPacket* p = pb->pop_front();
            archivePacket(p);
            delete p;
        } catch (XrdMonAPException& e) {
            if ( e.err() == SIG_SHUTDOWNNOW ) {
                XrdMonCtrSenderInfo::destructStatics();
                return;
            }
            e.printItOnce();
        }
    }
}

void
XrdMonCtrArchiver::check4InactiveSenders()
{
    _heartbeat = TIMESTAMP_FREQ;
    struct timeval tv;
    gettimeofday(&tv, 0);
    _currentTime = tv.tv_sec;
    
    long allowed = _currentTime - MAX_INACTIVITY;
    int i, s = _writers.size();
    for (i=0 ; i<s ; i++) {
        if ( _writers[i]->lastActivity() < allowed ) {
            cout << "No activity for " << MAX_INACTIVITY << " sec., "
                 << "closing all files for sender " 
                 << XrdMonCtrSenderInfo::hostPort(i) << endl;
            _writers[i]->forceClose();
        }
    }
}

void
XrdMonCtrArchiver::archivePacket(XrdMonCtrPacket* p)
{
    uint16_t senderId = XrdMonCtrSenderInfo::convert2Id(p->sender);    

    if ( _writers.size() <= senderId ) {
        _writers.push_back(
            new XrdMonCtrWriter(XrdMonCtrSenderInfo::hostPort(senderId)));
    }
    _writers[senderId]->operator()(p->buf, _currentTime);
}

