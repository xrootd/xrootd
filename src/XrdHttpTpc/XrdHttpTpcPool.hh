#ifndef __XRDHTTPTPCPOOL_HH__
#define __XRDHTTPTPCPOOL_HH__

#include <curl/curl.h>

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "XrdHttpTpcPMarkManager.hh"

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
        TPCRequest(const std::string &label, const int scitag, CURL *handle) : m_label(label), m_scitag(scitag), m_curl(handle) {}

        int WaitFor(std::chrono::steady_clock::duration);
        CURL *GetHandle() const;
        std::string GetLabel() const;
        int GetScitag() const;
        std::string GetRemoteConnDesc();
        void SetActive();
        void SetDone(int status, const std::string &msg);
        bool IsActive() const;
        void Cancel();
        void UpdateRemoteConnDesc();
        static std::string GenerateIdentifier(const std::string& label, const char *vorg, const int scitag);

       private:
        std::atomic<bool> m_active{false};
        int m_status{-1};
        std::string m_conn_list;
        std::mutex m_conn_mutex;
        std::atomic<off_t> m_progress_offset{0};
        // Label assigned to the request. Determines which queue it will be placed into.
        // A queue with matching identifier is created if it does not already exists.
        std::string m_label;
        int m_scitag;
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
        TPCQueue(const std::string &identifier, TPCRequestManager &parent) : m_identifier(identifier), m_parent(parent) {}

        bool Produce(TPCRequest &handler);
        TPCRequest *TryConsume();
        TPCRequest *ConsumeUntil(std::chrono::steady_clock::duration dur, TPCWorker *worker);
        void Done(TPCWorker *);
        bool IsDone() const { return m_done; }

       private:
        class TPCWorker final {
           public:
            TPCWorker(const std::string &label, int scitag, TPCQueue &queue);
            TPCWorker(const TPCWorker &) = delete;

            void Run();
            static void RunStatic(TPCWorker *myself);

            bool IsIdle() const { return m_idle; }
            void SetIdle(bool idle) { m_idle = idle; }
            std::condition_variable m_cv;

            static int closesocket_callback(void *clientp, curl_socket_t fd);
            static int opensocket_callback(void *clientp, curlsocktype purpose, struct curl_sockaddr *address);
            static int sockopt_callback(void *clientp, curl_socket_t curlfd, curlsocktype purpose);
            std::string getLabel() const { return m_label; }

           private:
            bool RunCurl(CURLM *multi_handle, TPCRequest &request);

            bool m_idle{false};
            // Label for this worker. Always set to the m_identifier of the queue it serves.
            const std::string m_label;
            TPCQueue &m_queue;
            XrdNetPMark *m_pmark_handle;
            XrdHttpTpc::PMarkManager m_pmark_manager;
        };

        static const long CONNECT_TIMEOUT = 60;
        bool m_done{false};
        // Unique identifier for this queue, in the format: "tpc_<vorg>_<scitag>".
        const std::string m_identifier;
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
    XrdOucEnv &m_xrdEnv;
};

}  // namespace TPC

#endif  // __XRDHTTPTPCPOOL_HH__
