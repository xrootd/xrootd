/*****************************************************************************/
/*                                                                           */
/*                        XrdMonDecPacketDecoder.hh                          */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$
#ifndef XRDMONDECPACKETDECODER_HH
#define XRDMONDECPACKETDECODER_HH

#include "XrdMon/XrdMonHeader.hh"
#include "XrdMon/XrdMonDecSink.hh"
#include <utility> // for pair
#include <sys/time.h>
using std::pair;

class XrdMonDecPacketDecoder {
public:
    enum { INV_SENDERID = 65500 };
    
    XrdMonDecPacketDecoder(const char* baseDir, 
                           const char* rtLogDir);

    XrdMonDecPacketDecoder(const char* baseDir,
                           bool saveTraces,
                           int maxTraceLogSize,
                           time_t upToTime);

    void init(dictid_t min, dictid_t max, const string& senderHP);
    sequen_t lastSeq() const { return _sink.lastSeq(); }
    
    void operator()(const XrdMonHeader& header,
                    const char* packet,
                    kXR_unt16 senderId=INV_SENDERID);
    bool     stopNow() const   { return _stopNow; }
    
private:
    typedef pair<time_t, time_t> TimePair; // <beg time, end time>

    struct CalcTime {
        CalcTime(float f, time_t t, int e)
            : timePerTrace(f), begTimeNextWindow(t), endOffset(e) {}
        float  timePerTrace;
        time_t begTimeNextWindow;
        int    endOffset;
    };
    
    CalcTime& f();
    
    typedef pair<float, time_t> FloatTime; // <time per trace, beg time next wind>

    void checkLostPackets(const XrdMonHeader& header);
    
    void decodeTracePacket(const char* packet, 
                           int packetLen);
    void decodeDictPacket(const char* packet, 
                          int packetLen);
    void decodeUserPacket(const char* packet, 
                          int packetLen);
    TimePair decodeTime(const char* packet);
    void decodeRWRequest(const char* packet, 
                         time_t timestamp);
    void decodeOpen(const char* packet, 
                    time_t timestamp);
    void decodeClose(const char* packet,
                     time_t timestamp);
    CalcTime prepareTimestamp(const char* packet, 
                              int& offset, 
                              int len, 
                              time_t& begTime);
private:
    XrdMonDecSink _sink;
    time_t        _time; // for verification if xrootd was restarted
    bool          _stopNow;

    time_t        _upToTime; // for decoding parts of log file
};

#endif /* XRDMONDECPACKETDECODER_HH */
