/*****************************************************************************/
/*                                                                           */
/*                             XrdMonDecSink.cc                              */
/*                                                                           */
/* (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University */
/*                            All Rights Reserved                            */
/*       Produced by Jacek Becla for Stanford University under contract      */
/*              DE-AC02-76SF00515 with the Department of Energy              */
/*****************************************************************************/

// $Id$

#include "XrdMon/XrdMonException.hh"
#include "XrdMon/XrdMonUtils.hh"
#include "XrdMon/XrdMonErrors.hh"
#include "XrdMon/XrdMonSenderInfo.hh"
#include "XrdMon/XrdMonDecSink.hh"
#include "XrdMon/XrdMonDecTraceInfo.hh"
#include <netinet/in.h>
#include <sstream>
#include <sys/time.h> // FIXME - remove when xrootd supports openfile
#include <iomanip>
#include <unistd.h>
using std::cerr;
using std::cout;
using std::endl;
using std::ios;
using std::map;
using std::setw;
using std::setfill;
using std::stringstream;

XrdMonDecSink::XrdMonDecSink(const char* baseDir,
                             const char* rtLogDir,
                             bool saveTraces,
                             int maxTraceLogSize)
    : _rtOn(rtLogDir != 0),
      _saveTraces(saveTraces),
      _tCacheSize(32*1024), // 32*1024 * 32 bytes = 1 MB FIXME-configurable?
      _traceLogNumber(0),
      _maxTraceLogSize(maxTraceLogSize),
      _lastSeq(0xFF),
      _uniqueDictId(1),
      _uniqueUserId(1),
      _senderId(65500) // make it invalid, so that _senderHost is initialized
{
    if ( maxTraceLogSize < 2  ) {
        cerr << "Trace log size must be > 2MB" << endl;
        throw XrdMonException(ERR_INVALIDARG, "Trace log size must be > 2MB");
    }

    _path = baseDir;
    _path += "/";
    _jnlPath = _path + "/jnl";
    _path += generateTimestamp();
    _path += "_";
    _dictPath = _path + "dict.ascii";
    _userPath = _path + "user.ascii";

    if ( 0 == access(_dictPath.c_str(), F_OK) ) {
        string s("File "); s += _dictPath;
        s += " exists. Move it somewhere else first.";
        throw XrdMonException(ERR_INVALIDARG, s);
    }
    if ( _saveTraces ) {
        _tCache.reserve(_tCacheSize+1);
        string fTPath = _path + "trace000.ascii";
        if ( 0 == access(fTPath.c_str(), F_OK) ) {
            string s("File "); s += fTPath;
            s += " exists. Move it somewhere else first.";
            throw XrdMonException(ERR_INVALIDARG, s);
        }
    }

    if ( _rtOn ) {
        _rtLogDir = rtLogDir;
        _rtLogDir += "/realTimeLogging.txt";
    } else {
        loadUniqueIdsAndSeq();
    }
}

XrdMonDecSink::~XrdMonDecSink()
{
    reset();    
}

void 
XrdMonDecSink::setSenderId(kXR_unt16 id)
{
    if ( id != _senderId ) {
        string hostPort ( XrdMonSenderInfo::hostPort(id) );
        pair<string, string> hp = breakHostPort(hostPort);
        _senderHost = hp.first;
        _senderId = id;
    }
}

struct connectDictIdsWithCache : public std::unary_function<XrdMonDecDictInfo*, void> {
    connectDictIdsWithCache(map<dictid_t, XrdMonDecDictInfo*>& dC) : _cache(dC){}
    void operator()(XrdMonDecDictInfo* di) {
        dictid_t id = di->xrdId();
        _cache[id] = di;
    }
    map<dictid_t, XrdMonDecDictInfo*>& _cache;
};

void
XrdMonDecSink::init(dictid_t min, dictid_t max, const string& senderHP)
{
    // read jnl file, create vector<XrdMonDecDictInfo*> of active 
    // XrdMonDecDictInfo objects
    vector<XrdMonDecDictInfo*> diVector = loadActiveDictInfo();

    // connect active XrdMonDecDictInfo objects to the cache
    std::for_each(diVector.begin(),
                  diVector.end(),
                  connectDictIdsWithCache(_dCache));

    pair<string, string> hp = breakHostPort(senderHP);
    _senderHost = hp.first;
}

