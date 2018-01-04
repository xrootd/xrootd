
#include <algorithm>
#include <sstream>

#include "XrdHttp/XrdHttpExtHandler.hh"
#include "XrdSfs/XrdSfsInterface.hh"

#include <curl/curl.h>

#include "XrdTpcVersion.hh"
#include "state.hh"

using namespace TPC;

State::~State() {
    if (m_headers) {
            curl_slist_free_all(m_headers);
            m_headers = nullptr;
            curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);
    }
    m_fh->close();
}

bool State::InstallHandlers(CURL *curl) {
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "xrootd-tpc/" XRDTPC_VERSION);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &State::HeaderCB);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
    if (m_push) {
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, &State::ReadCB);
        curl_easy_setopt(curl, CURLOPT_READDATA, this);
        struct stat buf;
        if (SFS_OK == m_fh->stat(&buf)) {
            curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, buf.st_size);
        }
    } else {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &State::WriteCB);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
    }
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    return true;
}

/**
 * Handle the 'Copy-Headers' feature
 */
void State::CopyHeaders(XrdHttpExtReq &req) {
    struct curl_slist *list = NULL;
    for (auto &hdr : req.headers) {
        if (hdr.first == "Copy-Header") {
            list = curl_slist_append(list, hdr.second.c_str());
        }
        // Note: len("TransferHeader") == 14
        if (!hdr.first.compare(0, 14, "TransferHeader")) {
            std::stringstream ss;
            ss << hdr.first.substr(14) << ": " << hdr.second;
            list = curl_slist_append(list, ss.str().c_str());
        }
    }
    if (list != nullptr) {
        curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, list);
        m_headers = list;
    }
}

void State::ResetAfterSize() {
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
    } else if (header.size() == 0 || header == "\n") {
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
    if (obj->GetStatusCode() < 0) {return 0;}  // malformed request - got body before headers.
    if (obj->GetStatusCode() >= 400) {return 0;}  // Status indicates failure.
    return obj->Write(static_cast<char*>(buffer), size*nitems);
}

int State::Write(char *buffer, size_t size) {
    int retval = m_fh->write(m_offset, buffer, size);
    if (retval == SFS_ERROR) {
            return -1;
    }
    m_offset += retval;
    //printf("Wrote a total of %ld bytes.\n", m_offset);
    return retval;
}

size_t State::ReadCB(void *buffer, size_t size, size_t nitems, void *userdata) {
    State *obj = static_cast<State*>(userdata);
    if (obj->GetStatusCode() < 0) {return 0;}  // malformed request - got body before headers.
    if (obj->GetStatusCode() >= 400) {return 0;}  // Status indicates failure.
    return obj->Read(static_cast<char*>(buffer), size*nitems);
}

int State::Read(char *buffer, size_t size) {
    int retval = m_fh->read(m_offset, buffer, size);
    if (retval == SFS_ERROR) {
        return -1;
    }
    m_offset += retval;
    //printf("Read a total of %ld bytes.\n", m_offset);
    return retval;
}
