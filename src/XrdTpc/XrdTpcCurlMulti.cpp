
#include "XrdTpcCurlMulti.hh"

#include <sys/select.h>

#ifndef HAS_CURL_MULTI
static
CURLMcode curl_multi_wait_impl(CURLM *multi_handle, int timeout_ms, int *numfds) {
    int max_fds;
    fd_set read_fd_set[FD_SETSIZE];
    fd_set write_fd_set[FD_SETSIZE];
    fd_set exc_fd_set[FD_SETSIZE];

    FD_ZERO(read_fd_set);
    FD_ZERO(write_fd_set);
    FD_ZERO(exc_fd_set);

    CURLMcode fdset_result = curl_multi_fdset(multi_handle, read_fd_set,
        write_fd_set, exc_fd_set, &max_fds);

    if (CURLM_OK != fdset_result) {
        return fdset_result;
    }

    struct timeval timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    int select_result = select(max_fds, read_fd_set, write_fd_set, exc_fd_set,
        &timeout);

    if (select_result >= 0) {
        *numfds = select_result;
        return CURLM_OK;
    }
    if (errno == EINTR) {
        return CURLM_OK;
    }
    if (errno == ENOMEM) {
        return CURLM_OUT_OF_MEMORY;
    }
    if (errno == EBADF) {
        return CURLM_BAD_SOCKET;
    }
    return CURLM_INTERNAL_ERROR;
}
#endif