void
XrdMonDecSink::addDictId(dictid_t xrdId, const char* theString, int len)
{
    XrdOucMutexHelper mh; mh.Lock(&_dMutex);
    std::map<dictid_t, XrdMonDecDictInfo*>::iterator itr = _dCache.find(xrdId);
    if ( itr != _dCache.end() ) {
        stringstream se;
        se << "DictID already in cache " << xrdId;
        throw XrdMonException(ERR_DICTIDINCACHE, se.str());
    }
    
    XrdMonDecDictInfo* di;
    _dCache[xrdId] = di = new XrdMonDecDictInfo(xrdId, _uniqueDictId++, 
                                                theString, len);
    
    cout << "Added dictInfo to sink: " << *di << endl;

    // FIXME: remove this line when xrootd supports openFile
    // struct timeval tv; gettimeofday(&tv, 0); openFile(xrdId, tv.tv_sec-8640000);
}

void
XrdMonDecSink::addUserId(dictid_t usrId, const char* theString, int len)
{
    XrdOucMutexHelper mh; mh.Lock(&_uMutex);
    std::map<dictid_t, XrdMonDecUserInfo*>::iterator itr = _uCache.find(usrId);
    if ( itr != _uCache.end() ) {
        stringstream se;
        se << "UserID already in cache " << usrId;
        throw XrdMonException(ERR_USERIDINCACHE, se.str());
    }
    
    XrdMonDecUserInfo* ui;
    _uCache[usrId] = ui = new XrdMonDecUserInfo(usrId, _uniqueUserId++, 
                                                theString, len);
    cout << "Added userInfo to sink: " << *ui << endl;

    if ( _rtOn ) {
        XrdOucMutexHelper mh; mh.Lock(&_urtMutex);
        _rtUCache.push_back(pair<OC, XrdMonDecUserInfo*>(OPEN, ui));
    }
}

void
XrdMonDecSink::add(dictid_t xrdId, XrdMonDecTraceInfo& trace)
{
    static long totalNoTraces = 0;
    static long noLostTraces  = 0;
    if ( ++totalNoTraces % 500001 == 500000 ) {
        cout << noLostTraces << " lost since last time" << endl;
        noLostTraces = 0;
    }

    XrdOucMutexHelper mh; mh.Lock(&_dMutex);
    std::map<dictid_t, XrdMonDecDictInfo*>::iterator itr = _dCache.find(xrdId);
    if ( itr == _dCache.end() ) {
        registerLostPacket(xrdId, "Add trace");
        return;
    }
    XrdMonDecDictInfo* di = itr->second;
    
    trace.setUniqueId(di->uniqueId());
    
    if ( ! di->addTrace(trace) ) {
        return; // something wrong with this trace, ignore it
    }
    if ( _saveTraces ) {
        //cout << "Adding trace to sink (dictid=" 
        //<< xrdId << ") " << trace << endl;
        _tCache.push_back(trace);
        if ( _tCache.size() >= _tCacheSize ) {
            flushTCache();
        }
    }
}

void
XrdMonDecSink::addUserDisconnect(dictid_t xrdId,
                                 kXR_int32 sec,
                                 time_t timestamp)
{
    XrdOucMutexHelper mh; mh.Lock(&_uMutex);
    std::map<dictid_t, XrdMonDecUserInfo*>::iterator itr = _uCache.find(xrdId);
    if ( itr == _uCache.end() ) {
        registerLostPacket(xrdId, "User disconnect");
        return;
    }
    itr->second->setDisconnectInfo(sec, timestamp);
    
    if ( _rtOn ) {
        XrdOucMutexHelper mh; mh.Lock(&_urtMutex);
        _rtUCache.push_back(
          pair<OC, XrdMonDecUserInfo*>(CLOSE, itr->second));
    }    
}

void
XrdMonDecSink::openFile(dictid_t xrdId, time_t timestamp)
{
    XrdOucMutexHelper mh; mh.Lock(&_dMutex);
    std::map<dictid_t, XrdMonDecDictInfo*>::iterator itr = _dCache.find(xrdId);
    if ( itr == _dCache.end() ) {
        registerLostPacket(xrdId, "Open file");
        cout << "requested open file " << xrdId << ", xrdId not found" << endl;
        return;
    }

    cout << "Opening file " << xrdId << endl;
    itr->second->openFile(timestamp);

    if ( _rtOn ) {
        XrdOucMutexHelper mh; mh.Lock(&_drtMutex);
        _rtDCache.push_back(pair<OC, XrdMonDecDictInfo*>(OPEN, itr->second));
    }
}

