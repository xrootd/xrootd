/******************************************************************************/
/* Copyright (C) 2026, Pelican Project, Morgridge Institute for Research      */
/*                                                                            */
/* This file is part of the XrdClHttp client plugin for XRootD.               */
/*                                                                            */
/* XRootD is free software: you can redistribute it and/or modify it under    */
/* the terms of the GNU Lesser General Public License as published by the     */
/* Free Software Foundation, either version 3 of the License, or (at your     */
/* option) any later version.                                                 */
/*                                                                            */
/* XRootD is distributed in the hope that it will be useful, but WITHOUT      */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or      */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public       */
/* License for more details.                                                  */
/*                                                                            */
/* The copyright holder's institutional names and contributor's names may not */
/* be used to endorse or promote products derived from this software without  */
/* specific prior written permission of the institution or contributor.       */
/******************************************************************************/

// Regression test for the bug where prefetch operations lingered in a paused
// libcurl state after File::Close(), accumulating worker slots until they
// expired via the transfer-stall timeout (default 60s).  Under repeated
// open/read/close traffic this exhausted the worker thread's max-ops budget
// and made the file system unresponsive — subsequent Open() calls would block
// in HandlerQueue::Produce on the producer CV until a slot freed up.

#include "XrdClHttp/XrdClHttpFile.hh"
#include "../XrdClHttpCommon/TransferTest.hh"

#include <XrdCl/XrdClDefaultEnv.hh>
#include <XrdCl/XrdClFile.hh>
#include <XrdCl/XrdClLog.hh>
#include <XrdOuc/XrdOucJson.hh>

#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>

class CloseDuringPrefetchFixture : public TransferFixture {};

namespace {

struct PrefetchCounters {
    uint64_t count{0};
    uint64_t cancelled{0};
    uint64_t expired{0};
    uint64_t failed{0};
};

// Read the plugin's per-process prefetch counters via the
// "XrdClHttpMonitoringJson" pseudo-property exposed by File::GetProperty.  We
// route through the property bag (rather than calling File::GetMonitoringJson
// directly) because XrdCl loads libXrdClHttp as a private plugin
// (RTLD_LOCAL) and the test binary links a separate copy of the library —
// only the property path crosses the plugin boundary into the right counters.
PrefetchCounters ReadPrefetchCounters(XrdCl::File &fh) {
    std::string js_str;
    EXPECT_TRUE(fh.GetProperty("XrdClHttpMonitoringJson", js_str));
    auto js = nlohmann::json::parse(js_str);
    const auto &pf = js.at("prefetch");
    PrefetchCounters c;
    c.count = pf.at("count").get<uint64_t>();
    c.cancelled = pf.at("cancelled").get<uint64_t>();
    c.expired = pf.at("expired").get<uint64_t>();
    c.failed = pf.at("failed").get<uint64_t>();
    return c;
}

} // namespace

