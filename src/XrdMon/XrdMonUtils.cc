/*****************************************************************************/
/*                                                                           */
/*                              XrdMonUtils.cc                               */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonAPException.hh"
#include "XrdMon/XrdMonErrors.hh"
#include "XrdMon/XrdMonUtils.hh"

#include <stdio.h>
#include <sys/stat.h>   /* mkdir  */
#include <sys/time.h>
#include <sys/types.h>  /* mkdir  */
#include <unistd.h>     /* access */

#include <iomanip>
#include <iostream>
#include <sstream>
using std::cout;
using std::endl;
using std::setfill;
using std::setw;
using std::ostringstream;

string
generateTimestamp()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    struct tm *t = localtime(&(tv.tv_sec));

    ostringstream s;
    // YYMM_DD_HH::MM::SS.MMM
    s << setw(2) << setfill('0') << t->tm_year+1900
      << setw(2) << setfill('0') << t->tm_mon+1
      << setw(2) << setfill('0') << t->tm_mday      << '_'
      << setw(2) << setfill('0') << t->tm_hour      << ':' 
      << setw(2) << setfill('0') << t->tm_min       << ':'
      << setw(2) << setfill('0') << t->tm_sec       << '.'
      << setw(3) << setfill('0') << tv.tv_usec/1000;

    return s.str();
}

string
timestamp2string(time_t timestamp)
{
    if ( 0 == timestamp ) {
        return string("0000-00-00 00:00:00");
    }
    struct tm *t = localtime(&timestamp);
    ostringstream ss;

    // Format: YYYY-MM-DD HH:MM:SS
    ss << setw(4) << setfill('0') << t->tm_year+1900 << '-'
       << setw(2) << setfill('0') << t->tm_mon+1     << '-'
       << setw(2) << setfill('0') << t->tm_mday      << ' '
       << setw(2) << setfill('0') << t->tm_hour      << ':'
       << setw(2) << setfill('0') << t->tm_min       << ':'
       << setw(2) << setfill('0') << t->tm_sec;
    return ss.str();
}

void
timestamp2string(time_t timestamp, char s[24])
{
    if ( 0 == timestamp ) {
        strcpy(s, "0000-00-00 00:00:00");
        return;
    }

    struct tm *t = localtime(&timestamp);
    
    // Format: YYYY-MM-DD HH:MM:SS
    sprintf(s, "%4d-%02d-%02d %02d:%02d:%02d",
            t->tm_year+1900,
            t->tm_mon+1,
            t->tm_mday,
            t->tm_hour,
            t->tm_min,
            t->tm_sec);
}

// converts string host:port to a pair<host, port>
pair<string, string>
breakHostPort(const string& hp)
{
    int colonPos = hp.rfind(':', hp.size());
    if ( colonPos == -1 ) {
        string se("No : in "); se += hp;
        throw XrdMonAPException(ERR_INVALIDADDR, se);
    }
    string host(hp, 0, colonPos);
    string port(hp, colonPos+1, hp.size()-colonPos-1);
    return pair<string, string>(host, port);
}

void
mkdirIfNecessary(const char* dir)
{
    if ( 0 == access(dir, F_OK) ) {
        return;
    }
    int ret = mkdir(dir, 0x3FD);
    if ( 0 != ret ) {
        ostringstream se;
        se << "Failed to mkdir " << dir << ", ret error " << ret;
        throw XrdMonAPException(ERR_NODIR, se.str());
    }
    cout << "mkdir " << dir << " OK" << endl;
}