void
XrdMonDecSink::closeFile(dictid_t xrdId, 
                         kXR_int64 bytesR, 
                         kXR_int64 bytesW, 
                         time_t timestamp)
{
    XrdOucMutexHelper mh; mh.Lock(&_dMutex);
    std::map<dictid_t, XrdMonDecDictInfo*>::iterator itr = _dCache.find(xrdId);
    if ( itr == _dCache.end() ) {
        registerLostPacket(xrdId, "Close file");
        return;
    }

    cout << "Closing file id= " << xrdId << " r= " 
         << bytesR << " w= " << bytesW << endl;
    itr->second->closeFile(bytesR, bytesW, timestamp);

    if ( _rtOn ) {
        XrdOucMutexHelper mh; mh.Lock(&_drtMutex);
        _rtDCache.push_back(pair<OC, XrdMonDecDictInfo*>(CLOSE, itr->second));
    }
}

void
XrdMonDecSink::loadUniqueIdsAndSeq()
{
    if ( 0 == access(_jnlPath.c_str(), F_OK) ) {
        char buf[32];
        fstream f(_jnlPath.c_str(), ios::in);
        f.read(buf, sizeof(sequen_t)+2*sizeof(dictid_t));
        f.close();

        memcpy(&_lastSeq, buf, sizeof(sequen_t));
        kXR_int32 v32;

        memcpy(&v32, buf+sizeof(sequen_t), sizeof(kXR_int32));
        _uniqueDictId = ntohl(v32);

        memcpy(&v32, buf+sizeof(sequen_t)+sizeof(dictid_t), sizeof(kXR_int32));
        _uniqueUserId = ntohl(v32);

        cout << "Loaded from jnl file: "
             << "seq " << (int) _lastSeq
             << ", uniqueDictId " << _uniqueDictId 
             << ", uniqueUserId " << _uniqueUserId 
             << endl;
    }
}

void
XrdMonDecSink::flushClosedDicts()
{
    fstream fD(_dictPath.c_str(), ios::out | ios::app);
    enum { BUFSIZE = 1024*1024 };
    
    char buf[BUFSIZE];
    map<dictid_t, XrdMonDecDictInfo*>::iterator itr;
    int curLen = 0, sizeBefore = 0, sizeAfter = 0;
    {
        XrdOucMutexHelper mh; mh.Lock(&_dMutex);
        sizeBefore = _dCache.size();

        vector<dictid_t> forDeletion;
        
        for ( itr=_dCache.begin() ; itr != _dCache.end() ; ++itr ) {
            XrdMonDecDictInfo* di = itr->second;
            if ( di != 0 && di->isClosed() ) {
                string dString = di->convert2string();
                dString += '\t';dString += _senderHost;dString += '\n';
                int strLen = dString.size();
                if ( curLen == 0 ) {
                    strcpy(buf, dString.c_str());
                } else {
                    if ( curLen + strLen >= BUFSIZE ) {
                        fD.write(buf, curLen);
                        curLen = 0;
                        //cout << "flushed to disk: \n" << buf << endl;
                        strcpy(buf, dString.c_str());
                    } else {
                        strcat(buf, dString.c_str());
                    }
                }
                curLen += strLen;
                delete itr->second;
                forDeletion.push_back(itr->first);
            }
        }
        int s = forDeletion.size();
        for (int i=0 ; i<s ; ++i) {
            _dCache.erase(forDeletion[i]);
        }
        
        sizeAfter = _dCache.size();
    }
    
    if ( curLen > 0 ) {
        fD.write(buf, curLen);
        //cout << "flushed to disk: \n" << buf << endl;
    }
    fD.close();
    cout << "flushed (d) " << sizeBefore-sizeAfter << ", left " << sizeAfter << endl;
}

void
XrdMonDecSink::flushUserCache()
{
    fstream fD(_userPath.c_str(), ios::app);
    enum { BUFSIZE = 1024*1024 };
    
    char buf[BUFSIZE];
    map<dictid_t, XrdMonDecUserInfo*>::iterator itr;
    int curLen = 0, sizeBefore = 0, sizeAfter = 0;
    {
        XrdOucMutexHelper mh; mh.Lock(&_uMutex);
        sizeBefore = _uCache.size();
        for ( itr=_uCache.begin() ; itr != _uCache.end() ; ++itr ) {
            XrdMonDecUserInfo* di = itr->second;
            if ( di != 0 && di->readyToBeStored() ) {
                string dString = di->convert2string();
                dString += '\t';dString += _senderHost;dString += '\n';
                int strLen = dString.size();
                if ( curLen == 0 ) {
                    strcpy(buf, dString.c_str());
                } else {
                    if ( curLen + strLen >= BUFSIZE ) {
                        fD.write(buf, curLen);
                        curLen = 0;
                        cout << "flushed to disk: \n" << buf << endl;
                        strcpy(buf, dString.c_str());
                    } else {
                        strcat(buf, dString.c_str());
                    }
                }
                curLen += strLen;
                delete itr->second;
                _uCache.erase(itr);
            }
        }
        sizeAfter = _uCache.size();
    }
    
    if ( curLen > 0 ) {
        fD.write(buf, curLen);
        cout << "flushed to disk: \n" << buf << endl;
    }
    fD.close();
    cout << "flushed (u) " << sizeBefore-sizeAfter << ", left " << sizeAfter << endl;
}

