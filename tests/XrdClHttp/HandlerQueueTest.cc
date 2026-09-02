/******************************************************************************/
/* Copyright (C) 2026 by European Organization for Nuclear Research (CERN)   */
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
/******************************************************************************/

#include "XrdClHttp/XrdClHttpOps.hh"

#include <gtest/gtest.h>

#include <chrono>
#include <memory>

namespace {

class QueueTestOperation final : public XrdClHttp::CurlOperation {
public:
    explicit QueueTestOperation(
        std::chrono::steady_clock::time_point expiry)
        : CurlOperation(nullptr, "https://queue.example/", expiry, nullptr,
                        nullptr, nullptr)
    {}

    void Success() override {}

    HttpVerb GetVerb() const override { return HttpVerb::GET; }
};

} // anonymous namespace

TEST(HandlerQueue, TryProduceRejectsWithoutWaiting)
{
    using namespace std::chrono_literals;

    XrdClHttp::HandlerQueue queue(1);
    const auto future = std::chrono::steady_clock::now() + 1min;
    auto first = std::make_shared<QueueTestOperation>(future);
    auto second = std::make_shared<QueueTestOperation>(future);

    ASSERT_TRUE(queue.TryProduce(first));
    EXPECT_FALSE(queue.TryProduce(second));

    EXPECT_EQ(queue.TryConsume(), first);
    EXPECT_TRUE(queue.TryProduce(second));
    EXPECT_EQ(queue.TryConsume(), second);

    auto expired = std::make_shared<QueueTestOperation>(
        std::chrono::steady_clock::now() - 1s);
    EXPECT_FALSE(queue.TryProduce(expired));

    queue.Shutdown();
    auto after_shutdown = std::make_shared<QueueTestOperation>(future);
    EXPECT_FALSE(queue.TryProduce(after_shutdown));
}
