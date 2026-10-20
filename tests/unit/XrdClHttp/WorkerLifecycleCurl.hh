// Test-only libcurl aliases.  The production sources are compiled unchanged;
// only this executable replaces the driver calls to exercise failure paths.
#ifndef XRDCLHTTP_WORKER_LIFECYCLE_CURL_HH
#define XRDCLHTTP_WORKER_LIFECYCLE_CURL_HH

#include <curl/curl.h>
#include <type_traits>

CURL *LifecycleEasyInit();
void LifecycleEasyCleanup(CURL *);
CURLM *LifecycleMultiInit();
CURLMcode LifecycleMultiAdd(CURLM *, CURL *);
CURLMcode LifecycleMultiRemove(CURLM *, CURL *);
CURLMcode LifecycleMultiPerform(CURLM *, int *);
CURLMsg *LifecycleMultiInfoRead(CURLM *, int *);
CURLMcode LifecycleMultiWait(CURLM *, curl_waitfd *, unsigned, int, int *);
CURLMcode LifecycleMultiCleanup(CURLM *);
void LifecycleHeaderData(CURL *, void *);
void LifecycleHeaderCallback(CURL *, curl_write_callback);
bool LifecycleEasyIsLive(CURL *);

template <typename T>
CURLcode LifecycleEasySetopt(CURL *curl, CURLoption option, T value)
{
    if (!LifecycleEasyIsLive(curl)) return CURLE_BAD_FUNCTION_ARGUMENT;
    if constexpr (std::is_convertible<T, void *>::value) {
        if (option == CURLOPT_HEADERDATA) LifecycleHeaderData(curl, value);
    }
    if constexpr (std::is_convertible<T, curl_write_callback>::value) {
        if (option == CURLOPT_HEADERFUNCTION) LifecycleHeaderCallback(curl, value);
    }
    return curl_easy_setopt(curl, option, value);
}

#define curl_easy_init LifecycleEasyInit
#define curl_easy_cleanup LifecycleEasyCleanup
#undef curl_easy_setopt
#define curl_easy_setopt LifecycleEasySetopt
#define curl_multi_init LifecycleMultiInit
#define curl_multi_add_handle LifecycleMultiAdd
#define curl_multi_remove_handle LifecycleMultiRemove
#define curl_multi_perform LifecycleMultiPerform
#define curl_multi_info_read LifecycleMultiInfoRead
#define curl_multi_wait LifecycleMultiWait
#define curl_multi_cleanup LifecycleMultiCleanup

#endif
