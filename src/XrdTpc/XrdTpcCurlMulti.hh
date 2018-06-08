
#include <curl/curl.h>

#if defined(curl_multi_wait)
#define HAS_CURL_MULTI
#endif

#ifndef HAS_CURL_MULTI
CURLMcode curl_multi_wait_impl(CURLM *multi_handle, int timeout_ms, int *numfds);
#endif

