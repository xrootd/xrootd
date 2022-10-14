/**
 * state.hh:
 *
 * Helper class for managing the state of a single TPC request.
 */
#pragma once

#include <memory>
#include <vector>

// Forward dec'ls
class XrdSfsFile;
class XrdHttpExtReq;
typedef void CURL;
struct curl_slist;

namespace TPC {
class Stream;

class State {
public:

    State() :
        m_push(true),
        m_recv_status_line(false),
        m_recv_all_headers(false),
        m_offset(0),
        m_start_offset(0),
        m_status_code(-1),
        m_error_code(0),
        m_content_length(-1),
        m_stream(NULL),
        m_curl(NULL),
        m_headers(NULL),
        m_is_transfer_state(true)
    {}

    /**
     * Don't use that constructor if you want to do some transfers.
     * @param curl the curl handle
     */
    State(CURL * curl):
        m_push(true),
        m_recv_status_line(false),
        m_recv_all_headers(false),
        m_offset(0),
        m_start_offset(0),
        m_status_code(-1),
        m_error_code(0),
        m_content_length(-1),
        m_stream(NULL),
        m_curl(curl),
        m_headers(NULL),
        m_is_transfer_state(false)
    {
        InstallHandlers(curl);
    }

    // Note that we are "borrowing" a reference to the curl handle;
    // it is not owned / freed by the State object.  However, we use it
    // as if there's only one handle per State.
    State (off_t start_offset, Stream &stream, CURL *curl, bool push) :
        m_push(push),
        m_recv_status_line(false),
        m_recv_all_headers(false),
        m_offset(0),
        m_start_offset(start_offset),
        m_status_code(-1),
        m_error_code(0),
        m_content_length(-1),
        m_stream(&stream),
        m_curl(curl),
        m_headers(NULL),
        m_is_transfer_state(true)
    {
        InstallHandlers(curl);
    }

    ~State();

    void SetTransferParameters(off_t offset, size_t size);

    void CopyHeaders(XrdHttpExtReq &req);

    off_t BytesTransferred() const {return m_offset;}

    off_t GetContentLength() const {return m_content_length;}

    int GetErrorCode() const {return m_error_code;}

    void SetErrorCode(int error_code) {m_error_code = error_code;}

    int GetStatusCode() const {return m_status_code;}

    std::string GetErrorMessage() const {return m_error_buf;}

    void SetErrorMessage(const std::string &error_msg) {m_error_buf = error_msg;}

    void ResetAfterRequest();

    CURL *GetHandle() const {return m_curl;}

    int AvailableBuffers() const;

    void DumpBuffers() const;

    // Returns true if at least one byte of the response has been received,
    // but not the entire contents of the response.
    bool BodyTransferInProgress() const {return m_offset && (m_offset != m_content_length);}

    // Duplicate the current state; all settings are copied over, but those
    // related to the transient state are reset as if from a constructor.
    State *Duplicate();

    // Move the contents of a State object.  To be replaced by a move
    // constructor once C++11 is allowed in XRootD.
    void Move (State &other);

    // Flush and finalize a transfer state.  Eventually calls close() on the underlying
    // file handle, which should hopefully synchronize the file metadata across
    // all readers (even other load-balanced servers on the same distributed file
    // system).
    //
    // Returns true on success; false otherwise.  Failures can happen, for example, if
    // not all buffers have been reordered by the underlying stream.
    bool Finalize();

    // Flush the data in memory to disk, even if it may cause unaligned or short
    // writes.  Typically, only done while shutting down the transfer (note some
    // backends may be unable to handle unaligned writes unless it's the last write).
    int Flush();

    // Retrieve the description of the remote connection; is of the form:
    //   tcp:129.93.3.4:1234
    //   tcp:[2600:900:6:1301:268a:7ff:fef6:a590]:2345
    // This is meant to facilitate the monitoring via the performance markers.
    std::string GetConnectionDescription();

private:
    bool InstallHandlers(CURL *curl);

    State(const State&);
    // Add back once C++11 is available
    //State(State &&) noexcept;

    // libcurl callback functions, along with the corresponding class methods.
    static size_t HeaderCB(char *buffer, size_t size, size_t nitems,
                           void *userdata);
    int Header(const std::string &header);
    static size_t WriteCB(void *buffer, size_t size, size_t nitems, void *userdata);
    ssize_t Write(char *buffer, size_t size);
    static size_t ReadCB(void *buffer, size_t size, size_t nitems, void *userdata);
    int Read(char *buffer, size_t size);

    bool m_push;  // whether we are transferring in "push-mode"
    bool m_recv_status_line;  // whether we have received a status line in the response from the remote host.
    bool m_recv_all_headers;  // true if we have seen the end of headers.
    off_t m_offset;  // number of bytes we have received.
    off_t m_start_offset;  // offset where we started in the file.
    int m_status_code;  // status code from HTTP response.
    int m_error_code; // error code from underlying stream operations.
    off_t m_content_length;  // value of Content-Length header, if we received one.
    Stream *m_stream;  // stream corresponding to this transfer.
    CURL *m_curl;  // libcurl handle
    struct curl_slist *m_headers; // any headers we set as part of the libcurl request.
    std::vector<std::string> m_headers_copy; // Copies of custom headers.
    std::string m_resp_protocol;  // Response protocol in the HTTP status line.
    std::string m_error_buf;  // Any error associated with a response.
    bool m_is_transfer_state; // If set to true, this state will be used to perform some transfers
};

};