// Open a file, read enough chunks to trigger prefetch, then Close() before
// the prefetch has finished.  Repeat many times.
//
// We use a body well over a megabyte so the prefetch op cannot complete in
// the gap between the second Read() returning and Close() running, even on
// loopback — this guarantees the op is still in libcurl (running or paused)
// at the moment Close fires.
TEST_F(CloseDuringPrefetchFixture, RapidCloseCancelsPrefetch)
{
    constexpr size_t chunk_size = 64 * 1024;       // 64 KB per Read
    constexpr off_t file_size = 64 * 1024 * 1024;  // 64 MB body

    // The worker's per-thread budget is 20 in-flight ops (XrdClHttpWorker.hh
    // m_max_ops); we deliberately exceed it so that the bug — paused ops
    // pinning slots until the 60s stall timeout — would surface as a hang.
    constexpr int kIterations = 40;
    constexpr unsigned char starting_char = 'a';

    auto url_base = GetOriginURL() + "/test/close_during_prefetch";
    ASSERT_NO_FATAL_FAILURE(WritePattern(url_base, file_size, starting_char, chunk_size));

    auto url = url_base + "?authz=" + GetReadToken();

    // Read the baseline once via a throwaway file — this also forces the
    // plugin to load now so the counters are zero-relative to this point.
    PrefetchCounters before;
    {
        XrdCl::File fh;
        auto rv = fh.Open(url, XrdCl::OpenFlags::Read, XrdCl::Access::Mode(0755),
                          static_cast<uint16_t>(10));
        ASSERT_TRUE(rv.IsOK()) << "baseline Open failed: " << rv.ToString();
        before = ReadPrefetchCounters(fh);
        ASSERT_TRUE(fh.Close().IsOK());
    }

    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        XrdCl::File fh;
        auto rv = fh.Open(url, XrdCl::OpenFlags::Read, XrdCl::Access::Mode(0755),
                          static_cast<uint16_t>(10));
        ASSERT_TRUE(rv.IsOK()) << "Iteration " << i << " Open failed: " << rv.ToString();

        // Two synchronous reads engage the prefetch mechanism (the second
        // read appends a new handler to the in-flight op).  After the
        // second read returns, the prefetch op is left running in the
        // background with hundreds of times the just-consumed bytes still
        // to transfer.
        std::string buf1(chunk_size, '\0');
        std::string buf2(chunk_size, '\0');
        uint32_t got = 0;
        rv = fh.Read(0, chunk_size, buf1.data(), got);
        ASSERT_TRUE(rv.IsOK()) << "Iteration " << i << " Read#1 failed";
        rv = fh.Read(chunk_size, chunk_size, buf2.data(), got);
        ASSERT_TRUE(rv.IsOK()) << "Iteration " << i << " Read#2 failed";

        // Close immediately while the prefetch op is still outstanding.
        // This is the line that, pre-fix, would silently leave a paused op
        // pinned to a worker slot for up to 60s.
        rv = fh.Close();
        ASSERT_TRUE(rv.IsOK()) << "Iteration " << i << " Close failed: " << rv.ToString();
    }
    auto elapsed = std::chrono::steady_clock::now() - t0;
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

    // Re-read the counters via a fresh open; we cannot reuse the loop's
    // `fh` because it's been closed.
    PrefetchCounters after;
    {
        XrdCl::File fh;
        auto rv = fh.Open(url, XrdCl::OpenFlags::Read, XrdCl::Access::Mode(0755),
                          static_cast<uint16_t>(10));
        ASSERT_TRUE(rv.IsOK());
        after = ReadPrefetchCounters(fh);
        ASSERT_TRUE(fh.Close().IsOK());
    }
    auto count_delta = after.count - before.count;
    auto cancelled_delta = after.cancelled - before.cancelled;
    auto expired_delta = after.expired - before.expired;
    auto failed_delta = after.failed - before.failed;
    std::cerr << "Prefetch counters delta: count=" << count_delta
              << " cancelled=" << cancelled_delta
              << " expired=" << expired_delta
              << " failed=" << failed_delta
              << " elapsed_ms=" << elapsed_ms
              << std::endl;

    // Confirm prefetch fired in roughly every iteration.  If it didn't, the
    // body of the test isn't exercising the cancel path — perhaps the file
    // size threshold changed or the read pattern stopped triggering prefetch.
    EXPECT_GE(count_delta, static_cast<uint64_t>(kIterations) / 2)
        << "Expected at least " << (kIterations / 2)
        << " prefetch ops to start; saw " << count_delta;

    // The prefetch-cancel counter should have advanced by roughly the
    // iteration count.  Not every iteration is guaranteed to leave the op
    // outstanding (a very fast origin can finish before Close runs), so we
    // require a majority rather than all.
    EXPECT_GE(cancelled_delta, static_cast<uint64_t>(kIterations) / 2)
        << "Expected at least " << (kIterations / 2)
        << " prefetch cancellations; saw " << cancelled_delta;

    // The stall-expiry path is the *old* behavior we replaced.  If it
    // fires, it means Close failed to deliver the cancellation and the op
    // sat until libcurl's 60s transfer-stall timer expired.
    EXPECT_LT(expired_delta, cancelled_delta)
        << "Stall-expiry count (" << expired_delta
        << ") exceeded cancellation count (" << cancelled_delta
        << "); the close-cancel path is not winning the race.";

    // Wall-clock budget.  With the fix, this completes in a few seconds on
    // localhost.  Without the fix, the worker queue fills after ~20
    // iterations and every subsequent Open blocks for ~60s waiting for a
    // paused op to stall-expire.
    EXPECT_LT(elapsed_ms, 30'000)
        << "Rapid open/read/close took " << elapsed_ms
        << " ms; expected <30s.";
}

