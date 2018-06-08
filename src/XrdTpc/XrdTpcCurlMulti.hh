
#include <curl/curl.h>

#ifndef HAVE_CURL_MULTI_WAIT
CURLMcode curl_multi_wait_impl(CURLM *multi_handle, int timeout_ms, int *numfds);
#endif

