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

#include "XrdMon/XrdMonException.hh"
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
XrdMonDecPacketDecoder::XrdMonDecPacketDecoder(const char* baseDir, 
                                               const char* rtLogDir)
    : _sink(baseDir, rtLogDir, false, 2),
      _time(0),
      _stopNow(false),
      _upToTime(0)
{}

XrdMonDecPacketDecoder::XrdMonDecPacketDecoder(const char* baseDir,
                                               bool saveTraces,
                                               int maxTraceLogSize,
                                               time_t upToTime)
    : _sink(baseDir, 0, saveTraces, maxTraceLogSize),
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
XrdMonDecPacketDecoder::operator()(kXR_unt16 senderId,
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
    if ( static_cast<kXR_char>(*packet) != XROOTD_MON_WINDOW ) {
        stringstream se(stringstream::out);
        se << "Expected time window packet (1st packet), got " << (int) *packet;
        throw XrdMonException(ERR_NOTATIMEWINDOW, se.str());
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
            kXR_char infoType = static_cast<kXR_char>(*(packet+offset));
            time_t timestamp = begTime + (time_t) (elemNo++ * ct.timePerTrace);
            if ( !(infoType & XROOTD_MON_RWREQUESTMASK) ) {
                decodeRWRequest(packet+offset, timestamp);
            } else if ( infoType == XROOTD_MON_OPEN ) {
                decodeOpen(packet+offset, timestamp);
            } else if ( infoType == XROOTD_MON_CLOSE ) {
                decodeClose(packet+offset, timestamp);
            } else {
                stringstream es(stringstream::out);
                es << "Unsupported infoType of trace packet: " 
                   << (int) infoType;
                throw XrdMonException(ERR_INVALIDINFOTYPE, es.str());
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
    kXR_int32 x32;
    memcpy(&x32, packet, sizeof(kXR_int32));
    dictid_t dictId = ntohl(x32);
    
    _sink.add(dictId, packet+sizeof(kXR_int32), len-sizeof(kXR_int32));
}

XrdMonDecPacketDecoder::TimePair
XrdMonDecPacketDecoder::decodeTime(const char* packet)
{
    struct X {
        kXR_int32 endT;
        kXR_int32 begT;
    } x;

    memcpy(&x, packet+sizeof(kXR_int64), sizeof(X));
    return TimePair(ntohl(x.endT), ntohl(x.begT));
}

void
XrdMonDecPacketDecoder::decodeRWRequest(const char* packet, time_t timestamp)
{
    struct X {
        kXR_int64 tOffset;
        kXR_int32 tLen;
        kXR_int32 dictId;
    } x;
    memcpy(&x, packet, sizeof(X));
    x.tOffset = ntohll(x.tOffset);
    x.tLen = ntohl(x.tLen);
    x.dictId = ntohl(x.dictId);

    if ( x.tOffset < 0 ) {
        throw XrdMonException(ERR_NEGATIVEOFFSET);
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
    kXR_int32 dictId;
    memcpy(&dictId, 
           packet+sizeof(kXR_int64)+sizeof(kXR_int32), 
           sizeof(kXR_int32));
    dictId = ntohl(dictId);

    _sink.openFile(dictId, timestamp);
}

void
XrdMonDecPacketDecoder::decodeClose(const char* packet, time_t timestamp)
{
    XrdXrootdMonTrace trace;
    memcpy(&trace, packet, sizeof(XrdXrootdMonTrace));
    kXR_unt32 dictId = ntohl(trace.data.arg2.dictid);
    kXR_unt32 tR    = ntohl(trace.data.arg0.rTot[1]);
    kXR_unt32 tW    = ntohl(trace.data.arg1.wTot);
    char rShift    = trace.data.arg0.id[1];
    char wShift    = trace.data.arg0.id[2];
    kXR_int64 realR = tR; realR = realR << rShift;
    kXR_int64 realW = tW; realW = realW << wShift;

    //cout << "decoded close file, dict " << dictId 
    //     << ", total r " << tR << " shifted " << (int) rShift << ", or " << realR
    //     << ", total w " << tW << " shifted " << (int) wShift << ", or " << realW
    //     << endl;

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
    while ( static_cast<kXR_char>(*(packet+x)) != XROOTD_MON_WINDOW ) {
        if ( x >= len ) {
            throw XrdMonException(ERR_NOTATIMEWINDOW, 
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
        //throw XrdMonException(ERR_INVALIDTIME, se.str());
    }

    float timePerTrace = ((float)(t.first - begTime)) / noElems;
    //cout << "timepertrace = " << timePerTrace << endl;
    
    return CalcTime(timePerTrace, t.second, x);
}
