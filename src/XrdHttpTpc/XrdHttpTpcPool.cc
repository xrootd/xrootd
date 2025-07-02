#include "XrdHttpTpcPool.hh"

#include <fcntl.h>

#include <XrdOuc/XrdOucEnv.hh>
#include <XrdSys/XrdSysError.hh>
#include <XrdSys/XrdSysFD.hh>
#include <algorithm>
#include <sstream>
#include <string>
#include <thread>

#include "XrdHttpTpcTPC.hh"

using namespace TPC;

decltype(TPCRequestManager::m_pool_map) TPCRequestManager::m_pool_map;
decltype(TPCRequestManager::m_init_once) TPCRequestManager::m_init_once;
decltype(TPCRequestManager::m_mutex) TPCRequestManager::m_mutex;
decltype(TPCRequestManager::m_idle_timeout) TPCRequestManager::m_idle_timeout = std::chrono::minutes(1);
unsigned TPCRequestManager::m_max_pending_ops = 20;
unsigned TPCRequestManager::m_max_workers = 20;

TPCRequestManager::TPCQueue::TPCWorker::TPCWorker(const std::string &label, int scitag, TPCQueue &queue)
    : m_label(label),
      m_queue(queue),
      m_scitag(scitag),  // use this to generate the queue identifier: queues
                         // will be based on the scitag
      m_pmark_handle((XrdNetPMark *)queue.m_parent.m_xrdEnv.GetPtr("XrdNetPMark*")),
      m_pmark_manager(m_pmark_handle, scitag, TPC::TpcType::Pull) {}

void TPCRequestManager::TPCQueue::TPCWorker::RunStatic(TPCWorker *myself) { myself->Run(); }

bool TPCRequestManager::TPCQueue::TPCWorker::RunCurl(CURLM *multi_handle, TPCRequestManager::TPCRequest &request) {
    CURLMcode mres;
    auto curl = request.GetHandle();

    curl_easy_setopt(curl, CURLOPT_CLOSESOCKETFUNCTION, closesocket_callback);
    curl_easy_setopt(curl, CURLOPT_CLOSESOCKETDATA, this);
    curl_easy_setopt(curl, CURLOPT_OPENSOCKETFUNCTION, opensocket_callback);
    curl_easy_setopt(curl, CURLOPT_OPENSOCKETDATA, this);
    curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, sockopt_callback);
    curl_easy_setopt(curl, CURLOPT_SOCKOPTDATA, this);

    mres = curl_multi_add_handle(multi_handle, curl);
    if (mres) {
        std::stringstream ss;
        ss << "Failed to add transfer to libcurl multi-handle: HTTP library "
              "failure="
           << curl_multi_strerror(mres);
        m_queue.m_parent.m_log.Log(LogMask::Error, "TPCWorker", ss.str().c_str());
        request.SetDone(500, ss.str());
        return false;
    }
    request.SetActive();

    CURLcode res = static_cast<CURLcode>(-1);
    int running_handles = 1;
    const int update_interval{1};
    time_t now = time(NULL);
    time_t last_update = now - update_interval;  // Inorder to always fetch on first pass

    auto fail_and_exit = [&](int code, const std::string &msg) -> bool {
        curl_multi_remove_handle(multi_handle, curl);
        m_queue.m_parent.m_log.Log(code >= 500 ? LogMask::Error : LogMask::Info, "TPCWorker", msg.c_str());
        request.SetDone(code, msg);
        return false;
    };

    do {
        mres = curl_multi_perform(multi_handle, &running_handles);
        if (mres != CURLM_OK) {
            return fail_and_exit(500, "Internal curl multi-handle error: " + std::string(curl_multi_strerror(mres)));
        }

        now = time(NULL);
        if (now - last_update >= update_interval) {
            request.UpdateRemoteConnDesc();
            last_update = now;
        }

        CURLMsg *msg;
        do {
            int msgq = 0;
            msg = curl_multi_info_read(multi_handle, &msgq);
            if (msg && (msg->msg == CURLMSG_DONE)) {
                res = msg->data.result;
                break;
            }
        } while (msg);

        mres = curl_multi_wait(multi_handle, NULL, 0, 1000, nullptr);
        if (mres != CURLM_OK) {
            return fail_and_exit(500, "Error during curl_multi_wait: " + std::string(curl_multi_strerror(mres)));
        }

        if (!request.IsActive()) {
            return fail_and_exit(499, "Transfer cancelled");
        }

    } while (running_handles);

    request.UpdateRemoteConnDesc();

    if (res == static_cast<CURLcode>(-1)) {
        return fail_and_exit(500, "Internal state error in libcurl - no transfer results returned");
    }

    curl_multi_remove_handle(multi_handle, curl);
    request.SetDone(res, "Transfer complete");

    return true;
}

