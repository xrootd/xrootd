/**
 * Implementation of multi-stream HTTP transfers for the TPCHandler
 */

#ifdef XRD_CHUNK_RESP

#include "XrdTpcTPC.hh"
#include "XrdTpcState.hh"
#include "XrdTpcCurlMulti.hh"

#include "XrdSys/XrdSysError.hh"

#include <curl/curl.h>

#include <sstream>
#include <stdexcept>


using namespace TPC;

class CurlHandlerSetupError : public std::runtime_error {
public:
    CurlHandlerSetupError(const std::string &msg) :
        std::runtime_error(msg)
    {}

    virtual ~CurlHandlerSetupError() throw () {}
};

namespace {
class MultiCurlHandler {
public:
    MultiCurlHandler(std::vector<State*> &states, XrdSysError &log) :
        m_handle(curl_multi_init()),
        m_states(states),
        m_log(log)
    {
        if (m_handle == NULL) {
            throw CurlHandlerSetupError("Failed to initialize a libcurl multi-handle");
        }
        m_avail_handles.reserve(states.size());
        m_active_handles.reserve(states.size());
        for (std::vector<State*>::const_iterator state_iter = states.begin();
             state_iter != states.end();
             state_iter++) {
            m_avail_handles.push_back((*state_iter)->GetHandle());
        }
    }

    ~MultiCurlHandler()
    {
        if (!m_handle) {return;}
        for (std::vector<CURL *>::const_iterator it = m_active_handles.begin();
             it != m_active_handles.end();
             it++) {
            curl_multi_remove_handle(m_handle, *it);
            curl_easy_cleanup(*it);
        }
        for (std::vector<CURL *>::const_iterator it = m_avail_handles.begin();
             it != m_avail_handles.end();
             it++) {
            curl_easy_cleanup(*it);
        }
        curl_multi_cleanup(m_handle);
    }

    MultiCurlHandler(const MultiCurlHandler &) = delete;

    CURLM *Get() const {return m_handle;}

    void FinishCurlXfer(CURL *curl) {
        CURLMcode mres = curl_multi_remove_handle(m_handle, curl);
        if (mres) {
            std::stringstream ss;
            ss << "Failed to remove transfer from set: "
               << curl_multi_strerror(mres);
            throw std::runtime_error(ss.str());
        }
        for (std::vector<State*>::iterator state_iter = m_states.begin();
             state_iter != m_states.end();
             state_iter++) {
            if (curl == (*state_iter)->GetHandle()) {
                (*state_iter)->ResetAfterRequest();
                break;
            }
        }
        for (std::vector<CURL *>::iterator iter = m_active_handles.begin();
             iter != m_active_handles.end();
             ++iter)
        {
            if (*iter == curl) {
                m_active_handles.erase(iter);
                break;
            }
        }
        m_avail_handles.push_back(curl);
    }

    off_t StartTransfers(off_t current_offset, off_t content_length, size_t block_size,
                         int &running_handles) {
         bool started_new_xfer = false;
         do {
             size_t xfer_size = std::min(content_length - current_offset, static_cast<off_t>(block_size));
             if (xfer_size == 0) {return current_offset;}
             if (!(started_new_xfer = StartTransfer(current_offset, xfer_size))) {
                 // In this case, we need to start new transfers but weren't able to.
                 if (running_handles == 0) {
                     if (!CanStartTransfer(true)) {
                         m_log.Emsg("StartTransfers", "Unable to start transfers.");
                     }
                 }
                 break;
             } else {
                 running_handles += 1;
             }
             current_offset += xfer_size;
        } while (true);
        return current_offset;
    }

private:

    bool StartTransfer(off_t offset, size_t size) {
        if (!CanStartTransfer(false)) {return false;}
        for (std::vector<CURL*>::const_iterator handle_it = m_avail_handles.begin();
             handle_it != m_avail_handles.end();
             handle_it++) {
            for (std::vector<State*>::iterator state_it = m_states.begin();
                 state_it != m_states.end();
                 state_it++) {
                if ((*state_it)->GetHandle() == *handle_it) {  // This state object represents an idle handle.
                    (*state_it)->SetTransferParameters(offset, size);
                    ActivateHandle(**state_it);
                    return true;
                }
            }
        }
        return false;
    }

    void ActivateHandle(State &state) {
        CURL *curl = state.GetHandle();
        m_active_handles.push_back(curl);
        CURLMcode mres;
        mres = curl_multi_add_handle(m_handle, curl);
        if (mres) {
            std::stringstream ss;
            ss << "Failed to add transfer to libcurl multi-handle"
               << curl_multi_strerror(mres);
            throw std::runtime_error(ss.str());
        }
        for (auto iter = m_avail_handles.begin();
             iter != m_avail_handles.end();
             ++iter)
        {
            if (*iter == curl) {
                m_avail_handles.erase(iter);
                break;
            }
        }
    }

