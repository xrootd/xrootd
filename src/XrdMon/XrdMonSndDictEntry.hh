/*****************************************************************************/
/*                                                                           */
/*                          XrdMonSndDictEntry.hh                            */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef DICTENTRY_HH
#define DICTENTRY_HH

#include "XrdMon/XrdMonTypes.hh"

#include <iostream>
#include <string>
using std::ostream;
using std::string;

// <user>.<pid>:<fd>@<host>\npath
class XrdMonSndDictEntry {
public:
    struct CompactEntry {
        int32_t id;
        string  others;  // <user>.<pid>:<fd>@<host>\n<path>
        int16_t size() const {return 4 + others.size();}
    };
    
    XrdMonSndDictEntry(string u, 
                       int16_t pid,
                       int16_t fd,
                       string host,
                       string path,
                       int32_t id);

    CompactEntry code();
    
private:
    string  _user;
    int16_t _pid;
    int16_t _fd;
    string  _host;
    string  _path;

    int32_t _myId;

    friend ostream& operator<<(ostream& o, 
                               const XrdMonSndDictEntry& m);
};

#endif /* DICTENTRY_HH */