void TPCRequestManager::TPCQueue::TPCWorker::Run() {
    m_queue.m_parent.m_log.Log(LogMask::Info, "TPCWorker", "Worker for", m_queue.m_label.c_str(), "starting");

    // Create the multi-handle and add in the current transfer to it.
    CURLM *multi_handle = curl_multi_init();
    if (!multi_handle) {
        m_queue.m_parent.m_log.Log(LogMask::Error, "TPCWorker",
                                   "Unable to create"
                                   " a libcurl multi-handle; fatal error for worker");
        m_queue.Done(this);
        return;
    }

    while (true) {
        auto request = m_queue.TryConsume();
        if (!request) {
            request = m_queue.ConsumeUntil(m_idle_timeout, this);
            if (!request) {
                m_queue.m_parent.m_log.Log(LogMask::Info, "TPCWorker", "Worker for", m_queue.m_label.c_str(), "exiting");
                break;
            }
        }
        if (!RunCurl(multi_handle, *request)) {
            m_queue.m_parent.m_log.Log(LogMask::Error, "TPCWorker",
                                       "Worker's multi-handle"
                                       " caused an internal error.  Worker immediately exiting");
            break;
        }
    }
    curl_multi_cleanup(multi_handle);
    m_queue.Done(this);
}

/******************************************************************************/
/*           s o c k o p t _ s e t c l o e x e c _ c a l l b a c k            */
/******************************************************************************/

/**
 * The callback that will be called by libcurl when the socket has been created
 * https://curl.se/libcurl/c/CURLOPT_SOCKOPTFUNCTION.html
 *
 * Note: that this callback has been replaced by the opensocket_callback as it
 *       was needed for monitoring to report what IP protocol was being used.
 *       It has been kept in case we will need this callback in the future.
 */

int TPCRequestManager::TPCQueue::TPCWorker::sockopt_callback(void *clientp, curl_socket_t curlfd, curlsocktype purpose) {
    TPCWorker *tpcWorker = (TPCWorker *)clientp;

    if (purpose == CURLSOCKTYPE_IPCXN && tpcWorker && tpcWorker->m_pmark_manager.isEnabled()) {
        // We will not reach this callback if the corresponding socket could not
        // have been connected the socket is already connected only if the
        // packet marking is enabled
        return CURL_SOCKOPT_ALREADY_CONNECTED;
    }
    return CURL_SOCKOPT_OK;
}

/******************************************************************************/
/*                   o p e n s o c k e t _ c a l l b a c k                    */
/******************************************************************************/
/**
 * The callback that will be called by libcurl when the socket is about to be
 * opened so we can capture the protocol that will be used.
 */

int TPCRequestManager::TPCQueue::TPCWorker::opensocket_callback(void *clientp, curlsocktype purpose, struct curl_sockaddr *address) {
    // Return a socket file descriptor (note the clo_exec flag will be set).
    int fd = XrdSysFD_Socket(address->family, address->socktype, address->protocol);
    // See what kind of address will be used to connect
    if (fd < 0) {
        return CURL_SOCKET_BAD;
    }
    TPCWorker *tpcWorker = (TPCWorker *)clientp;

    if (purpose == CURLSOCKTYPE_IPCXN && clientp) {
        XrdNetAddr thePeer(&(address->addr));
        //   rec->isIPv6 =  (thePeer.isIPType(XrdNetAddrInfo::IPv6)
        //                   && !thePeer.isMapped());
        std::stringstream connectErrMsg;

        if (!tpcWorker->m_pmark_manager.connect(fd, &(address->addr), address->addrlen, CONNECT_TIMEOUT, connectErrMsg)) {
            tpcWorker->m_queue.m_parent.m_log.Emsg("TPCWorker:", "Unable to connect socket:", connectErrMsg.str().c_str());
            return CURL_SOCKET_BAD;
        }

        tpcWorker->m_pmark_manager.startTransfer();
        tpcWorker->m_pmark_manager.beginPMarks();
    }
    return fd;
}

