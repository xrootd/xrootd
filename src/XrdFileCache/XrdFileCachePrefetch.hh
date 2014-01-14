#ifndef __XRDFILECACHE_PREFETCH_HH__
#define __XRDFILECACHE_PREFETCH_HH__
//----------------------------------------------------------------------------------
// Copyright (c) 2014 by Board of Trustees of the Leland Stanford, Jr., University
// Author: Alja Mrak-Tadel, Matevz Tadel, Brian Bockelman
//----------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//----------------------------------------------------------------------------------

#include <string>
#include <vector>
#include <queue>

#include <XrdSys/XrdSysPthread.hh>
#include <XrdOss/XrdOss.hh>
#include <XrdOuc/XrdOucCache.hh>

#include "XrdFileCacheInfo.hh"
#include "XrdFileCacheStats.hh"

class XrdClient;
namespace XrdFileCache
{
class Prefetch {

    friend class IOEntireFile;
    friend class IOFileBlock;

    struct Task
    {
        int firstBlock;
        int lastBlock;
        int cntFetched;
        XrdSysCondVar* condVar;

        Task(int fb = 0, int lb = 0, XrdSysCondVar* iCondVar = 0) :
            firstBlock(fb), lastBlock(lb), cntFetched(0),
            condVar(iCondVar) {}

        ~Task() {}

        void Dump();
    };

public:

    Prefetch(XrdOucCacheIO & inputFile, std::string& path, long long offset, long long fileSize);
    ~Prefetch();
    void Run();
    void Join();

    void AddTaskForRng(long long offset, int size, XrdSysCondVar* cond);

    bool GetStatForRng(long long offset, int size, int& pulled, int& nblocks);

    Stats&
    GetStats() { return m_stats; }

protected:
    ssize_t Read(char * buff, off_t offset, size_t size);
    void AppendIOStatToFileInfo();

    void CloseCleanly();

private:
    bool GetNextTask(Task&);
    bool Open();
    bool Close();
    bool Fail(bool cleanup);
    void RecordDownloadInfo();
    int getBytesToRead(Task& task, int block) const;

    // file
    XrdOssDF *m_output;
    XrdOssDF *m_infoFile;
    Info m_cfi;
    XrdOucCacheIO & m_input;
    std::string m_temp_filename;
    long long m_offset;
    long long m_fileSize;
    std::queue<Task> m_tasks_queue;

    bool m_started;
    bool m_failed;
    bool m_stop;

    int m_numMissBlock;
    int m_numHitBlock;

    XrdSysCondVar m_stateCond;
    XrdSysMutex m_downloadStatusMutex;
    XrdSysMutex m_quequeMutex;

    Stats m_stats;
};

}
#endif
