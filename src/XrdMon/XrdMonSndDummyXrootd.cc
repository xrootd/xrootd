/*****************************************************************************/
/*                                                                           */
/*                         XrdMonSndDummyXrootd.cc                           */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonSndDummyXrootd.hh"
#include "XrdMon/XrdMonSndDebug.hh"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::setfill;
using std::setw;
using std::stringstream;

// create new user/process/file every ... calls
int16_t XrdMonSndDummyXrootd::NEWUSERFREQUENCY = 1;
int16_t XrdMonSndDummyXrootd::NEWPROCFREQUENCY = 1;
int16_t XrdMonSndDummyXrootd::NEWFILEFREQUENCY = 1;

int16_t XrdMonSndDummyXrootd::MAXHOSTS         = 20;

XrdMonSndDummyXrootd::XrdMonSndDummyXrootd()
    : _noCalls2NewUser(1), 
      _noCalls2NewProc(0),
      _noCalls2NewFile(0),
      _firstAvailId(0)
{}

XrdMonSndDummyXrootd::~XrdMonSndDummyXrootd()
{
    cout << "this is the mapping:\n";
    int i, size = _noTracesPerDict.size();
    for (i=0 ; i<size ; i++) {
        cout << setw(9) << setfill('0') << i << "  --> " 
             << setw(4) << setfill('0') << _noTracesPerDict[i] << " entries" << endl;
    }
}

int
XrdMonSndDummyXrootd::initialize(const char* pathFile) 
{
    return readPaths(pathFile);
}

XrdMonSndDictEntry
XrdMonSndDummyXrootd::newXrdMonSndDictEntry()
{
    --_noCalls2NewUser;
    --_noCalls2NewProc;
    
    if ( _noCalls2NewUser == 0 ) { createUser();    }
    if ( _noCalls2NewProc == 0 ) { createProcess(); }
    createFile();

    User& user     = _users[_activeUser];
    User::HostAndPid& hp = user.myProcesses[_activeProcess];
    PathData& pd   = _paths[ hp.myFiles[_activeFile] ];


    XrdMonSndDictEntry e(generateUserName(user.uid), 
                hp.pid, 
                pd.fd, 
                hp.name, 
                pd.path, 
                _firstAvailId++);
    _noTracesPerDict.push_back(0);
    _openFiles.push_back(true);
    
    return e;
}

XrdMonSndTraceEntry
XrdMonSndDummyXrootd::newXrdMonSndTraceEntry()
{
    // make sure it is sometimes>2GB
    int64_t offset = ((uint64_t) rand())*(rand()%512);
    int32_t length = 16384;
    int32_t id = (uint32_t) rand() % _firstAvailId; // do this until finds still open file

    XrdMonSndTraceEntry d(offset, length, id);
    ++(_noTracesPerDict[id]);
    return d;
}

int32_t
XrdMonSndDummyXrootd::closeOneFile()
{
    while ( 1 ) {
        int32_t id = (uint32_t) rand() % _firstAvailId;
        if ( _openFiles[id] ) {
            _openFiles[id] = false;
            return id;
        }
    }
    return 0;
}

void 
XrdMonSndDummyXrootd::closeFiles(vector<int32_t>& closedFiles)
{
    for (int32_t i=0 ; i<_firstAvailId ; i++) {
        closedFiles.push_back(i);
    }
}

int
XrdMonSndDummyXrootd::readPaths(const char* pathFile)
{
    ifstream f(pathFile);
    if ( ! f ) {
        cerr << "Error opening file " << pathFile << endl;
        return 1;
    }
    
    int16_t fd = 100;
    while ( f ) {
        char buffer[256];
        f >> buffer;
        _paths.push_back( PathData(buffer, fd++) );
    }
    f.close();

    return 0;
}

