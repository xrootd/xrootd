/*****************************************************************************/
/*                                                                           */
/*                            XrdMonCtrWriter.cc                             */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonException.hh"
#include "XrdMon/XrdMonCommon.hh"
#include "XrdMon/XrdMonHeader.hh"
#include "XrdMon/XrdMonUtils.hh"
#include "XrdMon/XrdMonCtrDebug.hh"
#include "XrdMon/XrdMonErrors.hh"
#include "XrdMon/XrdMonCtrMutexLocker.hh"
#include "XrdMon/XrdMonCtrWriter.hh"

#include <assert.h>
#include <netinet/in.h> /* ntohs  */
#include <stdio.h>      /* rename */
#include <unistd.h>     /* F_OK   */

#include <iomanip>
#include <iostream>
#include <sstream>
using std::cout;
using std::endl;
using std::ios;
using std::setw;
using std::stringstream;

string  XrdMonCtrWriter::_baseDir;
kXR_int64 XrdMonCtrWriter::_maxLogSize(1024*1024*1024); // 1GB
long    XrdMonCtrWriter::_totalArchived(0);

// never make it < MAXPACKETSIZE
kXR_int32 XrdMonCtrWriter::_bufferSize(1024*1024);      // 1MB 

XrdMonCtrWriter::XrdMonCtrWriter(const char* senderHP)
    : _buffer(0),
      _bPos(0),
      _lastActivity(0)
{
    assert(_bufferSize > 0);

    _timestamp = generateTimestamp();
    
    stringstream ss(stringstream::out);
    ss << senderHP;
    _sender = ss.str();
}

XrdMonCtrWriter::~XrdMonCtrWriter()
{
    flushBuffer();
    closeLog();
    publish();
    delete [] _buffer;
}

void
XrdMonCtrWriter::operator()(const char* packet, 
                            const XrdMonHeader& header, 
                            long currentTime)
{
    _lastActivity = currentTime;
    
    // initialize buffer
    if ( 0 == _buffer ) {
        _buffer = new char [_bufferSize];
        if ( 0 == _buffer ) {
            throw XrdMonException(ERR_NOMEM,
               "Unable to allocate buffer - run out of memory");
        }
    }

    // check if there is space in buffer
    // if not, flush to log file
    if ( bufferIsFull(header.packetLen()) ) {
        //cout << "flushing buffer for " << _sender << endl;
        flushBuffer();
    }

    // write packet to buffer
    memcpy(_buffer+_bPos, packet, header.packetLen());
    _bPos += header.packetLen();

    if ( XrdMonCtrDebug::verbose(XrdMonCtrDebug::Receiving) ) {
        XrdOucMutexHelper mh; mh.Lock(&XrdMonCtrDebug::_mutex);
        cout << " Archiving packet #" << setw(5) << ++_totalArchived 
             << " from " << _sender << " " << header << endl;
    }
}

void
XrdMonCtrWriter::forceClose()
{
    cout <<"forceClose not implemented" << endl;
}

// ===========================================
// =========== private below =================
// ===========================================

void
XrdMonCtrWriter::flushBuffer()
{
    if ( _bPos == 0 ) {
        return;
    }
    
    if ( !logIsOpen() ) {
        openLog();
    }
    
    if ( logIsFull() ) {
        closeLog();
        publish();
        openLog();
    }

    _file.write(_buffer, _bPos);

    memset(_buffer, 0, _bufferSize);
    _bPos = 0;
}
void
XrdMonCtrWriter::mkActiveLogNameDirs() const
{
    string ss(_baseDir);
    ss += '/';    
    pair<string, string> hp = breakHostPort(_sender);
    ss += hp.first;
    mkdirIfNecessary(ss.c_str());
    ss += '/';
    ss += hp.second;
    mkdirIfNecessary(ss.c_str());
}

string
XrdMonCtrWriter::logName(LogType t) const
{
    string ss(_baseDir);
    ss += '/';    
    pair<string, string> hp = breakHostPort(_sender);
    ss += hp.first;
    ss += '/';
    ss += hp.second;
    ss += '/';
    if ( t == ACTIVE ) {
        ss += "active";
    } else if ( t == PERMANENT ) {
        ss += _timestamp;
        ss += '_';
        ss += _sender;
    } else {
        throw XrdMonException(ERR_INVALIDARG, "in XrdMonCtrWriter::logName");
    }
    ss += ".rcv";

    return ss;
}

void
XrdMonCtrWriter::openLog()
{
    mkActiveLogNameDirs();
    _file.open(logName(ACTIVE).c_str(), ios::out|ios::binary|ios::ate);
}

void
XrdMonCtrWriter::closeLog()
{
    if ( _file.is_open() ) {
        _file.close();
    }
}

void
XrdMonCtrWriter::publish()
{
    string src = logName(ACTIVE);

    if ( 0 == _bPos && 0 != access(src.c_str(), F_OK) ) {
        return;
    }

    string dest = logName(PERMANENT);
    if ( 0 != rename(src.c_str(), dest.c_str()) ) {
        string ss("Cannot rename "); ss += src;
        ss += " to "; ss += dest;
        throw XrdMonException(ERR_RENAME, ss);
    }
    _timestamp = generateTimestamp();
}

ostream&
operator<<(ostream& o, const XrdMonCtrWriter& w)
{
    o << w._sender;
    return o;
}
