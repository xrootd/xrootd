
#include "XrdTpcCurlMulti.hh"

#include <errno.h>
#include <sys/select.h>

#ifndef HAVE_CURL_MULTI_WAIT
CURLMcode curl_multi_wait_impl(CURLM *multi_handle, int timeout_ms, int *numfds) {
    int max_fds = FD_SETSIZE;
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
    if (max_fds == -1) {
        // Per the curl documentation, this case "is because libcurl currently
        // does something that isn't possible for your application to monitor
        // with a socket and unfortunately you can then not know exactly when
        // the current action is completed using select()."
        //
        // We use their recommendation to sleep for 100ms.
        max_fds = 0;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100*1000;
    } else {
        max_fds ++;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
    }
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

