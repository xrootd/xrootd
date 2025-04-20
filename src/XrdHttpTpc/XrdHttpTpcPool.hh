#ifndef __XRDHTTPTPCPOOL_HH__
#define __XRDHTTPTPCPOOL_HH__

#include <curl/curl.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

// Forward dec'ls
class XrdOucEnv;
class XrdSysError;

// A pool manager for TPC requests
//
// The manager maintains a set of worker pools, one for each distinct identifier
// (typically, one per organization; this prevents the mixing of transfers from
// different organizations on the same TCP socket).  Each TPC transfer submitted
// must have an identifier; the transfer is then queued for the appropriate pool
// and subsequently executed by one of the worker threads.
//
// Transfers are packed to as few workers as possible in an attempt to reduce
// the number of TCP connections; however, if the transfer is not picked up
// quickly, a new worker will be spawned.  Idle workers will auto-shutdown; if
// not used, the pool will have no running threads.
namespace TPC {

class TPCRequestManager final {
   public:
    class TPCRequest {
       public:
        TPCRequest(const std::string &ident, CURL *handle) : m_ident(ident), m_curl(handle) {}

        int WaitFor(std::chrono::steady_clock::duration);

        CURL *GetHandle() const { return m_curl; }
        void SetProgress(off_t offset);
        void SetDone(int status, const std::string &msg);
        const std::string &GetIdentifier() const { return m_ident; }
        bool IsActive() const { return m_active.load(std::memory_order_relaxed); }
        void Cancel() {  // TODO: implement.
        }
        std::string GetResults() const { return m_message; }
        off_t GetProgress() const { return m_progress_offset.load(std::memory_order_relaxed); }

       private:
        std::atomic<bool> m_active{false};
        int m_status{-1};
        std::atomic<off_t> m_progress_offset{0};
        std::string m_ident;
        CURL *m_curl;
        std::condition_variable m_cv;
        std::mutex m_mutex;
        std::string m_message;
    };

    TPCRequestManager(XrdOucEnv &xrdEnv, XrdSysError &eDest);

    bool Produce(TPCRequest &handler);

    void SetWorkerIdleTimeout(std::chrono::steady_clock::duration dur);
    void SetMaxWorkers(unsigned max_workers) { m_max_workers = max_workers; }
    void SetMaxIdleRequests(unsigned max_pending_ops) { m_max_pending_ops = max_pending_ops; }

   private:
    class TPCQueue {
        class TPCWorker;

       public:
        TPCQueue(const std::string &ident, TPCRequestManager &parent) : m_label(ident), m_parent(parent) {}

        bool Produce(TPCRequest &handler);
        TPCRequest *TryConsume();
        TPCRequest *ConsumeUntil(std::chrono::steady_clock::duration dur, TPCWorker *worker);
        void Done(TPCWorker *);
        bool IsDone() const { return m_done; }

       private:
        class TPCWorker final {
           public:
            TPCWorker(const std::string &label, TPCQueue &queue);
            TPCWorker(const TPCWorker &) = delete;

            void Run();
            static void RunStatic(TPCWorker *myself);

            bool IsIdle() const { return m_idle; }
            void SetIdle(bool idle) { m_idle = idle; }
            std::condition_variable m_cv;

           private:
            bool RunCurl(CURLM *multi_handle, TPCRequest &request);

            bool m_idle{false};
            const std::string m_label;
            TPCQueue &m_queue;
        };

        bool m_done{false};
        const std::string m_label;
        std::vector<std::unique_ptr<TPCWorker>> m_workers;
        std::deque<TPCRequest *> m_ops;
        std::mutex m_mutex;
        TPCRequestManager &m_parent;
    };

    void Done(const std::string &ident);

    static std::shared_mutex m_mutex;

    XrdSysError &m_log;  // Log object for the request manager

    static std::chrono::steady_clock::duration m_idle_timeout;
    static std::unordered_map<std::string, std::shared_ptr<TPCQueue>> m_pool_map;
    static unsigned m_max_pending_ops;
    static unsigned m_max_workers;
    static std::once_flag m_init_once;
};

}  // namespace TPC

#endif  // __XRDHTTPTPCPOOL_HH__
