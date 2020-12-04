#include "XrdPfc.hh"
#include "XrdPfcTrace.hh"

#include <fcntl.h>
#include <sys/time.h>

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOss/XrdOssAt.hh"
#include "XrdSys/XrdSysTrace.hh"

using namespace XrdPfc;

namespace XrdPfc
{

XrdSysTrace* GetTrace()
{
   // needed for logging macros
   return Cache::GetInstance().GetTrace();
}

// Temporary, extensive purge tracing
// #define TRACE_PURGE(x) TRACE(Debug, x)
// #define TRACE_PURGE(x) std::cout << "PURGE " << x << "\n"
#define TRACE_PURGE(x)

//==============================================================================
// DirState
//==============================================================================

class DirState
{
   DirState    *m_parent;

   Stats        m_stats;        // access stats from client reads in this directory (and subdirs)

   long long    m_usage;        // collected / measured during purge traversal
   long long    m_usage_extra;  // collected from write events in this directory and subdirs
   long long    m_usage_purged; // amount of data purged from this directory (and subdirectories for leaf nodes)

   // begin purge traversal usage \_ so we can have a good estimate of what came in during the traversal
   // end purge traversal usage   /  (should be small, presumably)

   // quota info, enabled?

   int          m_depth;
   int          m_max_depth;    // XXXX Do we need this? Should it be passed in to find functions?
   bool         m_stat_report;  // not used yet - storing of stats requested

   typedef std::map<std::string, DirState> DsMap_t;
   typedef DsMap_t::iterator               DsMap_i;

   DsMap_t      m_subdirs;

   void init()
   {
      m_usage = 0;
      m_usage_extra  = 0;
      m_usage_purged = 0;
   }

   DirState* create_child(const std::string &dir)
   {
      std::pair<DsMap_i, bool> ir = m_subdirs.insert(std::make_pair(dir, DirState(this)));
      return &  ir.first->second;
   }

   DirState* find_path_tok(PathTokenizer &pt, int pos, bool create_subdirs)
   {
      if (pos == pt.get_n_dirs()) return this;

      DsMap_i i = m_subdirs.find(pt.m_dirs[pos]);

      DirState *ds = 0;

      if (i != m_subdirs.end())
      {
         ds = & i->second;
      }
      if (create_subdirs && m_depth < m_max_depth)
      {
         ds = create_child(pt.m_dirs[pos]);
      }
      if (ds) return ds->find_path_tok(pt, pos + 1, create_subdirs);

      return 0;
   }

public:

   DirState(int max_depth) : m_parent(0), m_depth(0), m_max_depth(max_depth)
   {
      init();
   }

   DirState(DirState *parent) : m_parent(parent), m_depth(m_parent->m_depth + 1), m_max_depth(m_parent->m_max_depth)
   {
      init();
   }

   DirState* get_parent()                     { return m_parent; }

   void      set_usage(long long u)           { m_usage = u; m_usage_extra = 0; }
   void      add_up_stats(const Stats& stats) { m_stats.AddUp(stats); }
   void      add_usage_purged(long long up)   { m_usage_purged += up; }

   DirState* find_path(const std::string &path, int max_depth, bool parse_as_lfn, bool create_subdirs)
   {
      PathTokenizer pt(path, max_depth, parse_as_lfn);

      return find_path_tok(pt, 0, create_subdirs);
   }

   DirState* find_dir(const std::string &dir, bool create_subdirs)
   {
      DsMap_i i = m_subdirs.find(dir);

      if (i != m_subdirs.end())  return & i->second;

      if (create_subdirs && m_depth < m_max_depth)  return create_child(dir);

      return 0;
   }

   void reset_stats()
   {
      m_stats.Reset();

      for (DsMap_i i = m_subdirs.begin(); i != m_subdirs.end(); ++i)
      {
         i->second.reset_stats();
      }
   }

   void upward_propagate_stats()
   {
      for (DsMap_i i = m_subdirs.begin(); i != m_subdirs.end(); ++i)
      {
         i->second.upward_propagate_stats();

         m_stats.AddUp(i->second.m_stats);
      }

      m_usage_extra += m_stats.m_BytesWritten;
   }

