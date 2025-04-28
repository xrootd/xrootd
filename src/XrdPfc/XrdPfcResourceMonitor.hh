#ifndef __XRDPFC_RESOURCEMONITOR_HH__
#define __XRDPFC_RESOURCEMONITOR_HH__

#include "XrdPfcStats.hh"

#include "XrdSys/XrdSysPthread.hh"

#include <string>
#include <vector>
#include <list>

class XrdOss;

namespace XrdPfc {

struct DataFsState;
struct DirState;
struct DirStateElement;
struct DataFsSnapshot;
struct DirPurgeElement;
struct DataFsPurgeshot;
class FsTraversal;

//==============================================================================
// ResourceMonitor
//==============================================================================

// Encapsulates local variables used withing the previous mega-function Purge().
//
// This will be used within the continuously/periodically ran heart-beat / breath
// function ... and then parts of it will be passed to invoked FS scan and purge
// jobs (which will be controlled throught this as well).

// Andy: XRDADMINPATH Is the directory for administrative files (i.e. all.adminpath)
// Also: XrdOucEnv::Export("XRDLOGDIR", logParms.logfn); (in XrdOucLogging::configLog)

class ResourceMonitor
{
   template<typename ID, typename RECORD>
   class Queue {
   public:
      struct Entry {
         ID      id;
         RECORD  record;
      };
      using queue_type = std::vector<Entry>;
      using iterator   = typename queue_type::iterator;

      Queue() = default;

      int  write_queue_size() const { return m_write_queue.size(); }
      bool read_queue_empty() const { return m_read_queue.empty(); }
      int  read_queue_size()  const { return m_read_queue.size(); }

      // Writer / producer access
      void push(ID id, RECORD stat) { m_write_queue.push_back({ id, stat }); }
      // Existing entry access for updating Stats
      RECORD& write_record(int pos) { return m_write_queue[pos].record; }

      // Reader / consumer access
      int swap_queues() { m_read_queue.clear(); m_write_queue.swap(m_read_queue); return read_queue_size(); }
      const queue_type& read_queue() const { return m_read_queue; }
      iterator begin() const { return m_read_queue.begin(); }
      iterator end()   const { return m_read_queue.end(); }

      // Shrinkage of overgrown queues
      void shrink_read_queue() { m_read_queue.clear(); m_read_queue.shrink_to_fit(); }

   private:
      queue_type m_write_queue, m_read_queue;
   };

   struct AccessToken {
      std::string  m_filename;
      unsigned int m_last_queue_swap_u1   = 0xffffffff;
      int          m_last_write_queue_pos = -1;
      DirState    *m_dir_state = nullptr;

      void clear() {
         m_filename.clear();
         m_last_queue_swap_u1 = 0xffffffff;
         m_last_write_queue_pos = -1;
         m_dir_state = nullptr;
      }
   };
   std::vector<AccessToken> m_access_tokens;
   std::vector<int>         m_access_tokens_free_slots;

   struct OpenRecord {
      time_t m_open_time;
      bool   m_existing_file;
   };

   struct CloseRecord {
      time_t m_close_time;
      Stats  m_full_stats;
   };

   struct PurgeRecord {
      long long m_size_in_st_blocks;
      int       m_n_files;
   };

   Queue<int, OpenRecord>          m_file_open_q;
   Queue<int, Stats>               m_file_update_stats_q;
   Queue<int, CloseRecord>         m_file_close_q;
   Queue<DirState*,   PurgeRecord> m_file_purge_q1;
   Queue<std::string, PurgeRecord> m_file_purge_q2;
   Queue<std::string, long long>   m_file_purge_q3;
   // DirPurge queue -- not needed? But we do need last-change timestamp in DirState.

   long long    m_current_usage_in_st_blocks = 0;  // aggregate disk usage by files

   XrdSysMutex  m_queue_mutex;        // mutex shared between queues
   unsigned int m_queue_swap_u1 = 0u; // identifier of current swap cycle

   DataFsState &m_fs_state;
   XrdOss      &m_oss;

   // Requests for File opens during name-space scans. Such LFNs are processed
   // with some priority
   struct LfnCondRecord
   {
      const std::string &f_lfn;
      XrdSysCondVar     &f_cond;
      bool               f_checked = false;
   };

   XrdSysMutex              m_dir_scan_mutex;
   std::list<LfnCondRecord> m_dir_scan_open_requests;
   int                      m_dir_scan_check_counter;
   bool                     m_dir_scan_in_progress = false;

   void process_inter_dir_scan_open_requests(FsTraversal &fst);
   void cross_check_or_process_oob_lfn(const std::string &lfn, FsTraversal &fst);
   long long get_file_usage_bytes_to_remove(const DataFsPurgeshot &ps, long long previous_file_usage, int logLeve);

public:
   ResourceMonitor(XrdOss& oss);
   ~ResourceMonitor();

   // --- Initial scan, building of DirState tree

   void scan_dir_and_recurse(FsTraversal &fst);
   bool perform_initial_scan();

   // --- Event registration

