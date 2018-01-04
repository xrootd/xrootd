/**
 * state.hh:
 *
 * Helper class for managing the state of a single TPC request.
 */

#include <memory>

// Forward dec'ls
class XrdSfsFile;
class XrdHttpExtReq;
typedef void CURL;

namespace TPC {

class State {
public:
    State (std::unique_ptr<XrdSfsFile> fh, CURL *curl, bool push) :
        m_push(push),
        m_fh(std::move(fh)),
        m_curl(curl)
    {
        InstallHandlers(curl);
    }

    ~State();

    void CopyHeaders(XrdHttpExtReq &req);

    off_t BytesTransferred() const {return m_offset;}

    off_t GetContentLength() const {return m_content_length;}

    int GetStatusCode() const {return m_status_code;}

    void ResetAfterSize();

private:
    bool InstallHandlers(CURL *curl);

    // libcurl callback functions, along with the corresponding class methods.
    static size_t HeaderCB(char *buffer, size_t size, size_t nitems,
                           void *userdata);
    int Header(const std::string &header);
    static size_t WriteCB(void *buffer, size_t size, size_t nitems, void *userdata);
    int Write(char *buffer, size_t size);
    static size_t ReadCB(void *buffer, size_t size, size_t nitems, void *userdata);
    int Read(char *buffer, size_t size);

    bool m_push{true};  // whether we are transferring in "push-mode"
    bool m_recv_status_line{false};  // whether we have received a status line in the response from the remote host.
    bool m_recv_all_headers{false};  // true if we have seen the end of headers.
    off_t m_offset{0};  // number of bytes we have received.
    int m_status_code{-1};  // status code from HTTP response.
    off_t m_content_length{-1};  // value of Content-Length header, if we received one.
    std::unique_ptr<XrdSfsFile> m_fh;  // file-handle corresponding to this transfer.
    CURL *m_curl{nullptr};  // libcurl handle
    struct curl_slist *m_headers{nullptr}; // any headers we set as part of the libcurl request.
    std::string m_resp_protocol;  // Response protocol in the HTTP status line.
};

};