   long long upward_propagate_usage_purged()
   {
      for (DsMap_i i = m_subdirs.begin(); i != m_subdirs.end(); ++i)
      {
         m_usage_purged += i->second.upward_propagate_usage_purged();
      }
      m_usage -= m_usage_purged;

      long long ret = m_usage_purged;
      m_usage_purged = 0;
      return ret;
   }

   void dump_recursively(const char *name)
   {
      printf("%*d %s usage=%lld usage_extra=%lld usage_total=%lld num_ios=%d duration=%d b_hit=%lld b_miss=%lld b_byps=%lld b_wrtn=%lld\n",
             2 + 2*m_depth, m_depth, name, m_usage, m_usage_extra, m_usage + m_usage_extra,
             m_stats.m_NumIos, m_stats.m_Duration, m_stats.m_BytesHit, m_stats.m_BytesMissed, m_stats.m_BytesBypassed, m_stats.m_BytesWritten);

      for (DsMap_i i = m_subdirs.begin(); i != m_subdirs.end(); ++i)
      {
         i->second.dump_recursively(i->first.c_str());
      }
   }
};


//==============================================================================
// DataFsState
//==============================================================================

class DataFsState
{
   int       m_max_depth;
   DirState  m_root;
   time_t    m_prev_time;

public:
   DataFsState() :
      m_max_depth ( Cache::Conf().m_dirStatsStoreDepth ),
      m_root      ( m_max_depth ),
      m_prev_time ( time(0) )
   {}

   int       get_max_depth() const { return m_max_depth; }

   DirState* get_root()            { return & m_root; }

   DirState* find_dirstate_for_lfn(const std::string& lfn)
   {
      return m_root.find_path(lfn, m_max_depth, true, true);
   }

   void reset_stats()                   { m_root.reset_stats();                   }
   void upward_propagate_stats()        { m_root.upward_propagate_stats();        }
   void upward_propagate_usage_purged() { m_root.upward_propagate_usage_purged(); }

   void dump_recursively()
   {
      time_t now = time(0);

      printf("DataFsState::dump_recursively epoch = %lld delta_t = %lld max_depth = %d\n",
             (long long) now, (long long) (now - m_prev_time), m_max_depth);

      m_prev_time = now;

      m_root.dump_recursively("root");
   }
};


//==============================================================================
// FPurgeState
//==============================================================================

class FPurgeState
{
public:
   struct FS
   {
      std::string path;
      long long   nBytes;
      time_t      time;
      DirState   *dirState;

      FS(const std::string &dname, const char *fname, long long n, time_t t, DirState *ds) :
         path(dname + fname), nBytes(n), time(t), dirState(ds)
      {}
   };

   typedef std::multimap<time_t, FS> map_t;
   typedef map_t::iterator           map_i;

   map_t   m_fmap; // map of files that are purge candidates

   typedef std::list<FS>    list_t;
   typedef list_t::iterator list_i;

   list_t  m_flist; // list of files to be removed unconditionally

   long long nBytesReq;
   long long nBytesAccum;
   long long nBytesTotal;
   time_t    tMinTimeStamp;
   time_t    tMinUVKeepTimeStamp;

   // XrdOss   *m_oss;
   XrdOssAt  m_oss_at;

   // ------------------------------------
   // Directory handling & stat collection
   // ------------------------------------

   DirState    *m_dir_state;
   std::string  m_current_path; // Includes trailing '/'
   int          m_dir_level;
   const int    m_max_dir_level_for_stat_collection; // until we honor globs from pfc.dirstats

   std::vector<std::string> m_dir_names_stack;
   std::vector<long long>   m_dir_usage_stack;

   const char   *m_info_ext;
   const size_t  m_info_ext_len;
   XrdSysTrace  *m_trace;

   static const char *m_traceID;


   void begin_traversal(DirState *root, const char *root_path = "/")
   {
      m_dir_state = root;
      m_dir_level = 0;
      m_current_path = root_path;
      m_dir_usage_stack.push_back(0);

      TRACE_PURGE("FPurgeState::begin_traversal cur_path '" << m_current_path << "', usage=" << m_dir_usage_stack.back() << ", level=" << m_dir_level);
   }

