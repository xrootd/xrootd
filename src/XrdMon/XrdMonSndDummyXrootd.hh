/*****************************************************************************/
/*                                                                           */
/*                         XrdMonSndDummyXrootd.hh                           */
/*                                                                           */
/* (c) 2005 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#ifndef XRDMONSNDDUMMYXROOTD_HH
#define XRDMONSNDDUMMYXROOTD_HH

#include <vector>
#include <string>
#include "XrdMon/XrdMonTypes.hh"
#include "XrdMon/XrdMonSndTraceEntry.hh"
#include "XrdMon/XrdMonSndDictEntry.hh"
using std::vector;
using std::string;

class XrdMonSndDummyXrootd {
public:
    static int16_t NEWUSERFREQUENCY;
    static int16_t NEWPROCFREQUENCY;
    static int16_t NEWFILEFREQUENCY;
    static int16_t MAXHOSTS;
    
    XrdMonSndDummyXrootd();
    ~XrdMonSndDummyXrootd();

    int initialize(const char* pathFile);
    XrdMonSndDictEntry newXrdMonSndDictEntry();
    XrdMonSndTraceEntry newXrdMonSndTraceEntry();
    int32_t closeOneFile();
    void closeFiles(vector<int32_t>& closedFiles);
    
private:
    int readPaths(const char* pathFile);
    void createUser();
    void createProcess();
    void createFile();
    string generateUserName(int16_t uid);
    string generateHostName();
    
    struct User {
        struct HostAndPid {
            string name;
            int16_t pid;
            vector<int16_t> myFiles; // offsets in _paths vector
            HostAndPid(string n, int16_t id) 
                : name(n), pid(id) {};
        };

        int16_t uid;
        vector<HostAndPid> myProcesses;
        User(int16_t id) : uid(id) {}
    };

    vector<User> _users;

    int32_t _noCalls2NewUser;
    int32_t _noCalls2NewProc;
    int32_t _noCalls2NewFile;

    int16_t _activeUser;
    int16_t _activeProcess;
    int16_t _activeFile;
    bool    _newFile;

    struct PathData {
        string path;
        int16_t fd;
        PathData(const char* s, int16_t id) : path(s), fd(id) {}
    };

    // input data to pick from, loaded from ascii file
    // Yes, this might be a lot of memory
    vector<PathData> _paths;

    int32_t _firstAvailId;
    vector<uint32_t> _noTracesPerDict;

    vector<bool> _openFiles; // true: open, false: close
};

#endif /* XRDMONSNDDUMMYXROOTD_HH */
