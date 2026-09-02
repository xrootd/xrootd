/******************************************************************************/
/*                                                                            */
/*             X r d H t t p T a p e A p i S t o r e T e s t . c c          */
/*                                                                            */
/* (c) 2026 by the XRootD Collaboration                                       */
/*                                                                            */
/* This file is part of the XRootD software suite.                            */
/*                                                                            */
/******************************************************************************/

#include <gtest/gtest.h>

#include "XrdHttpTapeApiStore.hh"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace
{
using Json = nlohmann::json;
namespace fs = std::filesystem;

class TapeApiStoreTest : public testing::Test
{
  protected:
    void SetUp() override
    {
      char path[] = "/tmp/xrd-http-tape-api.XXXXXX";
      const char *created = mkdtemp(path);
      ASSERT_NE(created, nullptr);
      m_root = created;
      m_store = std::make_unique<XrdHttpTapeApiStore>(m_root);
      ASSERT_TRUE(m_store->Initialize());
    }

    void TearDown() override
    {
      std::error_code ignored;
      fs::remove_all(m_root, ignored);
    }

    void Archive(const std::string &path, const std::string &contents)
    {
      const fs::path destination = m_root / "archive" /
                                   fs::path(path).relative_path();
      fs::create_directories(destination.parent_path());
      std::ofstream output(destination, std::ios::binary);
      output << contents;
      ASSERT_TRUE(output.good());
    }

    fs::path m_root;
    std::unique_ptr<XrdHttpTapeApiStore> m_store;
};

TEST_F(TapeApiStoreTest, PersistsStageAndReportsLocality)
{
  Archive("/store/file", "tape contents");

  Json locality;
  ASSERT_TRUE(m_store->ArchiveInfo(Json::array({"/store/file"}), locality));
  ASSERT_EQ(locality.size(), 1);
  EXPECT_EQ(locality[0]["locality"], "TAPE");

  const Json files = Json::array({{
    {"path", "//store/file"},
    {"diskLifetime", "PT1H"},
    {"targetedMetadata", {{"xrootd-ci", {{"activity", "analysis"}}}}}
  }});
  std::string requestId;
  ASSERT_TRUE(m_store->CreateStage(files, requestId));
  EXPECT_EQ(requestId.size(), 36);
  EXPECT_TRUE(fs::is_regular_file(m_root / "disk/store/file"));
  EXPECT_TRUE(fs::is_regular_file(
    m_root / "requests" / (requestId + ".json")));

  Json status;
  ASSERT_TRUE(m_store->GetStage(requestId, status));
  EXPECT_EQ(status["id"], requestId);
  EXPECT_EQ(status["files"][0]["path"], "/store/file");
  EXPECT_EQ(status["files"][0]["state"], "COMPLETED");
  EXPECT_TRUE(status.contains("completedAt"));
  EXPECT_FALSE(status["files"][0].contains("targetedMetadata"));

  ASSERT_TRUE(m_store->ArchiveInfo(Json::array({"/store/file"}), locality));
  EXPECT_EQ(locality[0]["locality"], "DISK_AND_TAPE");

  ASSERT_TRUE(m_store->Release(requestId, Json::array({"/store/file"})));
  EXPECT_FALSE(fs::exists(m_root / "disk/store/file"));
  ASSERT_TRUE(m_store->ArchiveInfo(Json::array({"/store/file"}), locality));
  EXPECT_EQ(locality[0]["locality"], "TAPE");

  ASSERT_TRUE(m_store->DeleteStage(requestId));
  EXPECT_EQ(m_store->GetStage(requestId, status).code, 404);
}

TEST_F(TapeApiStoreTest, RecordsPerFileStageFailures)
{
  std::string requestId;
  ASSERT_TRUE(m_store->CreateStage(
    Json::array({{{"path", "/store/missing"}}}), requestId));

  Json status;
  ASSERT_TRUE(m_store->GetStage(requestId, status));
  EXPECT_EQ(status["files"][0]["state"], "FAILED");
  EXPECT_EQ(status["files"][0]["error"], "file is not stored on tape");
}

TEST_F(TapeApiStoreTest, ValidatesBulkRequestMembershipBeforeMutation)
{
  Archive("/store/file", "tape contents");
  std::string requestId;
  ASSERT_TRUE(m_store->CreateStage(
    Json::array({{{"path", "/store/file"}}}), requestId));

  const auto status = m_store->Release(
    requestId, Json::array({"/store/file", "/store/other"}));
  EXPECT_EQ(status.code, 400);
  EXPECT_TRUE(fs::is_regular_file(m_root / "disk/store/file"));
}

TEST_F(TapeApiStoreTest, RejectsPathsOutsideTheStorageRoot)
{
  std::string requestId;
  ASSERT_TRUE(m_store->CreateStage(
    Json::array({{{"path", "/store/../outside"}}}), requestId));

  Json status;
  ASSERT_TRUE(m_store->GetStage(requestId, status));
  EXPECT_EQ(status["files"][0]["state"], "FAILED");
  EXPECT_FALSE(fs::exists(m_root / "outside"));
}
}
