//------------------------------------------------------------------------------
// Unit tests for XrdMonOtlpBatch: the conversion of the collector's nested-JSON
// documents into OTLP/JSON export requests (logs and traces). The HTTP transport
// (XrdMonOtlp) needs libcurl and is not exercised here.
//------------------------------------------------------------------------------

#include "XrdApps/XrdMonCollect/XrdMonOtlp.hh"
#include "XrdOuc/XrdOucJson.hh"

#include <gtest/gtest.h>

using json = nlohmann::json;

namespace
{
// Find an OTLP KeyValue by key in an attributes array; nullptr if absent.
const json* kv(const json& arr, const std::string& key)
{
   for (const auto& e : arr)
       if (e.value("key", std::string()) == key) return &e;
   return nullptr;
}
}

TEST(XrdMonOtlp, LogRecordEncoding)
{
   json doc;
   doc["resource"]["service.name"]              = "xrootd";
   doc["resource"]["server.address"]            = "srv1";
   doc["resource"]["xrootd.server.incarnation"] = 1700000000;
   doc["scope"]["name"]          = "xrdmoncollect";
   doc["@timestamp"]             = "2026-07-02T10:00:32Z";
   doc["timeUnixNano"]           = "1751450432000000000";
   doc["severityNumber"]         = 9;
   doc["severityText"]           = "INFO";
   doc["traceId"]                = "9f1c8b0d4e2a6f37c1a8b0d4e2a6f371";
   doc["spanId"]                 = "3ab4c1d2e3f40516";
   doc["attributes"]["event.name"]                 = "xrootd.transfer";
   doc["attributes"]["file.path"]                  = "/store/data/file.root";
   doc["attributes"]["xrootd.transfer.read_bytes"] = 10485760;
   doc["attributes"]["xrootd.transfer.is_local"]   = true;

   XrdMonOtlpBatch b;
   b.add(doc);
   EXPECT_TRUE(b.haveLogs());
   EXPECT_FALSE(b.haveTraces());

   json body = json::parse(b.takeLogsBody());
   ASSERT_TRUE(body.contains("resourceLogs"));
   ASSERT_EQ(body["resourceLogs"].size(), 1u);
   const json& rl = body["resourceLogs"][0];

   // Resource attributes are re-encoded as typed OTLP KeyValues.
   const json& ra = rl["resource"]["attributes"];
   const json* svc = kv(ra, "service.name");
   ASSERT_NE(svc, nullptr);
   EXPECT_EQ((*svc)["value"]["stringValue"], "xrootd");
   const json* inc = kv(ra, "xrootd.server.incarnation");
   ASSERT_NE(inc, nullptr);
   EXPECT_EQ((*inc)["value"]["intValue"], "1700000000");   // 64-bit int as string

   const json& sl = rl["scopeLogs"][0];
   EXPECT_EQ(sl["scope"]["name"], "xrdmoncollect");
   ASSERT_EQ(sl["logRecords"].size(), 1u);
   const json& lr = sl["logRecords"][0];
   EXPECT_EQ(lr["severityNumber"], 9);
   EXPECT_EQ(lr["severityText"], "INFO");
   EXPECT_EQ(lr["timeUnixNano"], "1751450432000000000");
   EXPECT_EQ(lr["traceId"], "9f1c8b0d4e2a6f37c1a8b0d4e2a6f371");
   EXPECT_EQ(lr["spanId"], "3ab4c1d2e3f40516");
   EXPECT_EQ(lr["body"]["stringValue"], "xrootd.transfer");

   const json& la = lr["attributes"];
   const json* fp = kv(la, "file.path");
   ASSERT_NE(fp, nullptr);
   EXPECT_EQ((*fp)["value"]["stringValue"], "/store/data/file.root");
   const json* rb = kv(la, "xrootd.transfer.read_bytes");
   ASSERT_NE(rb, nullptr);
   EXPECT_EQ((*rb)["value"]["intValue"], "10485760");
   const json* il = kv(la, "xrootd.transfer.is_local");
   ASSERT_NE(il, nullptr);
   EXPECT_EQ((*il)["value"]["boolValue"], true);
}

TEST(XrdMonOtlp, SpanEncoding)
{
   json doc;
   doc["resource"]["service.name"] = "xrootd";
   doc["scope"]["name"]            = "xrdmoncollect";
   doc["traceId"]                  = "9f1c8b0d4e2a6f37c1a8b0d4e2a6f371";
   doc["spanId"]                   = "3ab4c1d2e3f40516";
   doc["parentSpanId"]             = "1122334455667788";
   doc["name"]                     = "read";
   doc["kind"]                     = "SPAN_KIND_SERVER";
   doc["startTimeUnixNano"]        = "1700000000000000000";
   doc["endTimeUnixNano"]          = "1700000082000000000";
   doc["status"]["code"]           = "STATUS_CODE_ERROR";
   doc["status"]["message"]        = "permission denied";
   doc["attributes"]["file.path"]  = "/store/data/x.root";

   XrdMonOtlpBatch b;
   b.add(doc);
   EXPECT_TRUE(b.haveTraces());
   EXPECT_FALSE(b.haveLogs());

   json body = json::parse(b.takeTracesBody());
   ASSERT_TRUE(body.contains("resourceSpans"));
   ASSERT_EQ(body["resourceSpans"].size(), 1u);
   const json& sp = body["resourceSpans"][0]["scopeSpans"][0]["spans"][0];
   EXPECT_EQ(sp["name"], "read");
   EXPECT_EQ(sp["kind"], "SPAN_KIND_SERVER");
   EXPECT_EQ(sp["startTimeUnixNano"], "1700000000000000000");
   EXPECT_EQ(sp["endTimeUnixNano"], "1700000082000000000");
   EXPECT_EQ(sp["parentSpanId"], "1122334455667788");
   EXPECT_EQ(sp["status"]["code"], "STATUS_CODE_ERROR");
   EXPECT_EQ(sp["status"]["message"], "permission denied");
   const json* fp = kv(sp["attributes"], "file.path");
   ASSERT_NE(fp, nullptr);
   EXPECT_EQ((*fp)["value"]["stringValue"], "/store/data/x.root");
}

// Records sharing a resource collapse into one resource block; taking a body
// clears the accumulator for that signal.
TEST(XrdMonOtlp, GroupsByResourceAndResets)
{
   json doc;
   doc["resource"]["service.name"] = "xrootd";
   doc["severityText"]             = "INFO";
   doc["attributes"]["event.name"] = "xrootd.transfer";

   XrdMonOtlpBatch b;
   b.add(doc);
   b.add(doc);

   json body = json::parse(b.takeLogsBody());
   ASSERT_EQ(body["resourceLogs"].size(), 1u);
   EXPECT_EQ(body["resourceLogs"][0]["scopeLogs"][0]["logRecords"].size(), 2u);
   EXPECT_FALSE(b.haveLogs());   // cleared by takeLogsBody()
}
