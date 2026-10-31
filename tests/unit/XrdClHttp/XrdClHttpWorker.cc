/******************************************************************************/
/* (c) 2026 by the XRootD Collaboration                                       */
/* This file is part of XRootD, distributed under the LGPL version 3 or later. */
/******************************************************************************/

#include "XrdClHttp/XrdClHttpWorker.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "WorkerLifecycleCurl.hh"

// This translation unit implements the aliases and must call real libcurl.
#undef curl_easy_init
#undef curl_easy_cleanup
#undef curl_easy_setopt
#undef curl_multi_init
#undef curl_multi_add_handle
#undef curl_multi_remove_handle
#undef curl_multi_perform
#undef curl_multi_info_read
#undef curl_multi_wait
#undef curl_multi_cleanup

#include <gtest/gtest.h>
#include <unistd.h>

#include <deque>
#include <cerrno>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

using namespace XrdClHttp;

class TestOperation final : public XrdCl::ResponseHandler, public CurlOperation {
public:
    TestOperation(bool options, CreateConnCalloutType callout) :
        CurlOperation(this, "http://worker.invalid/file",
                      std::chrono::steady_clock::now() + std::chrono::minutes(1),
                      XrdCl::DefaultEnv::GetLog(), callout, nullptr),
        requires_options(options) {}

    void HandleResponse(XrdCl::XRootDStatus *status, XrdCl::AnyObject *response) override
    {
        statuses.push_back(*status);
        delete status;
        delete response;
    }

    bool Setup(CURL *curl, CurlWorker &worker) override
    {
        if (throw_before_adoption) throw std::runtime_error("setup before adoption");
        if (false_before_adoption) return false;
        if (!CurlOperation::Setup(curl, worker)) return false;
        return !false_after_adoption;
    }

    HttpVerb GetVerb() const override {return HttpVerb::GET;}
    bool RequiresOptions() const override {return requires_options;}
    void OptionsDone() override {++options_done;}

    RedirectAction Redirect(std::string &target) override
    {
        ++redirects;
        target = "http://redirect" + std::to_string(redirects) + ".invalid/file";
        if (redirects <= options_redirects) return RedirectAction::ReinvokeAfterAllow;
        m_headers = HeaderParser();
        return RedirectAction::Reinvoke;
    }

    void Fail(uint16_t code, uint32_t number, const std::string &message) override
    {
        ++failures;
        CurlOperation::Fail(code, number, message);
    }

    void Success() override
    {
        ++successes;
        SetDone(false);
        auto handler = m_handler;
        m_handler = nullptr;
        if (handler) handler->HandleResponse(new XrdCl::XRootDStatus(), nullptr);
    }

    bool requires_options;
    bool throw_before_adoption{false};
    bool false_before_adoption{false};
    bool false_after_adoption{false};
    unsigned options_redirects{1};
    unsigned options_done{0};
    unsigned redirects{0};
    unsigned failures{0};
    unsigned successes{0};
    std::vector<XrdCl::XRootDStatus> statuses;
};

struct HandleState {
    bool live{true};
    curl_write_callback callback{nullptr};
    void *data{nullptr};
};

struct Harness;
thread_local Harness *current{nullptr};

struct Harness {
    Harness() : queue(std::make_shared<HandlerQueue>(50))
    {
        EXPECT_EQ(current, nullptr);
        worker = std::make_unique<CurlWorker>(queue, VerbsCache::Instance(),
                                               XrdCl::DefaultEnv::GetLog());
    }

    ~Harness()
    {
        current = this;
        CleanupHandles();
        current = nullptr;
        if (shutdown_read_fd != -1) close(shutdown_read_fd);
    }

    // Keep failed assertions safe to inspect if a regression leaves handles
    // attached to the multi-handle or retained by an operation.
    void CleanupHandles()
    {
        for (auto &op : operations) op->ReleaseHandle();
        for (auto &entry : handles) {
            if (!entry.second.live) continue;
            // Active callbacks still belong to operations retained by the
            // worker.  Recycled handles may contain stale callback data.
            if (active.count(entry.first) && entry.second.data)
                static_cast<CurlOperation *>(entry.second.data)->ReleaseHandle();
            if (multi) LifecycleMultiRemove(multi, entry.first);
            LifecycleEasyCleanup(entry.first);
        }
        if (multi) LifecycleMultiCleanup(multi);
    }

