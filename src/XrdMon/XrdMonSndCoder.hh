/*****************************************************************************/
/*                                                                           */
/*                            XrdMonSndCoder.hh                              */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONSNDCODER_HH
#define XRDMONSNDCODER_HH

#include "XrdMon/XrdMonCommon.hh"
#include "XrdMon/XrdMonSndAdminEntry.hh"
#include "XrdMon/XrdMonSndDebug.hh"
#include "XrdMon/XrdMonSndDictEntry.hh"
#include "XrdMon/XrdMonSndPacket.hh"
#include "XrdMon/XrdMonSndTraceEntry.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include <iostream>
#include <netinet/in.h>
#include <utility> // for pair
#include <vector>
using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::pair;
using std::vector;

// The class responsible for coding data into a binary packet

class XrdMonSndCoder {
public:
    XrdMonSndCoder();

    int prepare2Transfer(const XrdMonSndAdminEntry& ae);
    int prepare2Transfer(const vector<XrdMonSndTraceEntry>& vector);
    int prepare2Transfer(const vector<int32_t>& vector);
    int prepare2Transfer(const XrdMonSndDictEntry::CompactEntry& ce);

    const XrdMonSndPacket& packet() { return _packet; }
    void reset() { _packet.reset(); }
    void printStats() const ;
    
private:
    char* writeHere() { return _packet.offset(_putOffset); }
    int reinitXrdMonSndPacket(packetlen_t newSize, char packetCode);
    pair<char, uint32_t> generateBigNumber(const char* descr);
    
    inline void add_int08_t(int8_t value) {
        memcpy(writeHere(), &value, sizeof(int8_t));
        if ( XrdMonSndDebug::verbose(XrdMonSndDebug::SPacket) ) {
            cout << "stored int08_t value " << (int) value 
                 << ", _putOffset " << _putOffset << endl;
        }
        _putOffset += sizeof(int8_t);
    }
    inline void add_int16_t(int16_t value) {
        int16_t v = htons(value);
        memcpy(writeHere(), &v, sizeof(int16_t));
        if ( XrdMonSndDebug::verbose(XrdMonSndDebug::SPacket) ) {
            cout << "stored int16_t value " << value 
                 << ", _putOffset " << _putOffset << endl;
        }
        _putOffset += sizeof(int16_t);
    }
    inline void add_uint16_t(uint16_t value) {
        uint16_t v = htons(value);
        memcpy(writeHere(), &v, sizeof(uint16_t));
        if ( XrdMonSndDebug::verbose(XrdMonSndDebug::SPacket) ) {
            cout << "stored uint16_t value " << value 
                 << ", _putOffset " << _putOffset << endl;
        }
        _putOffset += sizeof(uint16_t);
    }
    inline void add_int32_t(int32_t value) {
        int32_t v = htonl(value);
        memcpy(writeHere(), &v, sizeof(int32_t));
        if ( XrdMonSndDebug::verbose(XrdMonSndDebug::SPacket) ) {
            cout << "stored int32_t value " << value 
                 << ", _putOffset " << _putOffset << endl;
        }
        _putOffset += sizeof(int32_t);
    }
    inline void add_uint32_t(uint32_t value) {
        uint32_t v = htonl(value);
        memcpy(writeHere(), &v, sizeof(uint32_t));
        if ( XrdMonSndDebug::verbose(XrdMonSndDebug::SPacket) ) {
            cout << "stored uint32_t value " << value 
                 << ", _putOffset " << _putOffset << endl;
        }
        _putOffset += sizeof(uint32_t);
    }
    inline void add_int64_t(int64_t value) {
        int64_t v = htonll(value);
        memcpy(writeHere(), &v, sizeof(int64_t));
        if ( XrdMonSndDebug::verbose(XrdMonSndDebug::SPacket) ) {
            cout << "stored int64_t value " << value 
                 << ", _putOffset " << _putOffset << endl;
        }
        _putOffset += sizeof(int64_t);
    }
    inline void add_Mark(char mark, int noChars=8) {
        assert(noChars<=8);
        char x[8];
        memset(x, 0, 8);
        x[0] = mark;
        memcpy(writeHere(), x, 1);
        if ( XrdMonSndDebug::verbose(XrdMonSndDebug::SPacket) ) {
            cout << "stored mark " << mark 
                 << ", _putOffset " << _putOffset << endl;
        }

        _putOffset += noChars;
    }
    inline void add_string(const string& s) {
        int16_t sLen = s.size();
        if ( 0 == sLen ) {
            cerr << "Error in add_string, size 0" << endl;
            return;
        }
        memcpy(writeHere(), s.c_str(), sLen);
        if ( XrdMonSndDebug::verbose(XrdMonSndDebug::SPacket) ) {
            cout << "stored string " << s 
                 << ", _putOffset " << _putOffset << endl;
        }
        _putOffset += sLen;
    }

private:
    XrdMonSndPacket  _packet;
    int32_t _putOffset; // tracks where to write inside packet
    sequen_t _sequenceNo;

    static time_t _serverStartTime;
    
    // statistics
    int32_t _noDict;
    int32_t _noOpen;
    int32_t _noClose;
    int32_t _noTrace;
    int32_t _noTime;
};

#endif /* XRDMONSNDCODER_HH */
