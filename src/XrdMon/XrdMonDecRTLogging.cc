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
#include <fcntl.h>
#include <strings.h> /* bcopy */
#include <sys/stat.h>
#include <fstream>
#include <iostream>
#include <stdio.h>
using std::cout;
using std::endl;
using std::fstream;
using std::ios;


XrdMonDecRTLogging::XrdMonDecRTLogging(const char* dir, int rtBufSize)
    : _buf(0), 
      _bufSize(rtBufSize)
{
    _rtLog = new char[strlen(dir) + 24];
    sprintf(_rtLog, "%s/realTimeLogging.txt", dir);

    _rtLogLock = new char [strlen(dir) + 32];
    sprintf(_rtLogLock, "%s.lock", _rtLog);

    _buf = new char [_bufSize];
    strcpy(_buf, "");
}

XrdMonDecRTLogging::~XrdMonDecRTLogging()
{
    delete [] _rtLog;
    delete [] _rtLogLock;
    delete [] _buf;
}

void
XrdMonDecRTLogging::add(XrdMonDecUserInfo::TYPE t, XrdMonDecUserInfo* x)
{
    XrdOucMutexHelper mh; mh.Lock(&_mutex);

    const char* s = x->writeRT2Buffer(t);
    if ( static_cast<int>(strlen(_buf) + strlen(s)) >= _bufSize ) {
        flush(false); // false -> don't lock mutex, already locked
    }
    strcat(_buf, s);
}

void
XrdMonDecRTLogging::add(XrdMonDecDictInfo::TYPE t, XrdMonDecDictInfo* x)
{
    XrdOucMutexHelper mh; mh.Lock(&_mutex);

    const char* s = x->writeRT2Buffer(t);
    if ( static_cast<int>(strlen(_buf) + strlen(s)) >= _bufSize ) {
        flush(false); // false -> don't lock mutex, already locked
    }
    strcat(_buf, s);
}

void
XrdMonDecRTLogging::flush(bool lockIt)
{
    // get the lock, wait if necessary
    struct flock lock_args;
    bzero(&lock_args, sizeof(lock_args));

    mode_t m = S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH;

    cout << "RT locking." << std::flush;
    int fLock = open(_rtLogLock, O_WRONLY|O_CREAT, m);
    lock_args.l_type = F_WRLCK;
    fcntl(fLock, F_SETLKW, &lock_args);    
    cout << "ok." << std::flush;

    // open rt log, write to it, and close it
    int f = open(_rtLog, O_WRONLY|O_CREAT|O_APPEND,m);

    int s = strlen(_buf);
    if ( s > 0 ) {        
        XrdOucMutexHelper mh;
        if ( lockIt ) {
            mh.Lock(&_mutex);
        }
        write(f, _buf, strlen(_buf));
        strcpy(_buf, "");
    }
    cout << s;
    close(f);

    // unlock
    bzero(&lock_args, sizeof(lock_args));
    lock_args.l_type = F_UNLCK;
    fcntl(fLock, F_SETLKW, &lock_args);
    close (fLock);
    
    cout << ".unlocked" << endl;
}

