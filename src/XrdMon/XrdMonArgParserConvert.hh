/*****************************************************************************/
/*                                                                           */
/*                        XrdMonArgParserConvert.hh                          */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonException.hh"
#include "XrdMon/XrdMonErrors.hh"
#include <sstream>
#include <stdlib.h> /* atoi */
using std::stringstream;

namespace XrdMonArgParserConvert 
{
    struct Convert2String {
        static const char* convert(const char* s) {
            return s;
        }
    };

    struct Convert2Int {
        static int convert(const char* s) {
            return atoi(s);
        }
    };

    struct ConvertOnOff {
        static bool convert(const char* s) {
            if ( 0 == strcasecmp(s, "on") ) {
                return true;
            }
            if ( 0 == strcasecmp(s, "off") ) {
                return false;
            }
            stringstream ss(stringstream::out);
            ss << "Expected 'on' or 'off', found " << s;
            throw XrdMonException(ERR_INVALIDARG, ss.str());
            return false;
        }
    };    
}