    bool CanStartTransfer(bool log_reason) const {
        size_t idle_handles = m_avail_handles.size();
        size_t transfer_in_progress = 0;
        for (std::vector<State*>::const_iterator state_iter = m_states.begin();
             state_iter != m_states.end();
             state_iter++) {
            for (std::vector<CURL*>::const_iterator handle_iter = m_active_handles.begin();
                 handle_iter != m_active_handles.end();
                 handle_iter++) {
                if (*handle_iter == (*state_iter)->GetHandle()) {
                    transfer_in_progress += (*state_iter)->BodyTransferInProgress();
                    break;
                }
            }
        }
        if (!idle_handles) {
            if (log_reason) {
                m_log.Emsg("CanStartTransfer", "Unable to start transfers as no idle CURL handles are available.");
            }
            return false;
        }
        ssize_t available_buffers = m_states[0]->AvailableBuffers();
        // To be conservative, set aside buffers for any transfers that have been activated
        // but don't have their first responses back yet.
        available_buffers -= (m_active_handles.size() - transfer_in_progress);
        if (log_reason && (available_buffers == 0)) {
            std::stringstream ss;
            ss << "Unable to start transfers as no buffers are available.  Available buffers: " <<
                m_states[0]->AvailableBuffers() << ", Active curl handles: " << m_active_handles.size()
                << ", Transfers in progress: " << transfer_in_progress;
            m_log.Emsg("CanStartTransfer", ss.str().c_str());
            if (m_states[0]->AvailableBuffers() == 0) {
                m_states[0]->DumpBuffers();
            }
        }
        return available_buffers > 0;
    }

    CURLM *m_handle;
    std::vector<CURL *> m_avail_handles;
    std::vector<CURL *> m_active_handles;
    std::vector<State*> &m_states;
    XrdSysError         &m_log;
};
}


