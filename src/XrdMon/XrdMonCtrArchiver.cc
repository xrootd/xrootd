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
#include "XrdMon/XrdMonException.hh"
#include "XrdMon/XrdMonCtrAdmin.hh"
#include "XrdMon/XrdMonCtrArchiver.hh"
#include "XrdMon/XrdMonCtrBuffer.hh"
#include "XrdMon/XrdMonErrors.hh"
#include "XrdMon/XrdMonCtrPacket.hh"
#include "XrdMon/XrdMonSenderInfo.hh"
#include "XrdMon/XrdMonCtrWriter.hh"
#include "XrdMon/XrdMonDecPacketDecoder.hh"
#include <sys/time.h>

#include <iostream>
using std::cout;
using std::endl;

XrdMonCtrArchiver::XrdMonCtrArchiver(const char* cBaseDir, 
                                     const char* dBaseDir,
                                     const char* rtLogDir,
                                     kXR_int64 maxLogSize,
                                     bool rtDec)
    : _decoder(0), 
      _currentTime(0),
      _heartbeat(1), // force taking timestamp first time
      _rtDec(rtDec)
{
    XrdMonCtrWriter::setBaseDir(cBaseDir);
    XrdMonCtrWriter::setMaxLogSize(maxLogSize);

    if ( rtDec ) {
        _decoder = new XrdMonDecPacketDecoder(dBaseDir, rtLogDir);
    }
}

XrdMonCtrArchiver::~XrdMonCtrArchiver()
{
    delete _decoder;
    
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
        } catch (XrdMonException& e) {
            if ( e.err() == SIG_SHUTDOWNNOW ) {
                XrdMonSenderInfo::destructStatics();
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
                 << XrdMonSenderInfo::hostPort(i) << endl;
            _writers[i]->forceClose();
        }
    }
}

void
XrdMonCtrArchiver::archivePacket(XrdMonCtrPacket* p)
{
    XrdMonHeader header;
    header.decode(p->buf);

    if ( XrdMonCtrAdmin::isAdminPacket(header) ) {
        kXR_int16 command = 0, arg = 0;
        XrdMonCtrAdmin::decodeAdminPacket(p->buf, command, arg);
        XrdMonCtrAdmin::doIt(command, arg);
        return;
    }

    kXR_unt16 senderId = XrdMonSenderInfo::convert2Id(p->sender);

    if ( 0 != _decoder ) {
        _decoder->operator()(header, p->buf, senderId);
    }
    
    if ( _writers.size() <= senderId ) {
        _writers.push_back(
            new XrdMonCtrWriter(XrdMonSenderInfo::hostPort(senderId)));
    }
    _writers[senderId]->operator()(p->buf, header, _currentTime);
}

