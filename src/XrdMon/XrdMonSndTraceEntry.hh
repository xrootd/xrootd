/*****************************************************************************/
/*                                                                           */
/*                          XrdMonSndTraceEntry.hh                           */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONSNDTRACEENTRY_HH
#define XRDMONSNDTRACEENTRY_HH

#include "XrdMon/XrdMonTypes.hh"
#include <iostream>
using std::ostream;

class XrdMonSndTraceEntry {
public:
    XrdMonSndTraceEntry(int64_t offset,
               int32_t  length,
               int32_t id);

    int64_t offset() const  { return _offset; }
    int32_t length() const  { return _length; }
    int32_t id()     const  { return _id;     }
    
private:
    int64_t _offset;
    int32_t _length;
    int32_t _id;

    friend ostream& operator<<(ostream& o, 
                               const XrdMonSndTraceEntry& m);
};

#endif /* XRDMONSNDTRACEENTRY_HH */