// used for offline processing of (full monitoring with traces) only
void
XrdMonDecSink::flushTCache()
{
    if ( _tCache.size() == 0 ) {
        return;
    }

    fstream f;
    enum { BUFSIZE = 32*1024 };    
    char buf[BUFSIZE];
    int curLen = 0;
    int s = _tCache.size();
    char oneTrace[256];
    for (int i=0 ; i<s ; ++i) {
        _tCache[i].convertToString(oneTrace);
        int strLen = strlen(oneTrace);
        if ( curLen == 0 ) {
            strcpy(buf, oneTrace);
        } else {
            if ( curLen + strLen >= BUFSIZE ) {
                write2TraceFile(f, buf, curLen);                
                curLen = 0;
                //cout << "flushed traces to disk: \n" << buf << endl;
                strcpy(buf, oneTrace);
            } else {
                strcat(buf, oneTrace);
            }
        }
        curLen += strLen;
    }
    if ( curLen > 0 ) {
        write2TraceFile(f, buf, curLen);
        //cout << "flushed traces to disk: \n" << buf << endl;
    }
    _tCache.clear();
    f.close();
}

// used for offline processing of (full monitoring with traces) only
void
XrdMonDecSink::checkpoint()
{
    enum { BUFSIZE = 1024*1024 };    
    char buf[BUFSIZE];
    int bufPos = 0;
    
    // open jnl file
    fstream f(_jnlPath.c_str(), ios::out);

    // save lastSeq and uniqueIds
    memcpy(buf+bufPos, &_lastSeq, sizeof(sequen_t));
    bufPos += sizeof(sequen_t);
    kXR_int32 v = htonl(_uniqueDictId);
    memcpy(buf+bufPos, &v, sizeof(dictid_t));
    bufPos += sizeof(dictid_t);
    v = htonl(_uniqueUserId);
    memcpy(buf+bufPos, &v, sizeof(dictid_t));
    bufPos += sizeof(dictid_t);
    
    // save all active XrdMonDecDictInfos
    int nr =0;
    map<dictid_t, XrdMonDecDictInfo*>::iterator itr;
    {
        XrdOucMutexHelper mh; mh.Lock(&_dMutex);
        for ( itr=_dCache.begin() ; itr != _dCache.end() ; ++itr ) {
            XrdMonDecDictInfo* di = itr->second;
            if ( di != 0 && ! di->isClosed() ) {
                ++nr;
                if ( di->stringSize() + bufPos >= BUFSIZE ) {
                    f.write(buf, bufPos);
                    bufPos = 0;
                }
                di->writeSelf2buf(buf, bufPos); // this will increment bufPos
                delete itr->second;
                _dCache.erase(itr);
            }
        }
    }
    if ( bufPos > 0 ) {
        f.write(buf, bufPos);
    }
    f.close();
    cout << "Saved in jnl file seq " << (int) _lastSeq
         << ", uniqueDictId " << _uniqueDictId 
         << ", uniqueUserId " << _uniqueUserId
         << " and " << nr << " XrdMonDecDictInfo objects." 
         << endl;
}

// used for offline processing of (full monitoring with traces) only
void
XrdMonDecSink::openTraceFile(fstream& f)
{
    stringstream ss(stringstream::out);
    ss << _path << "trace"
       << setw(3) << setfill('0') << _traceLogNumber
       << ".ascii";
    string fPath = ss.str();
    f.open(fPath.c_str(), ios::out | ios::app);
    cout << "trace log file opened " << fPath << endl;
}

// used for offline processing of (full monitoring with traces) only
void
XrdMonDecSink::write2TraceFile(fstream& f, 
                               const char* buf,
                               int len)
{
    if ( ! f.is_open() ) {
        openTraceFile(f);
    }
    kXR_int64 tobeSize = len + f.tellp();
    if (  tobeSize > _maxTraceLogSize*1024*1024 ) {
        f.close();
        ++_traceLogNumber;
        openTraceFile(f);
        
    }
    f.write(buf, len);
}

