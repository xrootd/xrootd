/*****************************************************************************/
/*                                                                           */
/*                            XrdMonSndCoder.cc                              */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonSndCoder.hh"
#include <sys/time.h>
#include <iomanip>
using std::setw;

time_t XrdMonSndCoder::_serverStartTime = 0;

XrdMonSndCoder::XrdMonSndCoder()
    : _sequenceNo(0),
      _noDict(0),
      _noOpen(0),
      _noClose(0),
      _noTrace(0),
      _noTime(0)
{
    if ( 0 == _serverStartTime ) {
        struct timeval tv;
        gettimeofday(&tv, 0);
        _serverStartTime = tv.tv_sec;
    }
}

int
XrdMonSndCoder::prepare2Transfer(const XrdMonSndAdminEntry& ae)
{
    int32_t packetSize = HDRLEN + ae.size();

    int ret = reinitXrdMonSndPacket(packetSize, PACKET_TYPE_ADMIN);
    if ( 0 != ret ) {
        return ret;
    }

    add_int16_t(ae.command());
    add_int16_t(ae.arg());

    return 0;
}

int
XrdMonSndCoder::prepare2Transfer(const vector<XrdMonSndTraceEntry>& vector)
{
    int16_t noElems = vector.size() + 3; // 3: 3 time entries
    if (vector.size() == 0 ) {
        noElems = 0;
    }
    
    int32_t packetSize = HDRLEN + noElems * TRACEELEMLEN;
    if ( packetSize > MAXPACKETSIZE ) {
        cerr << "Internal error: cached too many entries: " << noElems
             << ", MAXPACKETSIZE = " << MAXPACKETSIZE;
        noElems = (MAXPACKETSIZE-HDRLEN) / TRACEELEMLEN;
        cerr << " Will send only " << noElems << endl;
    }

    int ret = reinitXrdMonSndPacket(packetSize, PACKET_TYPE_TRACE);
    if ( 0 != ret ) {
        return ret;
    }

    int16_t middle = noElems/2;
    int32_t curTime = time(0);
    for (int16_t i=0 ; i<noElems-3 ; i++ ) {
        if (i== 0) { // add time entry
            add_Mark(XROOTD_MON_WINDOW);
            add_int32_t(curTime); // prev window ended
            add_int32_t(curTime);   // this window started
            ++_noTime;
            if ( XrdMonSndDebug::verbose(XrdMonSndDebug::SPacket) ) {
                cout << "Adding time window {" << curTime << ", " 
                     << curTime << "}" << ", elem no " << i << endl;
            }
        }
        if (i== middle) { // add time entry
            add_Mark(XROOTD_MON_WINDOW);
            add_int32_t(curTime); // prev window ended
            add_int32_t(curTime);   // this window started
            ++_noTime;
            if ( XrdMonSndDebug::verbose(XrdMonSndDebug::SPacket) ) {
                cout << "Adding time window {" << curTime << ", " 
                     << curTime << "}" << ", elem no " << i << endl;
            }
        }
        const XrdMonSndTraceEntry& de = vector[i];
        add_int64_t(de.offset());
        add_int32_t(de.length());
        add_int32_t(de.id()    );
        if (i==noElems-4) {
            add_Mark(XROOTD_MON_WINDOW);
            add_int32_t(curTime); // prev window ended
            add_int32_t(curTime);   // this window started
            ++_noTime;
            if ( XrdMonSndDebug::verbose(XrdMonSndDebug::SPacket) ) {
                cout << "Adding time window {" << curTime << ", " 
                     << curTime << "}" << ", elem no " << i << endl;
            }
        }
    }
    _noTrace += vector.size();
    
    return 0;
}

int 
XrdMonSndCoder::prepare2Transfer(const vector<int32_t>& vector)
{
    int16_t noElems = vector.size() + 2; // 2: 2 time entries
    int8_t sizeOfXrdMonSndTraceEntry = sizeof(int64_t)+sizeof(int32_t)+sizeof(int32_t);
    int32_t packetSize = HDRLEN + noElems * sizeOfXrdMonSndTraceEntry;
    if ( packetSize > MAXPACKETSIZE ) {
        cerr << "Internal error: cached too many entries: " << noElems
             << ", MAXPACKETSIZE = " << MAXPACKETSIZE;
        noElems = (MAXPACKETSIZE-HDRLEN) / sizeOfXrdMonSndTraceEntry;
        cerr << " Will send only " << noElems << endl;
    }

    int ret = reinitXrdMonSndPacket(packetSize, PACKET_TYPE_TRACE);
    if ( 0 != ret ) {
        return ret;
    }

    int32_t curTime = time(0);
    add_Mark(XROOTD_MON_WINDOW);
    add_int32_t(curTime); // prev window ended
    add_int32_t(curTime);   // this window started
    ++_noTime;
    if ( XrdMonSndDebug::verbose(XrdMonSndDebug::SPacket) ) {
        cout << "Adding time window {" << curTime << ", " 
             << curTime << "}" << ", elem no 0" << endl;
    }

    for (int16_t i=0 ; i<noElems-2 ; i++ ) {
        add_Mark(XROOTD_MON_CLOSE);
        add_int32_t(0);
        add_int32_t(vector[i]);
        ++_noClose;
        cout << "closing file, dictid " << vector[i] << endl;
    }
    add_Mark(XROOTD_MON_WINDOW);
    add_int32_t(curTime); // prev window ended
    add_int32_t(curTime); // this window started
    ++_noTime;
    if ( XrdMonSndDebug::verbose(XrdMonSndDebug::SPacket) ) {
        cout << "Adding time window {" << curTime << ", " 
             << curTime << "}" << ", elem no " << noElems-2 << endl;
    }
    return 0;
}



int
XrdMonSndCoder::prepare2Transfer(const XrdMonSndDictEntry::CompactEntry& ce)
{
    int32_t packetSize = HDRLEN + ce.size();

    int ret = reinitXrdMonSndPacket(packetSize, PACKET_TYPE_DICT);
    if ( 0 != ret ) {
        return ret;
    }

    add_int32_t(ce.id);
    add_string  (ce.others);

    ++_noDict;
    
    return 0;
}

int
XrdMonSndCoder::reinitXrdMonSndPacket(packetlen_t newSize, char packetCode)
{
    _putOffset = 0;
    int ret = _packet.init(newSize);
    if ( 0 != ret ) {
        return ret;
    }

    if ( XrdMonSndDebug::verbose(XrdMonSndDebug::SPacket) ) {
        cout << "XrdMonSndPacket " << packetCode 
             << ", size " << setw(5) << newSize 
             << ", sequenceNo " << setw(3) << (int) _sequenceNo 
             << ", time " << _serverStartTime
             << " prepared for sending" << endl;
    }
    add_int08_t(packetCode);
    add_int08_t(_sequenceNo++);
    add_uint16_t(newSize);
    add_int32_t(_serverStartTime);

    return 0;
}

void
XrdMonSndCoder::printStats() const
{
    cout <<   "dict="    << _noDict
         << ", noOpen="  << _noOpen
         << ", noClose=" << _noClose
         << ", noTrace=" << _noTrace
         << ", noTime="  << _noTime << endl;
}

