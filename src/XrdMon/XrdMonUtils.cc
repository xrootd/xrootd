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

#include "XrdMon/XrdMonException.hh"
#include "XrdMon/XrdMonErrors.hh"
#include "XrdMon/XrdMonUtils.hh"

#include <errno.h>
#include <string.h>     /* strerror */
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
    const time_t sec = tv.tv_sec;
    struct tm *t = localtime(&sec);

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
        throw XrdMonException(ERR_INVALIDADDR, se);
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

    // find non-existing directory in the path, 
    // then create all missing directories
    string org(dir);
    int size = org.size();
    int pos = 0;
    vector<string> dirs2create;
    if ( org[size-1] == '/' ) {
        org = string(org, 0, size-1); // remove '/' at the end
    }
    dirs2create.push_back(org);
    do {
        pos = org.rfind('/', size-1);
        if ( pos == -1 ) {
            break;
        }
        org = string(dir, pos);
        if ( 0 == access(org.c_str(), F_OK) ) {
            break;
        }
        dirs2create.push_back(org);
    } while ( pos > 0 );

    size = dirs2create.size();
    for ( int i=size-1 ; i>=0 ; --i ) {
        const char*d = dirs2create[i].c_str();
        if ( 0 != mkdir(d, 0x3FD) ) {
            ostringstream se;
            se << "Failed to mkdir " << dir << ". "
               << "Error '" << strerror (errno) << "'";
            throw XrdMonException(ERR_NODIR, se.str());
        }
        cout << "mkdir " << d << " OK" << endl;
    }
}

