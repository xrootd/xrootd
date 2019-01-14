
#include <algorithm>
#include <sstream>
#include <stdexcept>

#include "XrdVersion.hh"
#include "XrdHttp/XrdHttpExtHandler.hh"
#include "XrdSfs/XrdSfsInterface.hh"

#include <curl/curl.h>

#include "XrdTpcState.hh"
#include "XrdTpcStream.hh"

using namespace TPC;


State::~State() {
    if (m_headers) {
            curl_slist_free_all(m_headers);
            m_headers = NULL;
            if (m_curl) {curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);}
    }
}


void State::Move(State &other)
{
    m_push = other.m_push;
    m_recv_status_line = other.m_recv_status_line;
    m_recv_all_headers = other.m_recv_all_headers;
    m_offset = other.m_offset;
    m_start_offset = other.m_start_offset;
    m_status_code = other.m_status_code;
    m_content_length = other.m_content_length;
    m_stream = other.m_stream;
    m_curl = other.m_curl;
    m_headers = other.m_headers;
    m_headers_copy = other.m_headers_copy;
    m_resp_protocol = m_resp_protocol;

    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, this);
    if (m_push) {
        curl_easy_setopt(m_curl, CURLOPT_READDATA, this);
    } else {
        curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, this);
    }
    other.m_headers_copy.clear();
    other.m_curl = NULL;
    other.m_headers = NULL;
    other.m_stream = NULL;
}


bool State::InstallHandlers(CURL *curl) {
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "xrootd-tpc/" XrdVERSION);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &State::HeaderCB);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
    if (m_push) {
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, &State::ReadCB);
        curl_easy_setopt(curl, CURLOPT_READDATA, this);
        struct stat buf;
        if (SFS_OK == m_stream->Stat(&buf)) {
            curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, buf.st_size);
        }
    } else {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &State::WriteCB);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    }
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Require a minimum speed from the transfer: must move at least 1MB every 2 minutes
    // (roughly 8KB/s).
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 2*60);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024*1024);
    return true;
}

/**
 * Handle the 'Copy-Headers' feature
 */
void State::CopyHeaders(XrdHttpExtReq &req) {
    struct curl_slist *list = NULL;
    for (std::map<std::string, std::string>::const_iterator hdr_iter = req.headers.begin();
         hdr_iter != req.headers.end();
         hdr_iter++) {
        if (hdr_iter->first == "Copy-Header") {
            list = curl_slist_append(list, hdr_iter->second.c_str());
            m_headers_copy.emplace_back(hdr_iter->second);
        }
        // Note: len("TransferHeader") == 14
        if (!hdr_iter->first.compare(0, 14, "TransferHeader")) {
            std::stringstream ss;
            ss << hdr_iter->first.substr(14) << ": " << hdr_iter->second;
            list = curl_slist_append(list, ss.str().c_str());
            m_headers_copy.emplace_back(ss.str());
        }
    }
    if (list != NULL) {
        curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, list);
        m_headers = list;
    }
}

void State::ResetAfterRequest() {
    m_offset = 0;
    m_status_code = -1;
    m_content_length = -1;
    m_recv_all_headers = false;
    m_recv_status_line = false;
}

size_t State::HeaderCB(char *buffer, size_t size, size_t nitems, void *userdata)
{
    State *obj = static_cast<State*>(userdata);
    std::string header(buffer, size*nitems);
    return obj->Header(header);
}

int State::Header(const std::string &header) {
    //printf("Recieved remote header (%d, %d): %s", m_recv_all_headers, m_recv_status_line, header.c_str());
    if (m_recv_all_headers) {  // This is the second request -- maybe processed a redirect?
        m_recv_all_headers = false;
        m_recv_status_line = false;
    }
    if (!m_recv_status_line) {
        std::stringstream ss(header);
        std::string item;
        if (!std::getline(ss, item, ' ')) return 0;
        m_resp_protocol = item;
        //printf("\n\nResponse protocol: %s\n", m_resp_protocol.c_str());
        if (!std::getline(ss, item, ' ')) return 0;
        try {
            m_status_code = std::stol(item);
        } catch (...) {
            return 0;
        }
        m_recv_status_line = true;
    } else if (header.size() == 0 || header == "\n" || header == "\r\n") {
        m_recv_all_headers = true;
    }
    else if (header != "\r\n") {
        // Parse the header
        std::size_t found = header.find(":");
        if (found != std::string::npos) {
            std::string header_name = header.substr(0, found);
            std::transform(header_name.begin(), header_name.end(), header_name.begin(), ::tolower);
            std::string header_value = header.substr(found+1);
            if (header_name == "content-length")
            {
                try {
                    m_content_length = std::stoll(header_value);
                } catch (...) {
                    // Header unparseable -- not a great sign, fail request.
                    //printf("Content-length header unparseable\n");
                    return 0;
                }
            }
        } else {
            // Non-empty header that isn't the status line, but no ':' present --
            // malformed request?
            //printf("Malformed header: %s\n", header.c_str());
            return 0;
        }
    }
    return header.size();
}

size_t State::WriteCB(void *buffer, size_t size, size_t nitems, void *userdata) {
    State *obj = static_cast<State*>(userdata);
    if (obj->GetStatusCode() < 0) {
        return 0;
     }  // malformed request - got body before headers.
    if (obj->GetStatusCode() >= 400) {return 0;}  // Status indicates failure.
    return obj->Write(static_cast<char*>(buffer), size*nitems);
}

int State::Write(char *buffer, size_t size) {
    int retval = m_stream->Write(m_start_offset + m_offset, buffer, size);
    if (retval == SFS_ERROR) {
        return -1;
    }
    m_offset += retval;
    return retval;
}

size_t State::ReadCB(void *buffer, size_t size, size_t nitems, void *userdata) {
    State *obj = static_cast<State*>(userdata);
    if (obj->GetStatusCode() < 0) {return 0;}  // malformed request - got body before headers.
    if (obj->GetStatusCode() >= 400) {return 0;}  // Status indicates failure.
    return obj->Read(static_cast<char*>(buffer), size*nitems);
}

int State::Read(char *buffer, size_t size) {
    int retval = m_stream->Read(m_start_offset + m_offset, buffer, size);
    if (retval == SFS_ERROR) {
        return -1;
    }
    m_offset += retval;
    //printf("Read a total of %ld bytes.\n", m_offset);
    return retval;
}

State *State::Duplicate() {
    CURL *curl = curl_easy_duphandle(m_curl);
    if (!curl) {
        throw std::runtime_error("Failed to duplicate existing curl handle.");
    }

    State *state = new State(0, *m_stream, curl, m_push);

    if (m_headers) {
        state->m_headers_copy.reserve(m_headers_copy.size());
        for (std::vector<std::string>::const_iterator header_iter = m_headers_copy.begin();
             header_iter != m_headers_copy.end();
             header_iter++) {
            state->m_headers = curl_slist_append(state->m_headers, header_iter->c_str());
            state->m_headers_copy.push_back(*header_iter);
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, NULL);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, state->m_headers);
    }

    return state;
}

void State::SetTransferParameters(off_t offset, size_t size) {
    m_start_offset = offset;
    m_offset = 0;
    m_content_length = size;
    std::stringstream ss;
    ss << offset << "-" << (offset+size-1);
    curl_easy_setopt(m_curl, CURLOPT_RANGE, ss.str().c_str());
}

int State::AvailableBuffers() const
{
    return m_stream->AvailableBuffers();
}

void State::DumpBuffers() const
{
    m_stream->DumpBuffers();
}

bool State::Finalize()
{
    return m_stream->Finalize();
}

