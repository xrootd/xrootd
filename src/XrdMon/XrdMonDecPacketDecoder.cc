/*****************************************************************************/
/*                                                                           */
/*                        XrdMonDecPacketDecoder.cc                          */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonAPException.hh"
#include "XrdMon/XrdMonCommon.hh"
#include "XrdMon/XrdMonHeader.hh"
#include "XrdMon/XrdMonUtils.hh"
#include "XrdMon/XrdMonErrors.hh"
#include "XrdMon/XrdMonDecPacketDecoder.hh"
#include "XrdMon/XrdMonDecTraceInfo.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdXrootd/XrdXrootdMonData.hh"
#include <netinet/in.h>
#include <sstream>
using std::cerr;
using std::cout;
using std::endl;
using std::stringstream;

// for light decoding in real time
XrdMonDecPacketDecoder::XrdMonDecPacketDecoder(const char* baseDir)
    : _sink(baseDir, false, 2),
      _time(0),
      _stopNow(false),
      _upToTime(0)
{}

XrdMonDecPacketDecoder::XrdMonDecPacketDecoder(const char* baseDir,
                                               bool saveTraces,
                                               int maxTraceLogSize,
                                               time_t upToTime)
    : _sink(baseDir, saveTraces, maxTraceLogSize),
      _time(0),
      _stopNow(false),
      _upToTime(upToTime)
{}

void
XrdMonDecPacketDecoder::init(dictid_t min, dictid_t max)
{
    _sink.init(min, max);
}    

// true if up-to-time reached and decoding should stop
void
XrdMonDecPacketDecoder::operator()(uint16_t senderId,
                                   const XrdMonHeader& header,
                                   const char* packet)
{
    _sink.setSenderId(senderId);
    
    int len = header.packetLen() - HDRLEN;
    cout << "header " << header << endl;
    
    if ( len < 1 ) {
        cout << "Warning: Ignoring empty packet" << endl;
        return;
    }
    
    if ( header.packetType() == PACKET_TYPE_TRACE ) {
        decodeTracePacket(packet+HDRLEN, len);
    } else if ( header.packetType() == PACKET_TYPE_DICT ) {
        decodeDictPacket(packet+HDRLEN, len);
    } else {
        cerr << "Unsupported packet type: " << header.packetType() << endl;
    }
    _sink.setLastSeq(header.seqNo());
}

// packet should point to data after header
void
XrdMonDecPacketDecoder::decodeTracePacket(const char* packet, int len)
{
    // decode first packet - time window
    if ( *packet != XROOTD_MON_WINDOW ) {
        stringstream se(stringstream::out);
        se << "Expected time window packet (1st packet), got " << (int) *packet;
        throw XrdMonAPException(ERR_NOTATIMEWINDOW, se.str());
    }
    TimePair t = decodeTime(packet);
    if ( _upToTime != 0 && _upToTime <= t.first ) {
        cout << "reached the up-to-time, will stop decoding now" << endl;
        _stopNow = true;
        return;
    }
    time_t begTime = t.second;
    int offset = TRACELEN;

    //cout << "Decoded time (first) " << t.first << " " << t.second << endl;
    
    while ( offset < len ) {
        CalcTime ct = prepareTimestamp(packet, offset, len, begTime);
        int elemNo = 0;
        while ( offset<ct.endOffset ) {
            char infoType = *(packet+offset);
            time_t timestamp = begTime + (time_t) (elemNo++ * ct.timePerTrace);
            if ( !(infoType & XROOTD_MON_RWREQUESTMASK) ) {
                decodeRWRequest(packet+offset, timestamp);
            } else if ( infoType == XROOTD_MON_OPEN ) {
                decodeOpen(packet+offset, timestamp);
            } else if ( infoType == XROOTD_MON_CLOSE ) {
                decodeClose(packet+offset, timestamp);
            } else {
                stringstream es(stringstream::out);
                es << "Unsupported infoType of trace packet: " << infoType;
                throw XrdMonAPException(ERR_INVALIDINFOTYPE, es.str());
            }
            offset += TRACELEN;
        }
        begTime = ct.begTimeNextWindow;
        offset += TRACELEN; // skip window trace which was already read
    }
}