    std::shared_ptr<TestOperation> Add(bool options = false,
                                     CreateConnCalloutType callout = nullptr)
    {
        auto op = std::make_shared<TestOperation>(options, callout);
        operations.push_back(op);
        queue->Produce(op);
        return op;
    }

    CURL *Find(CurlOperation *parent, bool options = false)
    {
        for (auto handle : active) {
            auto op = static_cast<CurlOperation *>(handles.at(handle).data);
            auto probe = dynamic_cast<CurlOptionsOp *>(op);
            if (options ? probe && probe->GetOperation().get() == parent : op == parent)
                return handle;
        }
        ADD_FAILURE() << "No active " << (options ? "OPTIONS probe" : "operation");
        return nullptr;
    }

    void CompleteOptions(CurlOperation *parent, CURLcode result = CURLE_OK,
                         unsigned status = 200)
    {
        auto handle = Find(parent, true);
        ASSERT_NE(handle, nullptr);
        auto options = static_cast<CurlOptionsOp *>(handles.at(handle).data);
        EXPECT_EQ(options->GetParentCurlHandle(), parent->GetCurlHandle());
        Complete(handle, result, status);
    }

    void Complete(CURL *handle, CURLcode result = CURLE_OK, unsigned status = 200)
    {
        if (!handle) return;
        auto &state = handles.at(handle);
        ASSERT_NE(state.callback, nullptr);
        ASSERT_NE(state.data, nullptr);
        if (result == CURLE_OK) {
            std::vector<std::string> headers{
                "HTTP/1.1 " + std::to_string(status) + " Test\r\n",
                "Allow: GET, HEAD, PROPFIND\r\n", "Content-Length: 0\r\n", "\r\n"};
            for (auto &header : headers) {
                EXPECT_EQ(state.callback(&header[0], 1, header.size(), state.data), header.size());
            }
        }
        CURLMsg message{};
        message.msg = CURLMSG_DONE;
        message.easy_handle = handle;
        message.data.result = result;
        completions.push_back(message);
    }

    void Run()
    {
        auto worker_ptr = worker.get();
        std::packaged_task<void()> task([this, worker_ptr] {
            current = this;
            worker_ptr->Run();
        });
        auto completion = task.get_future();
        // Use the same public ownership path as production.  Static worker
        // shutdown later joins this finished thread and unregisters its stats.
        worker_ptr->Start(std::move(worker), std::thread(std::move(task)));
        EXPECT_NO_THROW(completion.get());
        EXPECT_EQ(active.size(), 0u);
        EXPECT_EQ(cleaned, created);
        EXPECT_EQ(cleanup_order_errors, 0u);
        EXPECT_EQ(invalid_handle_uses, 0u);
        EXPECT_EQ(multi_cleanups, 1u);
        for (size_t i = 0; i < operations.size(); ++i) {
            SCOPED_TRACE("operation " + std::to_string(i));
            EXPECT_EQ(operations[i]->GetCurlHandle(), nullptr);
            EXPECT_TRUE(operations[i]->IsDone());
            EXPECT_EQ(operations[i]->statuses.size(), 1u);
            EXPECT_EQ(operations[i]->failures + operations[i]->successes, 1u);
        }
    }

    std::shared_ptr<HandlerQueue> queue;
    std::unique_ptr<CurlWorker> worker;
    std::vector<std::shared_ptr<TestOperation>> operations;
    std::unordered_map<CURL *, HandleState> handles;
    std::unordered_set<CURL *> active;
    std::deque<CURLMsg> completions;
    CURLMsg returned_message{};
    CURLM *multi{nullptr};
    std::vector<std::function<void()>> steps;
    size_t step{0};
    unsigned created{0};
    unsigned cleaned{0};
    unsigned adds{0};
    unsigned fail_add{0};
    unsigned fail_allocation{0};
    unsigned allocations{0};
    unsigned callouts{0};
    unsigned throw_callout{0};
    unsigned multi_cleanups{0};
    unsigned cleanup_order_errors{0};
    unsigned invalid_handle_uses{0};
    bool normal_shutdown{false};
    int shutdown_read_fd{-1};
};

