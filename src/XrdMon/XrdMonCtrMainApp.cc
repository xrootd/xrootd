/*****************************************************************************/
/*                                                                           */
/*                           XrdMonCtrMainApp.cc                             */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonArgParser.hh"
#include "XrdMon/XrdMonArgParserConvert.hh"
#include "XrdMon/XrdMonTypes.hh"
#include "XrdMon/XrdMonUtils.hh"
#include "XrdMon/XrdMonCtrArchiver.hh"
#include "XrdMon/XrdMonCtrDebug.hh"
#include "XrdMon/XrdMonCtrCollector.hh"
#include "XProtocol/XPtypes.hh"

#include <iostream>
using std::cerr;
using std::cout;
using std::endl;
using namespace XrdMonArgParserConvert;

const char*     defaultCtrLogDir     = "./logs/collector";
const char*     defaultDecLogDir     = "./logs/decoder";
const char*     defaultRtLogDir      = "./logs/rt";
const int       defaultDecFlushDelay = 600;             // [sec]
const bool      defaultRtOn          = false;           // rt off
const kXR_int64 defaultMaxCtrLogSize = 1024*1024*1024;  // 1GB

void
printHelp()
{
    cout << "\nxrdmonCollector\n"
         << "    [-ctrLogDir <path>]\n"
         << "    [-decLogDir <path>]\n"
         << "    [-rtLogDir <path>]\n"
         << "    [-decFlushDelay <value>]\n"
         << "    [-rt <on|off>]\n"
         << "    [-maxCtrLogSize <value>]\n"
         << "\n"
         << "-ctrLogDir <path>       Directory where collector's log file are stored.\n"
         << "                        Default value is \"" << defaultCtrLogDir << "\".\n"
         << "-decLogDir <path>       Directory where decoder's log file are stored.\n"
         << "                        Default value is \"" << defaultDecLogDir << "\".\n"
         << "-rtLogDir <path>        Directory where real time log file are stored.\n"
         << "                        Default value is \"" << defaultRtLogDir << "\".\n"
         << "-decFlushDelay <value>  Value in sec specifying how often data is\n"
         << "                        flushed to collector's log files.\n"
         << "                        Default value is \"" << defaultDecFlushDelay << "\".\n"
         << "-rt <on|off>            Turns on/off real time monitoring.\n"
         << "                        If it is turned off, rtLogDir value is ignored.\n"
         << "                        Default value is \"" << (defaultRtOn?"on":"off") << "\".\n"
         << "-maxCtrLogSize <value>  Max size of collector's log file.\n"
         << "                        Default value is \"" << defaultMaxCtrLogSize << "\".\n"
         << endl;
}

int main(int argc, char* argv[])
{
    XrdMonCtrDebug::initialize();

    XrdMonArgParser::ArgImpl<const char*, Convert2String> 
         arg_ctrLogDir  ("-ctrLogDir", defaultCtrLogDir);
    XrdMonArgParser::ArgImpl<const char*, Convert2String> 
         arg_decLogDir  ("-decLogDir", defaultDecLogDir);
    XrdMonArgParser::ArgImpl<const char*, Convert2String>
         arg_rtLogDir   ("-rtLogDir", defaultRtLogDir);
    XrdMonArgParser::ArgImpl<int, Convert2Int>
         arg_decFlushDel("-decFlushDelay", defaultDecFlushDelay);
    XrdMonArgParser::ArgImpl<bool, ConvertOnOff>
         arg_rtOn       ("-rt", defaultRtOn);
    XrdMonArgParser::ArgImpl<kXR_int64, Convert2LL> 
        arg_maxFSize   ("-maxCtrLogSize", defaultMaxCtrLogSize);

    try {
        XrdMonArgParser argParser;
        argParser.registerExpectedArg(&arg_ctrLogDir);
        argParser.registerExpectedArg(&arg_decLogDir);
        argParser.registerExpectedArg(&arg_rtLogDir);
        argParser.registerExpectedArg(&arg_decFlushDel);
        argParser.registerExpectedArg(&arg_rtOn);
        argParser.registerExpectedArg(&arg_maxFSize);
        argParser.parseArguments(argc, argv);
    } catch (XrdMonException& e) {
        e.printIt();
        printHelp();
        return 1;
    }
    
    cout << "ctrLogDir     is " << arg_ctrLogDir.myVal() << endl;
    cout << "decLogDir     is " << arg_decLogDir.myVal() << endl;
    cout << "rtLogDir      is " << arg_rtLogDir.myVal()  << endl;
    cout << "decFlushDelay is " << arg_decFlushDel.myVal() << endl;
    cout << "rt monitoring is " << (arg_rtOn.myVal()? "on" : "off") << endl;
    cout << "maxCtrLogSize is " << arg_maxFSize.myVal() << endl;

    try {
        mkdirIfNecessary(arg_ctrLogDir.myVal());
        mkdirIfNecessary(arg_decLogDir.myVal());
        if ( arg_rtOn.myVal() ) {
            mkdirIfNecessary(arg_rtLogDir.myVal());
            XrdMonCtrArchiver::_decFlushDelay = arg_decFlushDel.myVal();
        }
    } catch (XrdMonException& e) {
        e.printIt();
        return 2;
    }
    
    // start thread for receiving data
    pthread_t recThread;
    if ( 0 != pthread_create(&recThread, 
                             0, 
                             receivePackets,
                             0) ) {
        cerr << "Failed to create a collector thread" << endl;
        return 1;
    }

    // store received packets until admin packet with sigterm arrives
    XrdMonCtrArchiver archiver(arg_ctrLogDir.myVal(), 
                               arg_decLogDir.myVal(),
                               arg_rtLogDir.myVal(),
                               arg_maxFSize.myVal(), 
                               arg_rtOn.myVal());
    archiver();

    return 0;
}


