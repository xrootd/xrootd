/******************************************************************************/
/* Copyright (C) 2026, European Organization for Nuclear Research (CERN)      */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
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

#include "XrdClHttp/XrdClHttpFactory.hh"
#include "Server.hh"
#include "Utils.hh"

#include <XrdCl/XrdClAnyObject.hh>
#include <XrdCl/XrdClUtils.hh>
#include <XrdCl/XrdClXRootDResponses.hh>

#include <gtest/gtest.h>

#include <cerrno>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <unistd.h>

namespace {

struct MockHttpExchange {
    std::string response;
    std::string request;
    bool read_failed{false};
    bool write_failed{false};
};

class FixedHttpResponseHandler final : public XrdClTests::ClientHandler {
public:
    explicit FixedHttpResponseHandler(MockHttpExchange *exchange)
        : m_exchange(exchange)
    {}

    void HandleConnection(int socket) override
    {
        XrdCl::ScopedDescriptor descriptor(socket);
        char buffer[4096];

        while (m_exchange->request.find("\r\n\r\n") == std::string::npos) {
            ssize_t bytes_read;
            do {
                bytes_read = read(socket, buffer, sizeof(buffer));
            } while (bytes_read < 0 && errno == EINTR);

            if (bytes_read <= 0 ||
                m_exchange->request.size() + bytes_read > 64 * 1024) {
                m_exchange->read_failed = true;
                return;
            }
            m_exchange->request.append(buffer, bytes_read);
        }

        if (XrdClTests::Utils::Write(socket, m_exchange->response.data(),
                                    m_exchange->response.size()) !=
            static_cast<ssize_t>(m_exchange->response.size())) {
            m_exchange->write_failed = true;
        }
    }

private:
    MockHttpExchange *m_exchange;
};

class FixedHttpResponseFactory final : public XrdClTests::ClientHandlerFactory {
public:
    explicit FixedHttpResponseFactory(MockHttpExchange *exchange)
        : m_exchange(exchange)
    {}

    XrdClTests::ClientHandler *CreateHandler() override
    {
        return new FixedHttpResponseHandler(m_exchange);
    }

private:
    MockHttpExchange *m_exchange;
};

class SyncResponseHandler final : public XrdCl::ResponseHandler {
public:
    void HandleResponse(XrdCl::XRootDStatus *status,
                        XrdCl::AnyObject *response) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_status.reset(status);
        m_response.reset(response);
        m_cv.notify_one();
    }

    void Wait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [this] { return m_status != nullptr; });
    }

    std::tuple<std::unique_ptr<XrdCl::XRootDStatus>,
               std::unique_ptr<XrdCl::AnyObject>> Status()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return {std::move(m_status), std::move(m_response)};
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::unique_ptr<XrdCl::XRootDStatus> m_status;
    std::unique_ptr<XrdCl::AnyObject> m_response;
};

TEST(CurlDeleteMultiStatus, NoContentSucceeds)
{
    MockHttpExchange exchange;
    exchange.response =
        "HTTP/1.1 204 No Content\r\n"
        "Connection: close\r\n"
        "\r\n";
    XrdClTests::Server server(XrdClTests::Server::Inet4);
    ASSERT_TRUE(server.Setup(
        0, 1, new FixedHttpResponseFactory(&exchange)));

    const std::string endpoint =
        "http://127.0.0.1:" + std::to_string(server.GetPort());
    auto factory = std::make_unique<XrdClHttp::Factory>();
    std::unique_ptr<XrdCl::FileSystemPlugIn> filesystem(
        factory->CreateFileSystem(endpoint));
    ASSERT_NE(filesystem, nullptr);

    SyncResponseHandler handler;
    auto submitted = filesystem->Rm("/tree", &handler, 10);
    ASSERT_TRUE(submitted.IsOK()) << submitted.ToString();
    ASSERT_TRUE(server.Start());

    handler.Wait();
    auto [status, response] = handler.Status();
    ASSERT_TRUE(server.Stop());

    ASSERT_NE(status, nullptr);
    EXPECT_TRUE(status->IsOK()) << status->ToStr();
    EXPECT_EQ(response, nullptr);
    EXPECT_FALSE(exchange.read_failed);
    EXPECT_FALSE(exchange.write_failed);

    auto line_end = exchange.request.find("\r\n");
    ASSERT_NE(line_end, std::string::npos);
    EXPECT_EQ(exchange.request.substr(0, line_end),
              "DELETE /tree HTTP/1.1");
}

TEST(CurlDeleteMultiStatus, MultiStatusFails)
{
    const std::string body =
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
        "<D:multistatus xmlns:D=\"DAV:\">\n"
        "  <D:response>\n"
        "    <D:href>/tree/locked</D:href>\n"
        "    <D:status>HTTP/1.1 423 Locked</D:status>\n"
        "  </D:response>\n"
        "</D:multistatus>\n";
    MockHttpExchange exchange;
    exchange.response =
        "HTTP/1.1 207 Multi-Status\r\n"
        "Content-Type: application/xml; charset=utf-8\r\n"
        "Content-Length: " + std::to_string(body.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n" + body;
    XrdClTests::Server server(XrdClTests::Server::Inet4);
    ASSERT_TRUE(server.Setup(
        0, 1, new FixedHttpResponseFactory(&exchange)));

    const std::string endpoint =
        "http://127.0.0.1:" + std::to_string(server.GetPort());
    auto factory = std::make_unique<XrdClHttp::Factory>();
    std::unique_ptr<XrdCl::FileSystemPlugIn> filesystem(
        factory->CreateFileSystem(endpoint));
    ASSERT_NE(filesystem, nullptr);

    SyncResponseHandler handler;
    auto submitted = filesystem->Rm("/tree", &handler, 10);
    ASSERT_TRUE(submitted.IsOK()) << submitted.ToString();
    ASSERT_TRUE(server.Start());

    handler.Wait();
    auto [status, response] = handler.Status();
    ASSERT_TRUE(server.Stop());

    ASSERT_NE(status, nullptr);
    EXPECT_FALSE(status->IsOK());
    EXPECT_EQ(status->code, XrdCl::errErrorResponse);
    EXPECT_EQ(status->errNo, kXR_ServerError);
    EXPECT_EQ(status->GetErrorMessage(),
              "DELETE returned HTTP 207 Multi-Status; one or more resources were not removed");
    EXPECT_EQ(response, nullptr);
    EXPECT_FALSE(exchange.read_failed);
    EXPECT_FALSE(exchange.write_failed);

    auto line_end = exchange.request.find("\r\n");
    ASSERT_NE(line_end, std::string::npos);
    EXPECT_EQ(exchange.request.substr(0, line_end),
              "DELETE /tree HTTP/1.1");
}

} // namespace