   void end_traversal()
   {
      TRACE_PURGE("FPurgeState::end_traversal reporting for '" << m_current_path << "', usage=" << m_dir_usage_stack.back() << ", nBytesTotal=" << nBytesTotal << ", level=" << m_dir_level);

      m_dir_state->set_usage(m_dir_usage_stack.back());

      m_dir_state = 0;
   }

   void cd_down(const std::string& dir_name)
   {
      ++m_dir_level;

      if (m_dir_level <= m_max_dir_level_for_stat_collection)
      {
         m_dir_usage_stack.push_back(0);
         m_dir_state = m_dir_state->find_dir(dir_name, true);
      }

      m_dir_names_stack.push_back(dir_name);
      m_current_path.append(dir_name);
      m_current_path.append("/");
   }

   void cd_up()
   {
      if (m_dir_level <= m_max_dir_level_for_stat_collection)
      {
         long long tail = m_dir_usage_stack.back();
         m_dir_usage_stack.pop_back();

         TRACE_PURGE("FPurgeState::cd_up reporting for '" << m_current_path << "', usage=" << tail << ", level=" << m_dir_level);

         m_dir_state->set_usage(tail);
         m_dir_state = m_dir_state->get_parent();

         m_dir_usage_stack.back() += tail;
      }

      // remove trailing / and last dir but keep the new trailing / in place.
      m_current_path.erase(m_current_path.find_last_of('/', m_current_path.size() - 2) + 1);
      m_dir_names_stack.pop_back();

      --m_dir_level;
   }

   // ------------------------------------------------------------------------
   // ------------------------------------------------------------------------

   FPurgeState(long long iNBytesReq, XrdOss &oss) :
      nBytesReq(iNBytesReq), nBytesAccum(0), nBytesTotal(0), tMinTimeStamp(0), tMinUVKeepTimeStamp(0),
      // m_oss(oss),
      m_oss_at(oss),
      m_dir_state(0), m_dir_level(0),
      m_max_dir_level_for_stat_collection(Cache::Conf().m_dirStatsStoreDepth),
      m_info_ext(XrdPfc::Info::s_infoExtension),
      m_info_ext_len(strlen(XrdPfc::Info::s_infoExtension)),
      m_trace(Cache::GetInstance().GetTrace())
   {
      m_current_path.reserve(256);
      m_dir_names_stack.reserve(32);
      m_dir_usage_stack.reserve(m_max_dir_level_for_stat_collection + 1);
  }

   // ------------------------------------------------------------------------

   void      setMinTime(time_t min_time) { tMinTimeStamp = min_time; }
   time_t    getMinTime()          const { return tMinTimeStamp; }
   void      setUVKeepMinTime(time_t min_time) { tMinUVKeepTimeStamp = min_time; }
   long long getNBytesTotal()      const { return nBytesTotal; }

   void MoveListEntriesToMap()
   {
      for (list_i i = m_flist.begin(); i != m_flist.end(); ++i)
      {
         m_fmap.insert(std::make_pair(i->time, *i));
      }
      m_flist.clear();
   }

   /*
   void UnlinkInfoAndData(const char *fname, long long nbytes, XrdOssDF *iOssDF)
   {
      fname[fname_len - m_info_ext_len] = 0;
      if (nbytes > 0)
      {
         if ( ! Cache.GetInstance().IsFileActiveOrPurgeProtected(dataPath))
         {
            m_n_purged++;
            m_bytes_purged += nbytes;
         } else 
         {
            m_n_purge_protected++;
            m_bytes_purge_protected += nbytes;
            m_dir_state->add_usage_purged(nbytes);
            // XXXX should also tweak other stuff?
            fname[fname_len - m_info_ext_len] = '.';
            return;
         }
      }
      m_oss_at.Unlink(*iOssDF, fname);
      fname[fname_len - m_info_ext_len] = '.';
      m_oss_at.Unlink(*iOssDF, fname);
   }
   */

