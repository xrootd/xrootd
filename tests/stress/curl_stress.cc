
#include <curl/curl.h>
#include <gtest/gtest.h>

#include <fstream>
#include <string>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

namespace {

std::string g_http_url;
std::string g_hello_world = "Hello, world!\n";

} // namespace

class HttpStressTest : public testing::TestWithParam<std::tuple<int, std::string>> {
public:
  ~HttpStressTest() {}
  void SetUp() override {
    ParseEnv();
  
    auto [concurrency, prefix] = GetParam();

    m_multi_handle.reset(curl_multi_init());
    ASSERT_TRUE(m_multi_handle) << "Failed to allocate a curl multi-handle";

    m_offsets.resize(concurrency);
    CreateCurlUploadHandles(g_http_url + prefix + std::to_string(concurrency));
  }

  void TearDown() override {
    if (m_multi_handle) {
      for (auto &handle : m_handles) {
        curl_multi_remove_handle(m_multi_handle.get(), handle.get());
      }
    }
    m_multi_handle.reset();
    m_offsets.clear();
    m_handles.clear();
  }

protected:
  void Execute();

private:
  void ExecuteUploads();
  int CreateCurlUploadHandles(const std::string &url_prefix);
  void ParseEnv();

  std::unique_ptr<CURLM, decltype(curl_multi_cleanup)*> m_multi_handle{nullptr, &curl_multi_cleanup};
  std::vector<off_t> m_offsets;
  std::vector<std::unique_ptr<CURL, decltype(curl_easy_cleanup)*>> m_handles;
};

size_t hello_read_cb(char *buffer, size_t size, size_t nitems, void *offset_ptr) {
  off_t offset = *(off_t *)(offset_ptr);
  if (offset > 0) {
    return 0;
  }
  if (size*nitems < g_hello_world.size()) {
    return -1;
  }
  memcpy(buffer, g_hello_world.data(), g_hello_world.size());
  return g_hello_world.size();
}

size_t null_write_cb(char * /*buffer*/, size_t size, size_t nitems, void * /*data*/) {
  return size * nitems;
}

void HttpStressTest::Execute() {
    for (int idx=0; idx<20; idx++) {
      ASSERT_NO_FATAL_FAILURE(ExecuteUploads());
    }
}

int HttpStressTest::CreateCurlUploadHandles(const std::string &url_prefix) {
  for (size_t idx=0; idx<m_offsets.size(); idx++) {
    m_handles.emplace_back(curl_easy_init(), &curl_easy_cleanup);
    auto &handle = m_handles.back();
    auto url_final = url_prefix + "/test" + std::to_string(idx);
    curl_easy_setopt(handle.get(), CURLOPT_URL, url_final.c_str());
    curl_easy_setopt(handle.get(), CURLOPT_UPLOAD, 1);
    if (idx != 2) curl_easy_setopt(handle.get(), CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(handle.get(), CURLOPT_INFILESIZE_LARGE, (curl_off_t)g_hello_world.size());
    curl_easy_setopt(handle.get(), CURLOPT_READFUNCTION, hello_read_cb);
    curl_easy_setopt(handle.get(), CURLOPT_WRITEFUNCTION, null_write_cb);
    curl_easy_setopt(handle.get(), CURLOPT_FAILONERROR, 1);
    curl_easy_setopt(handle.get(), CURLOPT_TIMEOUT, 10L);
    m_offsets[idx] = 0;
    curl_easy_setopt(handle.get(), CURLOPT_READDATA, m_offsets.data() + idx);
  }
  return 0;
}

void HttpStressTest::ExecuteUploads() {
  size_t idx = 0;
  for (auto &handle : m_handles) {
    auto rc = curl_multi_add_handle(m_multi_handle.get(), handle.get());
    ASSERT_EQ(rc, CURLM_OK) << "Failed to add curl handle to multi-handle: " << curl_multi_strerror(rc);
    m_offsets[idx++] = 0;
  }

  int still_running;
  do {
    auto rc = curl_multi_perform(m_multi_handle.get(), &still_running);
    ASSERT_EQ(rc, CURLM_OK) << "Failed to perform curl multi-handle: " << curl_multi_strerror(rc);

    ASSERT_EQ(rc = curl_multi_wait(m_multi_handle.get(), NULL, 0, 1000, NULL), CURLM_OK) << "Unable to wait on curl multi-handle: " << curl_multi_strerror(rc);

    struct CURLMsg *msg;
    do {
      int msgq = 0;
      msg = curl_multi_info_read(m_multi_handle.get(), &msgq);
      if (msg && (msg->msg == CURLMSG_DONE)) {
        CURL *handle = msg->easy_handle;
        curl_multi_remove_handle(m_multi_handle.get(), handle);

        CURLcode res = msg->data.result;
        ASSERT_EQ(res, CURLE_OK) << "Failure when uploading: " << curl_easy_strerror(res);
      }
    } while (msg);
  } while (still_running);
}

void HttpStressTest::ParseEnv() {
  auto fname = getenv("TEST_CONFIG");
  ASSERT_NE(fname, nullptr) << "TEST_CONFIG environment variable is missing; was the test run invoked by ctest?";
	std::ifstream fh(fname);
  ASSERT_TRUE(fh.is_open()) << "Failed to open env file " << fname << ": " << strerror(errno);
	std::string line;
	while (std::getline(fh, line)) {
		auto idx = line.find("=");
		if (idx == std::string::npos) {
			continue;
		}
		auto key = line.substr(0, idx);
		auto val = line.substr(idx + 1);
		if (key != "HOST") {
      continue;
    }
    if (val.substr(0, 7) == "root://") {
      g_http_url = "http://" + val.substr(7);
    } else {
      g_http_url = val;
    }
	}

  ASSERT_FALSE(g_http_url.empty());
}

TEST_P(HttpStressTest, Upload) {
  Execute();
}

// INSTANTIATE_TEST_CASE_P was renamed to INSTANTIATE_TEST_SUITE_P after GTest 1.8.0.
// Currently, AlmaLinux 8 is the only platform that has a sufficiently old version
// of GTest that we need to use this ifdef to switch between the two.
#ifndef INSTANTIATE_TEST_SUITE_P
#define INSTANTIATE_TEST_SUITE_P INSTANTIATE_TEST_CASE_P
#endif

INSTANTIATE_TEST_SUITE_P(
  StressTests, HttpStressTest,
  testing::Combine(testing::Values(1, 10, 20), testing::Values("/stress_upload"))
);