/******************************************************************************/
/*                   c l o s e s o c k e t _ c a l l b a c k */
/******************************************************************************/
/**
 * The callback that will be called by libcurl when the socket is about to be
 * closed so we can send the done packet marking information.
 *
 */

int TPCRequestManager::TPCQueue::TPCWorker::closesocket_callback(void *clientp, curl_socket_t fd) {
    TPCWorker *tpcWorker = (TPCWorker *)clientp;

    tpcWorker->m_pmark_manager.endPmark(fd);
    return close(fd);
}

void TPCRequestManager::TPCQueue::Done(TPCWorker *worker) {
    std::unique_lock<std::mutex> lock(m_mutex);
    auto it = std::remove_if(m_workers.begin(), m_workers.end(), [&](std::unique_ptr<TPCWorker> &other) { return other.get() == worker; });
    m_workers.erase(it, m_workers.end());

    if (m_workers.empty()) {
        m_done = true;
        lock.unlock();
        m_parent.Done(m_label);
    }
}

void TPCRequestManager::Done(const std::string &ident) {
    m_log.Log(LogMask::Info, "TPCRequestManager", "Worker pool", ident.c_str(), "is idle and all workers have exited.");
    std::unique_lock<std::shared_mutex> lock(m_mutex);

    auto iter = m_pool_map.find(ident);
    if (iter != m_pool_map.end()) {
        m_pool_map.erase(iter);
    }
}

// Produce a request for processing.  If the queue is full, the request will
// be rejected and false will be returned.
//
// Implementation notes:
// - If a worker is idle, it will be woken up to process the request.
// - If no workers are idle, a new worker will be created to process the
//   request.
// - If the maximum number of workers is reached, the request will be queued
//   until a worker is available.
// - If the maximum number of pending operations is reached, the request will
//   be rejected.
// - If there are multiple idle workers, the oldest worker will be woken.  This
//   causes the newest workers to be idle for as long as possible and
//   potentially exit due to lack of work.  This is done to reduce the number of
//   "mostly idle" workers in the thread pool.
bool TPCRequestManager::TPCQueue::Produce(TPCRequest &handler) {
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_ops.size() == m_max_pending_ops) {
        m_parent.m_log.Log(LogMask::Warning, "TPCQueue", "Queue is full; rejecting request");
        return false;
    }

    m_ops.push_back(&handler);
    for (auto &worker : m_workers) {
        if (worker->IsIdle()) {
            worker->m_cv.notify_one();
            return true;
        }
    }

    if (m_workers.size() < m_max_workers) {
        auto worker = std::make_unique<TPCRequestManager::TPCQueue::TPCWorker>(handler.GetIdentifier(), handler.GetScitag(), *this);
        std::thread t(TPCRequestManager::TPCQueue::TPCWorker::RunStatic, worker.get());
        t.detach();
        m_workers.push_back(std::move(worker));
    }
    lk.unlock();

    return true;
}

TPCRequestManager::TPCRequest *TPCRequestManager::TPCQueue::TryConsume() {
    std::unique_lock<std::mutex> lk(m_mutex);
    if (m_ops.size() == 0) {
        return nullptr;
    }

    auto result = m_ops.front();
    m_ops.pop_front();

    return result;
}

// Wait for a request to be available for processing, or until the duration
// has elapsed.
//
// Returns the request that is available, or nullptr if the duration has
// elapsed.
TPCRequestManager::TPCRequest *TPCRequestManager::TPCQueue::ConsumeUntil(std::chrono::steady_clock::duration dur, TPCWorker *worker) {
    std::unique_lock<std::mutex> lk(m_mutex);
    worker->SetIdle(true);
    worker->m_cv.wait_for(lk, dur, [&] { return m_ops.size() > 0; });
    worker->SetIdle(false);
    if (m_ops.size() == 0) {
        return nullptr;
    }

    auto result = m_ops.front();
    m_ops.pop_front();

    return result;
}

void TPCRequestManager::TPCRequest::SetActive() { m_active.store(true, std::memory_order_relaxed); }

void TPCRequestManager::TPCRequest::Cancel() { m_active.store(false, std::memory_order_relaxed); }