   void CheckFile(const char *fname, Info &info, struct stat &fstat /*, XrdOssDF *iOssDF*/)
   {
      static const char *trc_pfx   = "FPurgeState::CheckFile ";

      long long nbytes = info.GetNDownloadedBytes();
      time_t    atime;
      if ( ! info.GetLatestDetachTime(atime))
      {
         // cinfo file does not contain any known accesses, use fstat.mtime instead.
         TRACE(Debug, trc_pfx << "could not get access time for " << m_current_path << fname << ", using mtime from stat instead.");
         atime = fstat.st_mtime;
      }
      // TRACE(Dump, trc_pfx << "checking " << fname << " accessTime  " << atime);

      nBytesTotal += nbytes;

      m_dir_usage_stack.back() += nbytes;

      // XXXX Should remove aged-out files here ... but I have trouble getting
      // the DirState and purge report set up consistently.
      // Need some serious code reorganization here.
      // Biggest problem is maintaining overall state a traversal state consistently.
      // Sigh.

      // In first two cases we lie about FS time (set to 0) to get them all removed early.
      // The age-based purge atime would also be good as there should be nothing
      // before that time in the map anyway.
      // But we use 0 as a test in purge loop to make sure we continue even if enough
      // disk-space has been freed.

      if (tMinTimeStamp > 0 && atime < tMinTimeStamp)
      {
         m_flist.push_back(FS(m_current_path, fname, nbytes, 0, m_dir_state));
         nBytesAccum += nbytes;
      }
      else if (tMinUVKeepTimeStamp > 0 &&
               Cache::Conf().does_cschk_have_missing_bits(info.GetCkSumState()) &&
               info.GetNoCkSumTimeForUVKeep() < tMinUVKeepTimeStamp)
      {
         m_flist.push_back(FS(m_current_path, fname, nbytes, 0, m_dir_state));
         nBytesAccum += nbytes;
      }
      else if (nBytesAccum < nBytesReq || ( ! m_fmap.empty() && atime < m_fmap.rbegin()->first))
      {
         m_fmap.insert(std::make_pair(atime, FS(m_current_path, fname, nbytes, atime, m_dir_state)));
         nBytesAccum += nbytes;

         // remove newest files from map if necessary
         while ( ! m_fmap.empty() && nBytesAccum - m_fmap.rbegin()->second.nBytes >= nBytesReq)
         {
            nBytesAccum -= m_fmap.rbegin()->second.nBytes;
            m_fmap.erase(--(m_fmap.rbegin().base()));
         }
      }
   }

   void TraverseNamespace(XrdOssDF *iOssDF)
   {
      static const char *trc_pfx = "FPurgeState::TraverseNamespace ";

      char          fname[256];
      struct stat   fstat;
      XrdOucEnv     env;

      TRACE_PURGE("Starting to read dir [" << m_current_path << "], iOssDF->getFD()=" << iOssDF->getFD() << ".");

      iOssDF->StatRet(&fstat);

      while (true)
      {
         int rc = iOssDF->Readdir(fname, 256);

         if (rc == -ENOENT) {
            TRACE_PURGE("  Skipping ENOENT dir entry [" << fname << "].");
            continue;
         }
         if (rc != XrdOssOK) {
            TRACE(Error, trc_pfx << "Readdir error at " << m_current_path  << ", err " << XrdSysE2T(-rc) << ".");
            break;
         }

         TRACE_PURGE("  Readdir [" << fname << "]");

         if (fname[0] == 0) {
            TRACE_PURGE("  Finished reading dir [" << m_current_path << "]. Break loop.");
            break;
         }
         if (fname[0] == '.' && (fname[1] == 0 || (fname[1] == '.' && fname[2] == 0))) {
            TRACE_PURGE("  Skipping here or parent dir [" << fname << "]. Continue loop.");
            continue;
         }

         size_t    fname_len = strlen(fname);
         XrdOssDF *dfh       = 0;

         if (S_ISDIR(fstat.st_mode))
         {
            if (m_oss_at.Opendir(*iOssDF, fname, env, dfh) == XrdOssOK)
            {
               cd_down(fname);           TRACE_PURGE("  cd_down -> [" << m_current_path << "].");
               TraverseNamespace(dfh);
               cd_up();                  TRACE_PURGE("  cd_up   -> [" << m_current_path << "].");
            }
            else
               TRACE(Warning, trc_pfx << "could not opendir [" << m_current_path << fname << "], " << XrdSysE2T(errno));
         }
         else if (fname_len > m_info_ext_len && strncmp(&fname[fname_len - m_info_ext_len], m_info_ext, m_info_ext_len) == 0)
         {
            // Check if the file is currently opened / purge-protected is done before unlinking of the file.

            Info cinfo(m_trace);

            if (m_oss_at.OpenRO(*iOssDF, fname, env, dfh) == XrdOssOK && cinfo.Read(dfh, m_current_path.c_str(), fname))
            {
               CheckFile(fname, cinfo, fstat);
            }
            else
            {
               TRACE(Warning, trc_pfx << "can't open or read " << m_current_path << fname << ", err " << XrdSysE2T(errno) << "; purging.");
               m_oss_at.Unlink(*iOssDF, fname);
               fname[fname_len - m_info_ext_len] = 0;
               m_oss_at.Unlink(*iOssDF, fname);
            }
         }
         else // XXXX devel debug only, to be removed
         {
            TRACE_PURGE("  Ignoring [" << fname << "], not a dir or cinfo.");
         }

         delete dfh;
      }
   }
};

const char *FPurgeState::m_traceID = "Purge";


//==============================================================================
// ResourceMonitor
//==============================================================================

// Encapsulates local variables used withing the previous mega-function Purge().
//
// This will be used within the continuously/periodically ran heart-beat / breath
// function ... and then parts of it will be passed to invoked FS scan and purge
// jobs (which will be controlled throught this as well).

class ResourceMonitor
{

};


//==============================================================================
//
//==============================================================================

namespace
{

class ScanAndPurgeJob : public XrdJob
{
public:
   ScanAndPurgeJob(const char *desc = "") : XrdJob(desc) {}