ConnectionCallout *SetupCallout(const std::string &, const ResponseInfo &)
{
    if (++current->callouts == current->throw_callout)
        throw std::runtime_error("OPTIONS setup failure");
    return nullptr;
}

class FailedConnectionCallout final : public ConnectionCallout {
public:
    int BeginCallout(std::string &error,
                     std::chrono::steady_clock::time_point &) override
    {
        error = "Connection callout could not start";
        return -1;
    }
    int FinishCallout(std::string &) override {return -1;}
};

ConnectionCallout *FailedCallout(const std::string &, const ResponseInfo &)
{
    return new FailedConnectionCallout;
}

}

// Real easy/multi handles are used to check libcurl's ownership rules.  Only
// network progress and selected failure results are simulated.  No request is
// sent, and no production fault-injection interface is required.
CURL *LifecycleEasyInit()
{
    if (current) {
        // All operations are already queued; avoid one-second idle Consume()
        // waits once a setup failure leaves this worker with no active handles.
        current->queue->Shutdown();
        if (++current->allocations == current->fail_allocation) return nullptr;
    }
    auto handle = curl_easy_init();
    if (current && handle) {
        current->handles[handle] = HandleState{};
        ++current->created;
    }
    return handle;
}

bool LifecycleEasyIsLive(CURL *handle)
{
    if (!current) return true;
    auto iter = current->handles.find(handle);
    if (iter != current->handles.end() && iter->second.live) return true;
    ++current->invalid_handle_uses;
    return false;
}

void LifecycleEasyCleanup(CURL *handle)
{
    if (current && handle) {
        if (!LifecycleEasyIsLive(handle)) return;
        if (current->active.erase(handle)) {
            ++current->cleanup_order_errors;
            if (current->multi) curl_multi_remove_handle(current->multi, handle);
        }
        current->handles.at(handle).live = false;
        ++current->cleaned;
    }
    curl_easy_cleanup(handle);
}

void LifecycleHeaderData(CURL *handle, void *data)
{
    if (current) current->handles.at(handle).data = data;
}

void LifecycleHeaderCallback(CURL *handle, curl_write_callback callback)
{
    if (current) current->handles.at(handle).callback = callback;
}

CURLM *LifecycleMultiInit()
{
    auto multi = curl_multi_init();
    if (current) current->multi = multi;
    return multi;
}

CURLMcode LifecycleMultiAdd(CURLM *multi, CURL *handle)
{
    if (current && ++current->adds == current->fail_add) return CURLM_OUT_OF_MEMORY;
    auto result = curl_multi_add_handle(multi, handle);
    if (current && result == CURLM_OK) current->active.insert(handle);
    return result;
}

CURLMcode LifecycleMultiRemove(CURLM *multi, CURL *handle)
{
    if (current) current->active.erase(handle);
    return curl_multi_remove_handle(multi, handle);
}

CURLMcode LifecycleMultiPerform(CURLM *, int *running)
{
    *running = static_cast<int>(current->active.size());
    if (current->step == current->steps.size())
        return current->normal_shutdown ? CURLM_OK : CURLM_INTERNAL_ERROR;
    current->steps[current->step++]();
    return CURLM_OK;
}

CURLMsg *LifecycleMultiInfoRead(CURLM *, int *remaining)
{
    if (current->completions.empty()) {
        *remaining = 0;
        return nullptr;
    }
    current->returned_message = current->completions.front();
    current->completions.pop_front();
    *remaining = static_cast<int>(current->completions.size());
    return &current->returned_message;
}

CURLMcode LifecycleMultiWait(CURLM *, curl_waitfd *fds, unsigned count, int, int *ready)
{
    if (ready) *ready = 0;
    if (count >= 3) current->shutdown_read_fd = fds[2].fd;
    if (current->normal_shutdown && count >= 3) fds[2].revents = CURL_WAIT_POLLIN;
    return CURLM_OK;
}

