/*****************************************************************************/
/*                                                                           */
/*                            XrdMonCtrWriter.hh                             */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONCTRWRITER_HH
#define XRDMONCTRWRITER_HH

#include "XrdMon/XrdMonTypes.hh"
#include <fstream>
#include <string>
using std::fstream;
using std::ostream;
using std::string;

class XrdMonHeader;

// Class writes data to a log file.
// One instance per one xrootd instance.
// It buffers data in memory to avoid 
// overloading disks

class XrdMonCtrWriter {
public:
    XrdMonCtrWriter(const char* senderHP);
    ~XrdMonCtrWriter();
    void operator()(const char* packet, 
                    const XrdMonHeader& header, 
                    long currentTime);
    void forceClose();
    long lastActivity() const { return _lastActivity; }

    static void setBaseDir(const string dir) { _baseDir    = dir; }
    static void setMaxLogSize(int64_t size)  { _maxLogSize = size;}
    static void setBufferSize(int size)      { _bufferSize = size;}
    
private:
    enum LogType { ACTIVE, PERMANENT };
    
    bool logIsOpen() { return _file.is_open(); }
    bool logIsFull() { return (int64_t) _file.tellp() >= _maxLogSize; }
    bool bufferIsFull(packetlen_t x) { return _bPos + x > _bufferSize; }

    string logName(LogType t) const;
    void mkActiveLogNameDirs() const;
    
    void flushBuffer();
    void openLog();
    void closeLog();
    void publish();

private:
    static string  _baseDir;
    static int64_t _maxLogSize;
    static int     _bufferSize;
    static long    _totalArchived;
    
    string  _timestamp;
    string  _sender;     // <hostid>:<port>
    char*   _buffer;
    int32_t _bPos;       // position where to write to buffer
    fstream _file;       // non-published log file

    long _lastActivity; // approx time of last activity

    friend ostream& operator<<(ostream& o, const XrdMonCtrWriter& w);
};

#endif /* XRDMONCTRWRITER_HH */