   void DoIt() {} // { Cache::GetInstance().ScanAndPurge(); }
};

}

//==============================================================================
// Cache methods
//==============================================================================

void Cache::copy_out_active_stats_and_update_data_fs_state()
{
   static const char *trc_pfx = "copy_out_active_stats_and_update_data_fs_state() ";

   StatsMMap_t updates;
   {
      XrdSysCondVarHelper lock(&m_active_cond);

      // Slurp in stats from files closed since last cycle.
      updates.swap( m_closed_files_stats );

      for (ActiveMap_i i = m_active.begin(); i != m_active.end(); ++i)
      {
         if (i->second != 0)
         {
            updates.insert(std::make_pair(i->first, i->second->DeltaStatsFromLastCall()));
         }
      }
   }

   m_fs_state->reset_stats(); // XXXX-CKSUM rethink how to do this if we keep some purge entries for next time

   for (StatsMMap_i i = updates.begin(); i != updates.end(); ++i)
   {
      DirState *ds = m_fs_state->find_dirstate_for_lfn(i->first);

      if (ds == 0)
      {
         TRACE(Error, trc_pfx << "Failed finding DirState for file '" << i->first << "'.");
         continue;
      }

      ds->add_up_stats(i->second);
   }

   m_fs_state->upward_propagate_stats();
}


//==============================================================================

void Cache::ResourceMonitorHeartBeat()
{
   // static const char *trc_pfx = "ResourceMonitorHeartBeat() ";

   // Pause before initial run
   sleep(1);

   // XXXX Setup initial / constant stats (total RAM, total disk, ???)

   XrdOucCacheStats             &S = Statistics;
   XrdOucCacheStats::CacheStats &X = Statistics.X;

   S.Lock();

   X.DiskSize = m_configuration.m_diskTotalSpace;

   X.MemSize = m_configuration.m_RamAbsAvailable;

   S.UnLock();

   // XXXX Schedule initial disk scan, time it!
   //
   // TRACE(Info, trc_pfx << "scheduling intial disk scan.");
   // schedP->Schedule( new ScanAndPurgeJob("XrdPfc::ScanAndPurge") );
   //
   // bool scan_and_purge_running = true;

   // XXXX Could we really hold last-usage for all files in memory?

   // XXXX Think how to handle disk-full, scan/purge not finishing:
   // - start dropping things out of write queue, but only when RAM gets near full;
   // - monitoring this then becomes a high-priority job, inner loop with sleep of,
   //   say, 5 or 10 seconds.

   while (true)
   {
      time_t heartbeat_start = time(0);

      // TRACE(Info, trc_pfx << "HeartBeat starting ...");

      // if sumary monitoring configured, pupulate OucCacheStats:
      S.Lock();

      // - available / used disk space (files usage calculated elsewhere (maybe))

      // - RAM usage
      {  XrdSysMutexHelper lck(&m_RAM_mutex);
         X.MemUsed   = m_RAM_used;
         X.MemWriteQ = m_RAM_write_queue;
      }
      // - files opened / closed etc

      // do estimate of available space
      S.UnLock();

      // if needed, schedule purge in a different thread.
      // purge is:
      // - deep scan + gather FSPurgeState
      // - actual purge
      //
      // this thread can continue running and, if needed, stop writing to disk
      // if purge is taking too long.

      // think how data is passed / synchronized between this and purge thread

      // !!!! think how stat collection is done and propgated upwards;
      // until now it was done once per purge-interval.
      // now stats will be added up more often, but purge will be done
      // only occasionally.
      // also, do we report cumulative values or deltas? cumulative should
      // be easier and consistent with summary data.
      // still, some are state - like disk usage, num of files.

      // Do we take care of directories that need to be newly added into DirState hierarchy?
      // I.e., when user creates new directories and these are covered by either full
      // spec or by root + depth declaration.

      int heartbeat_duration = time(0) - heartbeat_start;

      // TRACE(Info, trc_pfx << "HeartBeat finished, heartbeat_duration " << heartbeat_duration);

      // int sleep_time = m_configuration.m_purgeInterval - heartbeat_duration;
      int sleep_time = 60 - heartbeat_duration;
      if (sleep_time > 0)
      {
         sleep(sleep_time);
      }
   }
}

//==============================================================================

void Cache::Purge()
{
   static const char *trc_pfx = "Purge() ";

   XrdOucEnv    env;
   long long    disk_usage;
   long long    estimated_file_usage = m_configuration.m_diskUsageHWM;

   // Pause before initial run
   sleep(1);

   m_fs_state = new DataFsState;

   // { PathTokenizer p("/a/b/c/f.root", 2, true); p.deboog(); }
   // { PathTokenizer p("/a/b/f.root", 2, true); p.deboog(); }
   // { PathTokenizer p("/a/f.root", 2, true); p.deboog(); }
   // { PathTokenizer p("/f.root", 2, true); p.deboog(); }

   int  age_based_purge_countdown = 0; // enforce on first purge loop entry.
   bool is_first = true;

   while (true)
   {
      time_t purge_start = time(0);

      {
         XrdSysCondVarHelper lock(&m_active_cond);

         m_in_purge = true;
      }

      TRACE(Info, trc_pfx << "Started.");

      // Bytes to remove based on total disk usage (d) and file usage (f).
      long long bytesToRemove_d = 0, bytesToRemove_f = 0;

      // get amount of space to potentially erase based on total disk usage
      XrdOssVSInfo sP; // Make sure we start when a clean slate in each loop
      if (m_oss->StatVS(&sP, m_configuration.m_data_space.c_str(), 1) < 0)
      {
         TRACE(Error, trc_pfx << "can't get StatVS for oss space " << m_configuration.m_data_space);
         continue;
      }
      else
      {
         disk_usage = sP.Total - sP.Free;
         TRACE(Debug, trc_pfx << "used disk space " << disk_usage << " bytes.");

         if (disk_usage > m_configuration.m_diskUsageHWM)
         {
            bytesToRemove_d = disk_usage - m_configuration.m_diskUsageLWM;
         }
      }

      // estimate amount of space to erase based on file usage
      if (m_configuration.are_file_usage_limits_set())
      {
         long long estimated_writes_since_last_purge;
         {
            XrdSysCondVarHelper lock(&m_writeQ.condVar);

            estimated_writes_since_last_purge = m_writeQ.writes_between_purges;
            m_writeQ.writes_between_purges = 0;
         }
         estimated_file_usage += estimated_writes_since_last_purge;

         TRACE(Debug, trc_pfx << "estimated usage by files " << estimated_file_usage << " bytes.");

         bytesToRemove_f = std::max(estimated_file_usage - m_configuration.m_fileUsageNominal, 0ll);

         // Here we estimate fractional usages -- to decide if full scan is necessary before actual purge.
         double frac_du = 0, frac_fu = 0;
         m_configuration.calculate_fractional_usages(disk_usage, estimated_file_usage, frac_du, frac_fu);

         if (frac_fu > 1.0 - frac_du)
         {
            bytesToRemove_f = std::max(bytesToRemove_f, disk_usage - m_configuration.m_diskUsageLWM);
         }
      }

      long long bytesToRemove = std::max(bytesToRemove_d, bytesToRemove_f);

      bool enforce_age_based_purge = false;
      if (m_configuration.is_age_based_purge_in_effect() || m_configuration.is_uvkeep_purge_in_effect())
      {
         // XXXX ... I could collect those guys in larger vectors (maps?) and do traversal when
         // they are empty.
         if (--age_based_purge_countdown <= 0)
         {
            enforce_age_based_purge   = true;
            age_based_purge_countdown = m_configuration.m_purgeAgeBasedPeriod;
         }
      }

      bool enforce_traversal_for_usage_collection = is_first;
      // XXX Other conditions? Periodic checks?

      copy_out_active_stats_and_update_data_fs_state();

      TRACE(Debug, trc_pfx << "Precheck:");
      TRACE(Debug, "\tbytes_to_remove_disk    = " << bytesToRemove_d << " B");
      TRACE(Debug, "\tbytes_to remove_files   = " << bytesToRemove_f << " B (" << (is_first ? "max possible for initial run" : "estimated") << ")");
      TRACE(Debug, "\tbytes_to_remove         = " << bytesToRemove   << " B");
      TRACE(Debug, "\tenforce_age_based_purge = " << enforce_age_based_purge);
      is_first = false;

      long long bytesToRemove_at_start = 0; // set after file scan
      int       deleted_file_count     = 0;

      bool purge_required = (bytesToRemove > 0 || enforce_age_based_purge);

      // XXXX-PurgeOpt Need to retain this state between purges so I can avoid doing
      // the traversal more often than really needed.
      FPurgeState purgeState(2 * bytesToRemove, *m_oss); // prepare twice more volume than required

      if (purge_required || enforce_traversal_for_usage_collection)
      {
         // Make a sorted map of file paths sorted by access time.

         if (m_configuration.is_age_based_purge_in_effect())
         {
            purgeState.setMinTime(time(0) - m_configuration.m_purgeColdFilesAge);
         }
         if (m_configuration.is_uvkeep_purge_in_effect())
         {
            purgeState.setUVKeepMinTime(time(0) - m_configuration.m_cs_UVKeep);
         }

         XrdOssDF* dh = m_oss->newDir(m_configuration.m_username.c_str());
         if (dh->Opendir("/", env) == XrdOssOK)
         {
            purgeState.begin_traversal(m_fs_state->get_root());

            purgeState.TraverseNamespace(dh);

            purgeState.end_traversal();

            dh->Close();
         }
         delete dh; dh = 0;

         estimated_file_usage = purgeState.getNBytesTotal();

         TRACE(Debug, trc_pfx << "actual usage by files " << estimated_file_usage << " bytes.");

         // Adjust bytesToRemove_f and then bytesToRemove based on actual file usage,
         // possibly retreating below nominal file usage (but not below baseline file usage).
         if (m_configuration.are_file_usage_limits_set())
         {
            bytesToRemove_f = std::max(estimated_file_usage - m_configuration.m_fileUsageNominal, 0ll);

            double frac_du = 0, frac_fu = 0;
            m_configuration.calculate_fractional_usages(disk_usage, estimated_file_usage, frac_du, frac_fu);

            if (frac_fu > 1.0 - frac_du)
            {
               bytesToRemove = std::max(bytesToRemove_f, disk_usage - m_configuration.m_diskUsageLWM);
               bytesToRemove = std::min(bytesToRemove,   estimated_file_usage - m_configuration.m_fileUsageBaseline);
            }
            else
            {
               bytesToRemove = std::max(bytesToRemove_d, bytesToRemove_f);
            }
         }
         else
         {
            bytesToRemove = std::max(bytesToRemove_d, bytesToRemove_f);
         }
         bytesToRemove_at_start = bytesToRemove;

         TRACE(Debug, trc_pfx << "After scan:");
         TRACE(Debug, "\tbytes_to_remove_disk    = " << bytesToRemove_d << " B");
         TRACE(Debug, "\tbytes_to remove_files   = " << bytesToRemove_f << " B (measured)");
         TRACE(Debug, "\tbytes_to_remove         = " << bytesToRemove   << " B");
         TRACE(Debug, "\tenforce_age_based_purge = " << enforce_age_based_purge);
         TRACE(Debug, "\tmin_time                = " << purgeState.getMinTime());

         if (enforce_age_based_purge)
         {
            purgeState.MoveListEntriesToMap();
         }
      }

      // Dump statistcs before actual purging so maximum usage values get recorded.
      // Should really go to gstream --- and should really go from Heartbeat.
      if (m_configuration.is_dir_stat_reporting_on())
      {
         m_fs_state->dump_recursively();
      }

      if (purge_required)
      {
         // Loop over map and remove files with oldest values of access time.
         struct stat fstat;
         size_t      info_ext_len  =  strlen(Info::s_infoExtension);
         int         protected_cnt = 0;
         long long   protected_sum = 0;
         for (FPurgeState::map_i it = purgeState.m_fmap.begin(); it != purgeState.m_fmap.end(); ++it)
         {
            // Finish when enough space has been freed but not while age-based purging is in progress.
            // Those files are marked with time-stamp = 0.
            if (bytesToRemove <= 0 && ! (enforce_age_based_purge && it->first == 0))
            {
               break;
            }

            std::string &infoPath = it->second.path;
            std::string  dataPath = infoPath.substr(0, infoPath.size() - info_ext_len);

            if (IsFileActiveOrPurgeProtected(dataPath))
            {
               ++protected_cnt;
               protected_sum += it->second.nBytes;
               TRACE(Debug, trc_pfx << "File is active or purge-protected: " << dataPath << " size: " << it->second.nBytes);
               continue;
            }

            // remove info file
            if (m_oss->Stat(infoPath.c_str(), &fstat) == XrdOssOK)
            {
               // cinfo file can be on another oss.space, do not subtract for now.
               // Could be relevant for very small block sizes.
               // bytesToRemove        -= fstat.st_size;
               // estimated_file_usage -= fstat.st_size;
               // ++deleted_file_count;

               m_oss->Unlink(infoPath.c_str());
               TRACE(Dump, trc_pfx << "Removed file: '" << infoPath << "' size: " << fstat.st_size);
            }

            // remove data file
            if (m_oss->Stat(dataPath.c_str(), &fstat) == XrdOssOK)
            {
               bytesToRemove        -= it->second.nBytes;
               estimated_file_usage -= it->second.nBytes;
               ++deleted_file_count;

               m_oss->Unlink(dataPath.c_str());
               TRACE(Dump, trc_pfx << "Removed file: '" << dataPath << "' size: " << it->second.nBytes << ", time: " << it->first);

               if (it->second.dirState != 0) // XXXX This should now always be true.
                  it->second.dirState->add_usage_purged(it->second.nBytes);
               else
                  TRACE(Error, trc_pfx << "DirState not set for file '" << dataPath << "'.");
            }
         }
         if (protected_cnt > 0)
         {
            TRACE(Info, trc_pfx << "Encountered " << protected_cnt << " protected files, sum of their size: " << protected_sum);
         }

         m_fs_state->upward_propagate_usage_purged();
      }

      {
         XrdSysCondVarHelper lock(&m_active_cond);

         m_purge_delay_set.clear();
         m_in_purge = false;
      }

      int purge_duration = time(0) - purge_start;

      TRACE(Info, trc_pfx << "Finished, removed " << deleted_file_count << " data files, total size " <<
            bytesToRemove_at_start - bytesToRemove << ", bytes to remove at end " << bytesToRemove << ", purge duration " << purge_duration);

      int sleep_time = m_configuration.m_purgeInterval - purge_duration;
      if (sleep_time > 0)
      {
         sleep(sleep_time);
      }
   }
}

} // end XrdPfc namespace
