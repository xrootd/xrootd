/**
 * state.hh:
 *
 * Helper class for managing the state of a single TPC request.
 */

#include <memory>
#include <vector>

// Forward dec'ls
class XrdSfsFile;
class XrdHttpExtReq;
typedef void CURL;

namespace TPC {
class Stream;

class State {
public:

    // Note that we are "borrowing" a reference to the curl handle;
    // it is not owned / freed by the State object.  However, we use it
    // as if there's only one handle per State.
    State (off_t start_offset, Stream &stream, CURL *curl, bool push) :
        m_push(push),
        m_start_offset(start_offset),
        m_stream(stream),
        m_curl(curl)
    {
        InstallHandlers(curl);
    }

    ~State();

    void SetTransferParameters(off_t offset, size_t size);

    void CopyHeaders(XrdHttpExtReq &req);

    off_t BytesTransferred() const {return m_offset;}

    off_t GetContentLength() const {return m_content_length;}

    int GetStatusCode() const {return m_status_code;}

    void ResetAfterRequest();

    CURL *GetHandle() const {return m_curl;}

    int AvailableBuffers() const;

    // Returns true if at least one byte of the response has been received,
    // but not the entire contents of the response.
    bool BodyTransferInProgress() const {return m_offset && (m_offset != m_content_length);}

    // Duplicate the current state; all settings are copied over, but those
    // related to the transient state are reset as if from a constructor.
    State Duplicate();

    State(const State&) = delete;
    State(State &&) noexcept;

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
    off_t m_start_offset{0};  // offset where we started in the file.
    int m_status_code{-1};  // status code from HTTP response.
    off_t m_content_length{-1};  // value of Content-Length header, if we received one.
    Stream &m_stream;  // stream corresponding to this transfer.
    CURL *m_curl{nullptr};  // libcurl handle
    struct curl_slist *m_headers{nullptr}; // any headers we set as part of the libcurl request.
    std::vector<std::string> m_headers_copy; // Copies of custom headers.
    std::string m_resp_protocol;  // Response protocol in the HTTP status line.
};

};