CURL *TPCRequestManager::TPCRequest::GetHandle() const { return m_curl; }

int TPCRequestManager::TPCRequest::GetScitag() const { return m_scitag; }

bool TPCRequestManager::TPCRequest::IsActive() const { return m_active.load(std::memory_order_relaxed); }

std::string TPCRequestManager::TPCRequest::GetIdentifier() const {
    std::stringstream ss;
    ss << m_ident << "_" << m_scitag;
    return ss.str();
}

// Logic from State::GetConnectionDescription
void TPCRequestManager::TPCRequest::UpdateRemoteConnDesc() {
#if LIBCURL_VERSION_NUM >= 0x071500
    // Retrieve IP address and port from the curl handle
    const char *curl_ip = nullptr;
    CURLcode rc = curl_easy_getinfo(m_curl, CURLINFO_PRIMARY_IP, &curl_ip);
    if (rc != CURLE_OK || !curl_ip) {
        return;  // Failed to get IP, cannot update connection descriptor
    }

    long curl_port = 0;
    rc = curl_easy_getinfo(m_curl, CURLINFO_PRIMARY_PORT, &curl_port);
    if (rc != CURLE_OK || curl_port == 0) {
        return;  // Failed to get port, cannot update connection descriptor
    }

    // Format the connection string according to HTTP-TPC spec
    // IPv6 addresses must be enclosed in square brackets
    std::stringstream ss;
    if (strchr(curl_ip, ':') == nullptr) {
        ss << "tcp:" << curl_ip << ":" << curl_port;
    } else {
        ss << "tcp:[" << curl_ip << "]:" << curl_port;
    }

    {
        std::unique_lock<std::mutex> lock(m_conn_mutex);
        m_conn_list = ss.str();
    }
#else
    // For older libcurl versions, do nothing
    return;
#endif
}

std::string TPCRequestManager::TPCRequest::GetRemoteConnDesc() {
    std::unique_lock<std::mutex> lock(m_conn_mutex);
    return m_conn_list;
}

void TPCRequestManager::TPCRequest::SetDone(int status, const std::string &msg) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_status = status;
    m_message = msg;
    m_cv.notify_one();
}

int TPCRequestManager::TPCRequest::WaitFor(std::chrono::steady_clock::duration dur) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait_for(lock, dur, [&] { return m_status >= 0; });

    return m_status;
}

TPCRequestManager::TPCRequestManager(XrdOucEnv &xrdEnv, XrdSysError &eDest) : m_log(eDest), m_xrdEnv(xrdEnv) {}

void TPCRequestManager::SetWorkerIdleTimeout(std::chrono::steady_clock::duration dur) { m_idle_timeout = dur; }

// Send a request to a worker for processing.  If the worker is not available,
// the request will be queued until a worker is available.  If the queue is
// full, the request will be rejected and false will be returned.
bool TPCRequestManager::Produce(TPCRequestManager::TPCRequest &handler) {
    std::shared_ptr<TPCQueue> queue;
    // Get the queue from our per-label map.  To avoid a race condition,
    // if the queue we get has already been shut down, we release the lock
    // and try again (with the expectation that the queue will eventually
    // get the lock and remove itself from the map).
    while (true) {
        m_mutex.lock_shared();
        std::lock_guard<std::shared_mutex> guard{m_mutex, std::adopt_lock};
        auto iter = m_pool_map.find(handler.GetIdentifier());
        if (iter != m_pool_map.end()) {
            if (!iter->second->IsDone()) {
                queue = iter->second;
                break;
            }
        } else {
            break;
        }
    }
    if (!queue) {
        auto created_queue = false;
        std::string queue_name = "";
        {
            std::lock_guard<std::shared_mutex> guard(m_mutex);
            auto iter = m_pool_map.find(handler.GetIdentifier());
            if (iter == m_pool_map.end()) {
                queue = std::make_shared<TPCQueue>(handler.GetIdentifier(), *this);
                m_pool_map.insert(iter, {handler.GetIdentifier(), queue});
                created_queue = true;
                queue_name = handler.GetIdentifier();
            } else {
                queue = iter->second;
            }
        }
        if (created_queue) {
            m_log.Log(LogMask::Info, "RequestManager", "Created new TPC request queue for", queue_name.c_str());
        }
    }

    return queue->Produce(handler);
}