// Exercise the race where a PrefetchResponseHandler's worker-thread callback fires
// AFTER File::~File has run.  Without the fix, the handler's `File &m_parent`
// is dangling and the dereference inside HandleResponse / ResubmitOperation is
// UAF.  With the fix, PrefetchResponseHandler holds a shared_ptr to the
// lifetime-safe PrefetchDefaultHandler and gates all File accesses on an
// atomic-snapshot-under-lock of m_file (cleared by File::~File via DetachFile).
//
// Strategy: do a couple of sync Reads first so the prefetch op enters its
// paused-with-no-pending-handler state, THEN drop the File.  In that state
// CancelPrefetch posts a CancelResponseHandler via the continue queue, so the
// PrefetchResponseHandler chain (which is empty by now) is not the failure
// point — instead, what we're stressing is the destructor ordering: ~File runs
// DetachFile, member dtors run, the worker dispatches the cancellation, and
// any callback that races into PrefetchResponseHandler::HandleResponse must
// see m_file=nullptr and skip File memory cleanly.
TEST_F(CloseDuringPrefetchFixture, ParentDetachIsSafe)
{
    constexpr size_t chunk_size = 64 * 1024;
    constexpr off_t file_size = 16 * 1024 * 1024;
    constexpr unsigned char starting_char = 'b';
    constexpr int kIterations = 30;

    auto url_base = GetOriginURL() + "/test/close_during_prefetch_detach";
    ASSERT_NO_FATAL_FAILURE(WritePattern(url_base, file_size, starting_char, chunk_size));
    auto url = url_base + "?authz=" + GetReadToken();

    for (int i = 0; i < kIterations; ++i) {
        auto fh = std::make_unique<XrdCl::File>();
        auto rv = fh->Open(url, XrdCl::OpenFlags::Read, XrdCl::Access::Mode(0755),
                           static_cast<uint16_t>(10));
        ASSERT_TRUE(rv.IsOK()) << "iter " << i << " open failed: " << rv.ToString();

        // Two sync Reads put the prefetch op into the paused state with no
        // pending handler.  This is the dominant cancel scenario in the field.
        std::string buf1(chunk_size, '\0');
        std::string buf2(chunk_size, '\0');
        uint32_t got = 0;
        rv = fh->Read(0, chunk_size, buf1.data(), got);
        ASSERT_TRUE(rv.IsOK()) << "iter " << i << " read#1 failed";
        rv = fh->Read(chunk_size, chunk_size, buf2.data(), got);
        ASSERT_TRUE(rv.IsOK()) << "iter " << i << " read#2 failed";

        // Drop File while the prefetch op is still paused in the worker.
        // ~File runs CancelPrefetch (cancels + queues wakeup) then DetachFile
        // (atomically clears the File* on PrefetchDefaultHandler), then
        // members destruct.  Any callback racing in afterwards must see
        // m_file=nullptr under the lock and skip File-owned memory.
        fh.reset();
    }

    // If a callback dereferenced freed File memory we'd typically see a crash
    // (ASan SEGV / pure-virtual) before reaching here; passing this loop
    // is the regression check.
    SUCCEED() << kIterations << " parent-detach iterations completed without UAF";
}