   int register_file_open(const std::string& filename, time_t open_timestamp, bool existing_file) {
      // Simply return a token, we will resolve it in the actual processing of the queue.
      XrdSysMutexHelper _lock(&m_queue_mutex);
      int token_id;
      if ( ! m_access_tokens_free_slots.empty()) {
         token_id = m_access_tokens_free_slots.back();
         m_access_tokens_free_slots.pop_back();
         m_access_tokens[token_id].m_filename = filename;
         m_access_tokens[token_id].m_last_queue_swap_u1 = m_queue_swap_u1 - 1;
      } else {
         token_id = (int) m_access_tokens.size();
         m_access_tokens.push_back({filename, m_queue_swap_u1 - 1});
      }

      m_file_open_q.push(token_id, {open_timestamp, existing_file});
      return token_id;
   }

   void register_file_update_stats(int token_id, const Stats& stats) {
      XrdSysMutexHelper _lock(&m_queue_mutex);
      AccessToken &at = token(token_id);
      // Check if this is the first update within this queue swap cycle.
      if (at.m_last_queue_swap_u1 != m_queue_swap_u1) {
         m_file_update_stats_q.push(token_id, stats);
         at.m_last_queue_swap_u1 = m_queue_swap_u1;
         at.m_last_write_queue_pos = m_file_update_stats_q.write_queue_size() - 1;
      } else {
         Stats &existing_stats = m_file_update_stats_q.write_record(at.m_last_write_queue_pos);
         existing_stats.AddUp(stats);
      }
      // Optionally, one could return "scaler" to moodify stat-reporting
      // frequency in the file ... if it comes too often or too rarely.
      // See also the logic for determining reporting interval (in N_bytes_read)
      // in File::Open().
   }

   void register_file_close(int token_id, time_t close_timestamp, const Stats& full_stats) {
      XrdSysMutexHelper _lock(&m_queue_mutex);
      m_file_close_q.push(token_id, {close_timestamp, full_stats});
   }

   // deletions can come from purge and from direct requests (Cache::UnlinkFile), the latter
   // also covering the emergency shutdown of a file.
   void register_file_purge(DirState* target, long long size_in_st_blocks) {
      XrdSysMutexHelper _lock(&m_queue_mutex);
      m_file_purge_q1.push(target, {size_in_st_blocks, 1});
   }
   void register_multi_file_purge(DirState* target, long long size_in_st_blocks, int n_files) {
      XrdSysMutexHelper _lock(&m_queue_mutex);
      m_file_purge_q1.push(target, {size_in_st_blocks, n_files});
   }
   void register_multi_file_purge(const std::string& target, long long size_in_st_blocks, int n_files) {
      XrdSysMutexHelper _lock(&m_queue_mutex);
      m_file_purge_q2.push(target, {size_in_st_blocks, n_files});
   }
   void register_file_purge(const std::string& filename, long long size_in_st_blocks) {
      XrdSysMutexHelper _lock(&m_queue_mutex);
      m_file_purge_q3.push(filename, size_in_st_blocks);
   }

   // void register_dir_purge(DirState* target);
   // target assumed to be empty at this point, triggered by a file_purge removing the last file in it.
   // hmmh, this is actually tricky ... who will purge the dirs? we should now at export-to-vector time
   // and can prune leaf directories. This might fail if a file has been created in there in the meantime, which is ok.
   // However, is there a race condition between rmdir and creation of a new file in that dir? Ask Andy.

   // --- Helpers for event processing and actions

   AccessToken& token(int i) { return m_access_tokens[i]; }

   // --- Actions

   int  process_queues();

   void heart_beat();

   // --- Helpers for export of DirState vector snapshot.

   void fill_sshot_vec_children(const DirState &parent_ds,
                                int parent_idx,
                                std::vector<DirStateElement> &vec,
                                int max_depth);

   void fill_pshot_vec_children(const DirState &parent_ds,
                                int parent_idx,
                                std::vector<DirPurgeElement> &vec,
                                int max_depth);

   // Interface to other part of XCache -- note the CamelCase() notation.
   void CrossCheckIfScanIsInProgress(const std::string &lfn, XrdSysCondVar &cond);

   // main function, steers startup then enters heart_beat. does not die.
   void init_before_main();      // called from startup thread / configuration processing
   void main_thread_function();  // run in dedicated thread

   XrdSysCondVar  m_purge_task_cond  {0};
   // The following variables are set under the above lock, purge task signals to heart_beat.
   time_t         m_purge_task_start {0};
   time_t         m_purge_task_end   {0};
   bool           m_purge_task_active   {false}; // from the perspective of heart-beat, set only in heartbeat
   bool           m_purge_task_complete {false}; // from the perspective of the task, reset in heartbeat, set in task
   // When m_purge_task_active == true, DirState entries are not removed from the tree to
   // allow purge thread to report cleared files directly via DirState ptr.
   // Note, DirState removal happens during stat propagation traversal.

   // Purge helpers etc.
   void update_vs_and_file_usage_info();
   void perform_purge_check(bool purge_cold_files, int tl);

   void perform_purge_task(DataFsPurgeshot &ps);
   void perform_purge_task_cleanup();
};

}

#endif