void
XrdMonSndDummyXrootd::createUser()
{
    _noCalls2NewUser = NEWUSERFREQUENCY;
    _noCalls2NewProc = 0; // force creation of new process

    // generate new user
    int16_t newid = 1 + (uint16_t) rand() % 5000; // user id range: 0-5000
    // check if it already exists
    int i, s = _users.size();
    for ( i=0 ; i<s ; i++ ) {
        if ( _users[i].uid == newid ) {
            if ( XrdMonSndDebug::verbose(XrdMonSndDebug::Generator) ) {
                cout << "Generated uid that already exists" << endl;
            }
            _activeUser = i;
            return;
        }
    }
    // no it does not, so register it and use it
    _users.push_back( User(newid) );
    _activeUser = _users.size() - 1;
    if ( XrdMonSndDebug::verbose(XrdMonSndDebug::Generator) ) {
        cout << "Created new user, offset " << _activeUser
             << ", uid " << newid << endl;
    }
}


void
XrdMonSndDummyXrootd::createProcess()
{
    _noCalls2NewProc = NEWPROCFREQUENCY;
    _noCalls2NewFile = 0; // force creation of new file

    User& user = _users[_activeUser];
    
    // generate new process
    int16_t newid = 1 + (uint16_t) rand() % 10000; // pid range: 0-10000
    string newhost = generateHostName();
    // check if it already exists
    int i, s = user.myProcesses.size();
    for ( i=0 ; i<s ; i++ ) {
        if ( user.myProcesses[i].pid  == newid && 
             user.myProcesses[i].name == newhost ) {
            if ( XrdMonSndDebug::verbose(XrdMonSndDebug::Generator) ) {
                cout << "Generated host:pid that already exists" << endl;
            }
            _activeProcess = i;
            return;
        }
    }
    // no it does not, so register it and use it
    user.myProcesses.push_back(User::HostAndPid(newhost, newid));
    _activeProcess = user.myProcesses.size() - 1;
    if ( XrdMonSndDebug::verbose(XrdMonSndDebug::Generator) ) {
        cout << "Created new process, offset " << _activeProcess
             << ", " << newhost << ":" << newid << endl;
    }
}

void
XrdMonSndDummyXrootd::createFile()
{
    _noCalls2NewFile = NEWFILEFREQUENCY;

    User& user = _users[_activeUser];
    vector<User::HostAndPid>& myProcesses = user.myProcesses;
    User::HostAndPid& hp = myProcesses[_activeProcess];
    vector<int16_t>& myFiles = hp.myFiles;

    // open new file
    int16_t newOffset = (uint16_t) rand() % _paths.size();
    // check if it is already open
    int i, s = myFiles.size();
    for ( i=0 ; i<s ; i++ ) {
        if ( myFiles[i] == newOffset ) {
            if ( XrdMonSndDebug::verbose(XrdMonSndDebug::Generator) ) {
                cout << "Generated file descr that already exists" << endl;
            }
            _activeFile = i;
            return;
        }
    }
    // no it does not, so register it and use it
    myFiles.push_back(newOffset);
    _activeFile = myFiles.size() - 1;
    _newFile = true;
    if ( XrdMonSndDebug::verbose(XrdMonSndDebug::Generator) ) {
        PathData &pd = _paths[myFiles[_activeFile]];
        cout << "Created new file " << pd.path
             << " -> " << pd.fd << endl;
    }
}

string
XrdMonSndDummyXrootd::generateHostName()
{
    vector<string> type;
    type.push_back("barb");
    type.push_back("noma");
    type.push_back("tori");
    type.push_back("bronco");

    uint16_t t = (uint16_t) rand() % 3;
    uint16_t x = (uint16_t) rand() % MAXHOSTS;
    stringstream ss(stringstream::out);
    ss << type[t] << setw(4) << setfill('0') << x << ".slac.stanford.edu";

    return ss.str();
}

string
XrdMonSndDummyXrootd::generateUserName(int16_t uid)
{
    stringstream ss;
    ss << "a_" << uid << "_User";
    return ss.str();
}