CURLMcode LifecycleMultiCleanup(CURLM *multi)
{
    if (current) {
        if (!current->active.empty() || current->created != current->cleaned)
            ++current->cleanup_order_errors;
        ++current->multi_cleanups;
        current->multi = nullptr;
    }
    return curl_multi_cleanup(multi);
}

TEST(WorkerLifecycle, CleansHandleWhenSetupThrowsBeforeAdoption)
{
    Harness test;
    auto op = test.Add();
    op->throw_before_adoption = true;
    test.Run();
    EXPECT_EQ(op->failures, 1u);
}

TEST(WorkerLifecycle, CleansHandleWhenSetupReturnsFalseBeforeAdoption)
{
    Harness test;
    auto op = test.Add();
    op->false_before_adoption = true;
    test.Run();
    EXPECT_EQ(op->failures, 1u);
}

TEST(WorkerLifecycle, SetupExceptionAfterAdoptionFailsOperation)
{
    Harness test;
    auto op = test.Add(false, SetupCallout);
    test.throw_callout = 1;
    test.Run();
    EXPECT_EQ(test.callouts, 1u);
    EXPECT_EQ(op->failures, 1u);
}

TEST(WorkerLifecycle, SetupFalseAfterAdoptionFailsOperation)
{
    Harness test;
    auto op = test.Add();
    op->false_after_adoption = true;
    test.Run();
    EXPECT_EQ(op->failures, 1u);
}

struct OptionsSetupFailure {
    const char *name;
    bool redirect;
    unsigned allocation;
    unsigned add;
    unsigned callout;
};

class OptionsSetup : public testing::TestWithParam<OptionsSetupFailure> {};

TEST_P(OptionsSetup, FailsParentOnce)
{
    const auto &failure = GetParam();
    Harness test;
    auto op = test.Add(!failure.redirect, SetupCallout);
    test.fail_allocation = failure.allocation;
    test.fail_add = failure.add;
    test.throw_callout = failure.callout;
    if (failure.redirect)
        test.steps.push_back([&]{test.Complete(test.Find(op.get()), CURLE_OK, 302);});
    test.Run();
    EXPECT_EQ(op->redirects, failure.redirect ? 1u : 0u);
    EXPECT_EQ(op->failures, 1u);
    if (failure.callout) {
        EXPECT_EQ(test.callouts, failure.callout);
    }
}

INSTANTIATE_TEST_SUITE_P(WorkerLifecycle, OptionsSetup, testing::Values(
    OptionsSetupFailure{"InitialAllocation", false, 2, 0, 0},
    OptionsSetupFailure{"InitialAdd", false, 0, 1, 0},
    OptionsSetupFailure{"InitialException", false, 0, 0, 2},
    OptionsSetupFailure{"RedirectAllocation", true, 2, 0, 0},
    OptionsSetupFailure{"RedirectAdd", true, 0, 2, 0},
    OptionsSetupFailure{"RedirectException", true, 0, 0, 2}),
    [](const testing::TestParamInfo<OptionsSetupFailure> &info) {return info.param.name;});

struct OptionsResult {
    const char *name;
    CURLcode result;
    unsigned status;
};

class OptionsCompletion : public testing::TestWithParam<OptionsResult> {};

TEST_P(OptionsCompletion, ChecksParentReactivation)
{
    const auto &completion = GetParam();
    Harness test;
    auto op = test.Add(true);
    test.fail_add = 2;
    test.steps.push_back([&]{
        test.CompleteOptions(op.get(), completion.result, completion.status);
    });
    test.Run();
    EXPECT_EQ(op->failures, 1u);
    ASSERT_EQ(op->statuses.size(), 1u);
    EXPECT_EQ(op->statuses.front().errNo, CURLM_OUT_OF_MEMORY);
}

