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
#include <fstream>
#include <map>
using std::fstream;
using std::map;

class XrdMonDecTraceInfo;

class XrdMonDecSink {
public:
    XrdMonDecSink(const char* baseDir,
                  bool saveTraces,
                  int maxTraceLogSize);
    ~XrdMonDecSink();

    void setSenderId(uint16_t id);
    
    void init(dictid_t min, dictid_t max);
    sequen_t lastSeq() const { return _lastSeq; }
    void setLastSeq(sequen_t seq) { _lastSeq = seq; }
                    
    void add(dictid_t xrdId, const char* theString, int len);
    void add(dictid_t xrdId, XrdMonDecTraceInfo& trace);
    void openFile(dictid_t dictId, time_t timestamp);
    void closeFile(dictid_t dictId, 
                   int64_t bytesR, 
                   int64_t bytesW, 
                   time_t timestamp);

private:
    void loadUniqueIdAndSeq();
    vector<XrdMonDecDictInfo*> loadActiveDictInfo();
    void flushClosedDicts();
    void flushTCache();
    void checkpoint();
    void openTraceFile(fstream& f);
    void write2TraceFile(fstream& f, const char* buf, int len);
    void registerLostPacket(dictid_t id, const char* descr);
    string buildDictFileName();
    
private:
    map<dictid_t, XrdMonDecDictInfo*> _dCache;

    bool _saveTraces;
    typedef vector<XrdMonDecTraceInfo> TraceVector;
    TraceVector _tCache;
    uint32_t _tCacheSize;
    uint16_t _traceLogNumber;  // trace.000.ascii, 001, and so on...
    int64_t  _maxTraceLogSize; // [in MB]

    map<dictid_t, long> _lost; //lost dictIds -> number of lost traces
    
    sequen_t _lastSeq;
    dictid_t _uniqueId; // dictId in mySQL, unique for given xrootd host

    uint16_t _logNameSeqId; // to build unique names for multiple log files 
                            // created by the same process
    string _path;    // <basePath>/<date>_seqId_
    string _jnlPath; // <basePath>/jnl
    
    uint16_t _senderId; // needed to check if senderHost can be reused
    string   _senderHost;
};

#endif /* XRDMONDECSINK_HH */