vector<XrdMonDecDictInfo*>
XrdMonDecSink::loadActiveDictInfo()
{
    vector<XrdMonDecDictInfo*> v;

    if ( 0 != access(_jnlPath.c_str(), F_OK) ) {
        return v;
    }

    fstream f(_jnlPath.c_str(), ios::in);
    f.seekg(0, ios::end);
    int fSize = f.tellg();
    int pos = sizeof(sequen_t) + sizeof(kXR_int32);
    if ( fSize - pos == 0 ) {
        return v; // no active XrdMonDecDictInfo objects
    }
    f.seekg(pos); // skip seq and uniqueId
    char* buf = new char[fSize-pos];
    f.read(buf, fSize-pos);

    int bufPos = 0;
    while ( bufPos < fSize-pos ) {
        v.push_back( new XrdMonDecDictInfo(buf, bufPos) );
    }
    delete [] buf;
    
    return v;
}    

void
XrdMonDecSink::registerLostPacket(dictid_t xrdId, const char* descr)
{
    map<dictid_t, long>::iterator lostItr = _lost.find(xrdId);
    if ( lostItr == _lost.end() ) {
        cerr << descr << ": cannot find dictID " << xrdId << endl;
        _lost[xrdId] = 1;
    } else {
        ++lostItr->second;
    }
}

void
XrdMonDecSink::flushHistoryData()
{
    cout << "Flushing decoded data..." << endl;
    flushClosedDicts();
    flushUserCache();
}

void
XrdMonDecSink::flushRealTimeData()
{
    cout << "Flushing RT data..." << endl;
    fstream f(_rtLogDir.c_str(), ios::out|ios::app);

    {
        XrdOucMutexHelper mh; mh.Lock(&_urtMutex);
        int i, s = _rtUCache.size();
        for ( i=0 ; i<s ; ++i ) {
            if ( _rtUCache[i].first == OPEN ) {
                f << "u " << _rtUCache[i].second->convert2stringRTConnect() 
                  << '\t'  << _senderHost << endl;
                cout << "u " << _rtUCache[i].second->convert2stringRTConnect() 
                     << '\t'  << _senderHost << endl;
            } else {
                f << "d " << _rtUCache[i].second->convert2stringRTDisconnect()
                  << '\t'  << _senderHost << endl;
                cout <<"d " <<_rtUCache[i].second->convert2stringRTDisconnect()
                     << '\t'  << _senderHost << endl;
            }
        }
        _rtUCache.clear();
    }
    {   
        XrdOucMutexHelper mh; mh.Lock(&_drtMutex);
        int i, s = _rtDCache.size();
        for ( i=0 ; i<s ; ++i ) {
            if ( _rtDCache[i].first == OPEN ) {
                f << "o " << _rtDCache[i].second->convert2stringRTOpen() 
                  << '\t' << _senderHost << endl;
                cout << "o " << _rtDCache[i].second->convert2stringRTOpen() 
                     << '\t' << _senderHost << endl;
            } else {
                f << "c " << _rtDCache[i].second->convert2stringRTClose()
                  << '\t' << _senderHost << endl;
                cout << "c " << _rtDCache[i].second->convert2stringRTClose()
                     << '\t' << _senderHost << endl;
            }
        }
        _rtDCache.clear();
    }
    
    f.close();
}

void
XrdMonDecSink::reset()
{
    flushClosedDicts();

    reportLostPackets();
    _lost.clear();
    
    if ( ! _rtOn ) {
        flushTCache();
        checkpoint();
    }
    
    {
        XrdOucMutexHelper mh; mh.Lock(&_dMutex);
        std::map<dictid_t, XrdMonDecDictInfo*>::iterator dItr;
        for ( dItr=_dCache.begin() ; dItr != _dCache.end() ; ++ dItr ) {
            delete dItr->second;
        }
        _dCache.clear();
    }
    {
        XrdOucMutexHelper mh; mh.Lock(&_uMutex);
        std::map<dictid_t, XrdMonDecUserInfo*>::iterator uItr;
        for ( uItr=_uCache.begin() ; uItr != _uCache.end() ; ++ uItr ) {
            delete uItr->second;
        }
        _uCache.clear();
    }
}

void
XrdMonDecSink::reportLostPackets()
{
    int size = _lost.size();
    if ( size > 0 ) {
        cout << "Lost " << size << " dictIds {id, #lostTraces}: ";
        map<dictid_t, long>::iterator lostItr = _lost.begin();
        while ( lostItr != _lost.end() ) {
            cout << "{"<< lostItr->first << ", " << lostItr->second << "} ";
            ++lostItr;
        }    
        cout << endl;
    }
}