TEST_P(OptionsCompletion, ResumesRedirectedParent)
{
    const auto &completion = GetParam();
    Harness test;
    auto op = test.Add();
    test.steps.push_back([&]{
        test.Complete(test.Find(op.get()), CURLE_OK, 302);
    });
    test.steps.push_back([&]{
        test.CompleteOptions(op.get(), completion.result, completion.status);
    });
    test.steps.push_back([&]{
        test.Complete(test.Find(op.get()), CURLE_OK, 200);
    });
    test.Run();
    EXPECT_EQ(op->successes, 1u);
    EXPECT_EQ(op->failures, 0u);
    EXPECT_EQ(op->redirects, 2u);
    EXPECT_EQ(op->options_done,
              completion.result == CURLE_OK && completion.status == 200 ? 1u : 0u);
}

INSTANTIATE_TEST_SUITE_P(WorkerLifecycle, OptionsCompletion, testing::Values(
    OptionsResult{"Success", CURLE_OK, 200},
    OptionsResult{"HttpFailure", CURLE_OK, 405},
    OptionsResult{"TransportFailure", CURLE_COULDNT_CONNECT, 200}),
    [](const testing::TestParamInfo<OptionsResult> &info) {return info.param.name;});

class ConcurrentOptions : public testing::TestWithParam<unsigned> {};

TEST_P(ConcurrentOptions, PreservesOriginalParentHandles)
{
    Harness test;
    std::vector<std::shared_ptr<TestOperation>> parents;
    // Twenty live parents expand to forty entries during OPTIONS probing,
    // exercising map growth without exposing the worker's private map.
    for (unsigned i = 0; i < 20; ++i) {
        auto parent = test.Add();
        parent->options_redirects = GetParam();
        parents.push_back(parent);
    }
    test.steps.push_back([&]{
        for (auto &parent : parents) test.Complete(test.Find(parent.get()), CURLE_OK, 302);
    });
    for (unsigned i = 0; i < GetParam(); ++i)
        test.steps.push_back([&]{
            for (auto &parent : parents) test.CompleteOptions(parent.get());
        });
    test.steps.push_back([&]{
        for (auto &parent : parents) test.Complete(test.Find(parent.get()));
    });
    test.Run();
    for (auto &parent : parents) {
        EXPECT_EQ(parent->redirects, GetParam() + 1);
        EXPECT_EQ(parent->options_done, GetParam());
        EXPECT_EQ(parent->successes, 1u);
    }
}

INSTANTIATE_TEST_SUITE_P(WorkerLifecycle, ConcurrentOptions, testing::Values(1u, 2u),
    [](const testing::TestParamInfo<unsigned> &info) {
        return info.param == 1 ? "SingleProbe" : "ChainedProbes";
    });

TEST(WorkerLifecycle, WorkerExitReleasesExternallyRetainedActiveOperation)
{
    Harness test;
    auto op = test.Add();
    test.Run();
    EXPECT_EQ(op->failures, 1u);
    ASSERT_EQ(op->statuses.size(), 1u);
    EXPECT_EQ(op->statuses.front().errNo, CURLM_INTERNAL_ERROR);
}

TEST(WorkerLifecycle, WorkerExitReleasesExternallyRetainedParentAndOptions)
{
    Harness test;
    auto op = test.Add(true);
    test.Run();
    EXPECT_EQ(test.created, 2u);
    EXPECT_EQ(op->failures, 1u);
}

TEST(WorkerLifecycle, ShutdownCancelsExternallyRetainedActiveOperations)
{
    Harness test;
    test.normal_shutdown = true;
    test.Add();
    test.Add(true);
    test.Run();
    for (auto &op : test.operations) {
        ASSERT_EQ(op->statuses.size(), 1u);
        EXPECT_EQ(op->statuses.front().errNo, ECANCELED);
    }
}

TEST(WorkerLifecycle, ConnectionCalloutStartFailureFailsOnlyOnce)
{
    Harness test;
    auto op = test.Add(false, FailedCallout);
    test.steps.push_back([&]{test.Complete(test.Find(op.get()), CURLE_COULDNT_CONNECT);});
    test.Run();
    EXPECT_EQ(op->failures, 1u);
    EXPECT_EQ(op->successes, 0u);
}