// packet should point to data after header
void
XrdMonDecPacketDecoder::decodeDictPacket(const char* packet, int len)
{
    int32_t x32;
    memcpy(&x32, packet, sizeof(int32_t));
    dictid_t dictId = ntohl(x32);
    
    _sink.add(dictId, packet+sizeof(int32_t), len-sizeof(int32_t));
}

XrdMonDecPacketDecoder::TimePair
XrdMonDecPacketDecoder::decodeTime(const char* packet)
{
    struct X {
        int32_t endT;
        int32_t begT;
    } x;

    memcpy(&x, packet+sizeof(int64_t), sizeof(X));
    return TimePair(ntohl(x.endT), ntohl(x.begT));
}

void
XrdMonDecPacketDecoder::decodeRWRequest(const char* packet, time_t timestamp)
{
    struct X {
        int64_t tOffset;
        int32_t tLen;
        int32_t dictId;
    } x;
    memcpy(&x, packet, sizeof(X));
    x.tOffset = ntohll(x.tOffset);
    x.tLen = ntohl(x.tLen);
    x.dictId = ntohl(x.dictId);

    if ( x.tOffset < 0 ) {
        throw XrdMonAPException(ERR_NEGATIVEOFFSET);
    }
    char rwReq = 'r';
    if ( x.tLen<0 ) {
        rwReq = 'w';
        x.tLen *= -1;
    }

    XrdMonDecTraceInfo trace(x.tOffset, x.tLen, rwReq, timestamp);
    _sink.add(x.dictId, trace);
}

void
XrdMonDecPacketDecoder::decodeOpen(const char* packet, time_t timestamp)
{
    int32_t dictId;
    memcpy(&dictId, 
           packet+sizeof(int64_t)+sizeof(int32_t), 
           sizeof(int32_t));
    dictId = ntohl(dictId);

    _sink.openFile(dictId, timestamp);
}

void
XrdMonDecPacketDecoder::decodeClose(const char* packet, time_t timestamp)
{
    XrdXrootdMonTrace trace;
    memcpy(&trace, packet, sizeof(XrdXrootdMonTrace));
    uint32_t dictId = ntohl(trace.data.arg2.dictid);
    uint32_t tR    = ntohl(trace.data.arg0.rTot[1]);
    uint32_t tW    = ntohl(trace.data.arg1.wTot);
    char rShift    = trace.data.arg0.id[1];
    char wShift    = trace.data.arg0.id[2];
    int64_t realR = tR; realR = realR << rShift;
    int64_t realW = tW; realW = realW << wShift;

    cout << "decoded close file, dict " << dictId 
         << ", total r " << tR << " shifted " << (int) rShift << ", or " << realR
         << ", total w " << tW << " shifted " << (int) wShift << ", or " << realW
         << endl;

    _sink.closeFile(dictId, realR, realW, timestamp);
}

XrdMonDecPacketDecoder::CalcTime
XrdMonDecPacketDecoder::prepareTimestamp(const char* packet, 
                                         int& offset, 
                                         int len, 
                                         time_t& begTime)
{
    // look for time window
    int x = offset;
    int noElems = 0;
    while ( *(packet+x) != XROOTD_MON_WINDOW ) {
        if ( x >= len ) {
            throw XrdMonAPException(ERR_NOTATIMEWINDOW, 
                              "Expected time window packet (last packet)");
        }
        x += TRACELEN;
        ++noElems;
    }

    // cout << "Found timestamp, offset " << x 
    //     << " after " << noElems << " elements" << endl;
    
    // decode time window
    TimePair t = decodeTime(packet+x);

    // cout << "decoded time " << t.first << " " << t.second << endl;
    
    if ( begTime > t.first ) {
        stringstream se(stringstream::out);
        se << "Wrong time: " << begTime 
           << " > " << t.first << " at offset " << x << ", will fix";
        cout << se.str() << endl;
        begTime = t.first;
        //throw XrdMonAPException(ERR_INVALIDTIME, se.str());
    }

    float timePerTrace = ((float)(t.first - begTime)) / noElems;
    //cout << "timepertrace = " << timePerTrace << endl;
    
    return CalcTime(timePerTrace, t.second, x);
}