int TPCHandler::RunCurlWithStreamsImpl(XrdHttpExtReq &req, State &state,
                                       const char *log_prefix, size_t streams,
                                       std::vector<State*> handles)
{
    int result;
    bool success;
    CURL *curl = state.GetHandle();
    if ((result = DetermineXferSize(curl, req, state, success)) || !success) {
        return result;
    }
    off_t content_size = state.GetContentLength();
    off_t current_offset = 0;

    {
        std::stringstream ss;
        ss << "Successfully determined remote size for pull request: " << content_size;
        m_log.Emsg("ProcessPullReq", ss.str().c_str());
    }
    state.ResetAfterRequest();    

    size_t concurrency = streams * m_pipelining_multiplier;

    handles.reserve(concurrency);
    handles.push_back(new State());
    handles[0]->Move(state);
    for (size_t idx = 1; idx < concurrency; idx++) {
        handles.push_back(handles[0]->Duplicate());
    }

    // Create the multi-handle and add in the current transfer to it.
    MultiCurlHandler mch(handles, m_log);
    CURLM *multi_handle = mch.Get();

#ifdef USE_PIPELINING
    curl_multi_setopt(multi_handle, CURLMOPT_PIPELINING, 1);
    curl_multi_setopt(multi_handle, CURLMOPT_MAX_HOST_CONNECTIONS, streams);
#endif

    // Start response to client prior to the first call to curl_multi_perform
    int retval = req.StartChunkedResp(201, "Created", "Content-Type: text/plain");
    if (retval) {
        return retval;
    }

    // Start assigning transfers
    int running_handles = 0;
    current_offset = mch.StartTransfers(current_offset, content_size, m_block_size, running_handles);

    // Transfer loop: use curl to actually run the transfer, but periodically
    // interrupt things to send back performance updates to the client.
    time_t last_marker = 0;
    CURLcode res = static_cast<CURLcode>(-1);
    CURLMcode mres;
    do {
        time_t now = time(NULL);
        time_t next_marker = last_marker + m_marker_period;
        if (now >= next_marker) {
            if (SendPerfMarker(req, current_offset)) {
                return -1;
            }
            last_marker = now;
        }

        mres = curl_multi_perform(multi_handle, &running_handles);
        if (mres == CURLM_CALL_MULTI_PERFORM) {
            // curl_multi_perform should be called again immediately.  On newer
            // versions of curl, this is no longer used.
            continue;
        } else if (mres != CURLM_OK) {
            break;
        }

        // Harvest any messages, looking for CURLMSG_DONE.
        CURLMsg *msg;
        do {
            int msgq = 0;
            msg = curl_multi_info_read(multi_handle, &msgq);
            if (msg && (msg->msg == CURLMSG_DONE)) {
                CURL *easy_handle = msg->easy_handle;
                mch.FinishCurlXfer(easy_handle);
                res = msg->data.result;
                // If any requests fail, cut off the entire transfer.
                if (res != CURLE_OK) {
                    break;
                }
            }
        } while (msg);
        if (res != static_cast<CURLcode>(-1) && res != CURLE_OK) {
            m_log.Emsg(log_prefix, "Breaking loop due to failed curl transfer.");
            break;
        }

        if (running_handles < static_cast<int>(concurrency)) {
            // Issue new transfers if there is still pending work to do.
            // Otherwise, continue running until there are no handles left.
            if (current_offset != content_size) {
                current_offset = mch.StartTransfers(current_offset, content_size,
                                                    m_block_size, running_handles);
                if (!running_handles) {
                    std::stringstream ss;
                    ss << "No handles are able to run.  Streams=" << streams << ", concurrency="
                       << concurrency;
                    m_log.Emsg(log_prefix, ss.str().c_str());
                }
            } else if (running_handles == 0) {
                m_log.Emsg(log_prefix, "Unable to start new transfers; breaking loop.");
                break;
            }
        }

        int64_t max_sleep_time = next_marker - time(NULL);
        if (max_sleep_time <= 0) {
            continue;
        }
        int fd_count;
#ifdef HAVE_CURL_MULTI_WAIT
        mres = curl_multi_wait(multi_handle, NULL, 0, max_sleep_time*1000,
                               &fd_count);
#else
        mres = curl_multi_wait_impl(multi_handle, max_sleep_time*1000,
                                    &fd_count);
#endif
        if (mres != CURLM_OK) {
            m_log.Emsg(log_prefix, "Breaking transfer due to failed curl multi wait.");
            break;
        }
    } while (running_handles);

    if (mres != CURLM_OK) {
        std::stringstream ss;
        ss << "Internal libcurl multi-handle error: "
           << curl_multi_strerror(mres);
        throw std::runtime_error(ss.str());
    }

    // Harvest any messages, looking for CURLMSG_DONE.
    CURLMsg *msg;
    do {
        int msgq = 0;
        msg = curl_multi_info_read(multi_handle, &msgq);
        if (msg && (msg->msg == CURLMSG_DONE)) {
            CURL *easy_handle = msg->easy_handle;
            mch.FinishCurlXfer(easy_handle);
            if (res == CURLE_OK || res == static_cast<CURLcode>(-1))
                res = msg->data.result;  // Transfer result will be examined below.
        }
    } while (msg);

    if (res == static_cast<CURLcode>(-1)) { // No transfers returned?!?
        throw std::runtime_error("Internal state error in libcurl");
    }

    // Generate the final response back to the client.
    std::stringstream ss;
    if (res != CURLE_OK) {
        m_log.Emsg(log_prefix, "request failed when processing", curl_easy_strerror(res));
        ss << "failure: " << curl_easy_strerror(res);
    } else if (current_offset != content_size) {
        ss << "failure: Internal logic error led to early abort; current offset is " <<
              current_offset << " while full size is " << content_size;
        m_log.Emsg(log_prefix, ss.str().c_str());
    } else if (state.GetStatusCode() >= 400) {
        ss << "failure: Remote side failed with status code " << state.GetStatusCode();
        m_log.Emsg(log_prefix, "Remote server failed request", ss.str().c_str());
    } else {
        if (!handles[0]->Finalize()) {
            ss << "failure: Failed to finalize and close file handle.";
            m_log.Emsg(log_prefix, "Failed to finalize file handle");
        } else {
            ss << "success: Created";
        }
    }

    if ((retval = req.ChunkResp(ss.str().c_str(), 0))) {
        return retval;
    }
    return req.ChunkResp(NULL, 0);
}


int TPCHandler::RunCurlWithStreams(XrdHttpExtReq &req, State &state,
                                   const char *log_prefix, size_t streams)
{
    std::vector<State*> handles;
    try {
        int retval = RunCurlWithStreamsImpl(req, state, log_prefix, streams, handles);
        for (std::vector<State*>::iterator state_iter = handles.begin();
             state_iter != handles.end();
             state_iter++) {
            delete *state_iter;
        }
        return retval;
    } catch (CurlHandlerSetupError &e) {
        for (std::vector<State*>::iterator state_iter = handles.begin();
             state_iter != handles.end();
             state_iter++) {
            delete *state_iter;
        }

        m_log.Emsg(log_prefix, e.what());
        return req.SendSimpleResp(500, NULL, NULL, e.what(), 0);
    } catch (std::runtime_error &e) {
        for (std::vector<State*>::iterator state_iter = handles.begin();
             state_iter != handles.end();
             state_iter++) {
            delete *state_iter;
        }

        m_log.Emsg(log_prefix, e.what());
        std::stringstream ss;
        ss << "failure: " << e.what();
        int retval;
        if ((retval = req.ChunkResp(ss.str().c_str(), 0))) {
            return retval;
        }
        return req.ChunkResp(NULL, 0);
    }
}

#endif // XRD_CHUNK_RESP
