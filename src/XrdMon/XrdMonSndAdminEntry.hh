/*****************************************************************************/
/*                                                                           */
/*                          XrdMonSndAdminEntry.hh                           */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef ADMINENTRY_HH
#define ADMINENTRY_HH

#include "XrdMon/XrdMonCommon.hh"
#include "XrdMon/XrdMonTypes.hh"

class XrdMonSndAdminEntry {
public:    
    void setShutdown() {
        _command = c_shutdown;
        _arg = 0;
    }
    int16_t size() const         { return 2*sizeof(int16_t); }
    AdminCommand command() const { return _command; }
    int16_t arg() const          { return _arg; }
    
private:
    AdminCommand _command;
    int16_t _arg;
};

#endif /* ADMINENTRY_HH */
