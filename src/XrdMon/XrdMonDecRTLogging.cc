/*****************************************************************************/
/*                                                                           */
/*                           XrdMonDecRTLogging.cc                           */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMonDecRTLogging.hh"
#include <fstream>
#include <iostream>
using std::cout;
using std::endl;
using std::fstream;
using std::ios;


XrdMonDecRTLogging::XrdMonDecRTLogging(const char* dir, string& senderHost)
    : _senderHost(senderHost),
      _buf(0), 
      _bufSize(1024*1024)
{
    _rtLog = dir;
    _rtLog += "/realTimeLogging.txt";

    _buf = new char [_bufSize];
    strcpy(_buf, "");
}

XrdMonDecRTLogging::~XrdMonDecRTLogging()
{
    delete [] _buf;
}

void
XrdMonDecRTLogging::add(XrdMonDecUserInfo::TYPE t, XrdMonDecUserInfo* x)
{
    XrdOucMutexHelper mh; mh.Lock(&_mutex);

    const char* s = x->writeRT2Buffer(t, _senderHost);
    if ( static_cast<int>(strlen(_buf) + strlen(s)) >= _bufSize ) {
        flush(false); // false -> don't lock mutex, already locked
    }
    strcat(_buf, s);
}

void
XrdMonDecRTLogging::add(XrdMonDecDictInfo::TYPE t, XrdMonDecDictInfo* x)
{
    XrdOucMutexHelper mh; mh.Lock(&_mutex);

    const char* s = x->writeRT2Buffer(t, _senderHost);
    if ( static_cast<int>(strlen(_buf) + strlen(s)) >= _bufSize ) {
        flush(false); // false -> don't lock mutex, already locked
    }
    strcat(_buf, s);
}

void
XrdMonDecRTLogging::flush(bool lockIt)
{
    cout << "Flushing RT data..." << endl;

    fstream f(_rtLog.c_str(), ios::out|ios::app);

    XrdOucMutexHelper mh;
    if ( lockIt ) {
        mh.Lock(&_mutex);
    }

    f << _buf;
    strcpy(_buf, "");

    f.close();
}

