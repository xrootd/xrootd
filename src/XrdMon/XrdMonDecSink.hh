/*****************************************************************************/
/*                                                                           */
/*                             XrdMonDecSink.hh                              */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONDECSINK_HH
#define XRDMONDECSINK_HH

#include "XrdMon/XrdMonDecDictInfo.hh"
#include "XrdMon/XrdMonDecTraceInfo.hh"
#include "XrdMon/XrdMonDecUserInfo.hh"
#include "XrdMon/XrdMonDecRTLogging.hh"
#include "XrdOuc/XrdOucPthread.hh"
#include <algorithm>
#include <fstream>
#include <map>
#include <vector>
using std::fstream;
using std::map;
using std::pair;
using std::vector;

class XrdMonDecSink {
public:
    XrdMonDecSink(const char* baseDir,
                  const char* rtLogDir,
                  bool saveTraces,
                  int maxTraceLogSize);
    ~XrdMonDecSink();

    void setSenderId(kXR_unt16 id);
    
    void init(dictid_t min, dictid_t max, const string& senderHP);
    sequen_t lastSeq() const { return _lastSeq; }
    void setLastSeq(sequen_t seq) { _lastSeq = seq; }
                    
    void addDictId(dictid_t xrdId, const char* theString, int len);
    void addUserId(dictid_t xrdId, const char* theString, int len);
    void add(dictid_t xrdId, XrdMonDecTraceInfo& trace);
    void addUserDisconnect(dictid_t xrdId, kXR_int32 sec, kXR_int32 timestamp);
    void openFile(dictid_t dictId, kXR_int32 timestamp);
    void closeFile(dictid_t dictId, 
                   kXR_int64 bytesR, 
                   kXR_int64 bytesW, 
                   kXR_int32 timestamp);
    void flushHistoryData();
    void flushRealTimeData() { if ( 0 != _rtLogger ) _rtLogger->flush(); }
    
    void reset();
    
private:
    void loadUniqueIdsAndSeq();
    vector<XrdMonDecDictInfo*> loadActiveDictInfo();
    void flushClosedDicts();
    void flushUserCache();
    void flushTCache();
    void checkpoint();
    void openTraceFile(fstream& f);
    void write2TraceFile(fstream& f, const char* buf, int len);
    void registerLostPacket(dictid_t id, const char* descr);
    void reportLostPackets();
    
private:
    map<dictid_t, XrdMonDecDictInfo*> _dCache;
    map<dictid_t, XrdMonDecUserInfo*> _uCache;
    // The mutexes guard access to dCache and uCache respectively.
    // _dCache and _uCache can be accessed from different threads
    // (periodic data flushing inside dedicated thread)
    XrdOucMutex    _dMutex;
    XrdOucMutex    _uMutex;

    XrdMonDecRTLogging* _rtLogger;

    bool _saveTraces;
    typedef vector<XrdMonDecTraceInfo> TraceVector;
    TraceVector _tCache;
    kXR_unt32 _tCacheSize;
    kXR_unt16 _traceLogNumber;  // trace.000.ascii, 001, and so on...
    kXR_int64  _maxTraceLogSize; // [in MB]

    map<dictid_t, long> _lost; //lost dictIds -> number of lost traces
    
    sequen_t _lastSeq;
    dictid_t _uniqueDictId; // dictId in mySQL, unique for given xrootd host
    dictid_t _uniqueUserId; // userId in mySQL, unique for given xrootd host

    string _path;    // <basePath>/<date>_seqId_
    string _jnlPath; // <basePath>/jnl
    string _dictPath;// <basePath>/<YYYYMMDD_HH:MM:SS.MMM_dict.ascii
    string _userPath;// <basePath>/<YYYYMMDD_HH:MM:SS.MMM_user.ascii
    
    kXR_unt16 _senderId; // needed to check if senderHost can be reused
    string   _senderHost;
};

#endif /* XRDMONDECSINK_HH */
