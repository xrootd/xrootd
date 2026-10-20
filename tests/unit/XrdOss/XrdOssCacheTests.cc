#include "XrdOss/XrdOssCache.hh"

#include <gtest/gtest.h>

#include <cerrno>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>

namespace
{

// Exercise Alloc itself with independent, deterministic free-space snapshots.
// No data files or separate mounted filesystems are needed: aMode remains zero.
class OssCacheAllocation : public ::testing::Test
{
protected:
    std::string root;
    std::vector<std::string> paths;
    std::vector<std::unique_ptr<XrdOssCache_FS>> disks;
    std::vector<std::unique_ptr<XrdOssCache_FSData>> snapshots;

    void SetUp() override
    {
        ASSERT_EQ(XrdOssCache::fsfirst, nullptr);
        ASSERT_EQ(XrdOssCache_Group::fsgroups, nullptr);
        char path[] = "/tmp/xrootd-alloc-XXXXXX";
        ASSERT_NE(mkdtemp(path), nullptr);
        root = path;
    }

    void TearDown() override
    {
        auto *group = XrdOssCache_Group::fsgroups;
        while (group)
        {
            auto *next = group->next;
            for (int i = 0; i < group->fsNum; ++i)
                free(group->fsVec[i].apVec);
            free(group->fsVec);
            delete group;
            group = next;
        }
        XrdOssCache_Group::fsgroups = nullptr;
        disks.clear();
        for (auto &snapshot : snapshots)
            if (snapshot->pact != snapshot->path)
                free(const_cast<char *>(snapshot->pact));
        snapshots.clear();
        auto *data = XrdOssCache::fsdata;
        while (data)
        {
            auto *next = data->next;
            if (data->pact != data->path)
                free(const_cast<char *>(data->pact));
            delete data;
            data = next;
        }
        XrdOssCache::fsdata = nullptr;
        XrdOssCache::fsfirst = XrdOssCache::fslast = nullptr;
        XrdOssCache::fsTotal = XrdOssCache::fsLarge = XrdOssCache::fsTotFr = 0;
        XrdOssCache::fsFree = XrdOssCache::fsSize = XrdOssCache::fsCount = 0;
        XrdOssCache::Init(0, 0, 0);
        for (const auto &path : paths) rmdir(path.c_str());
        if (!root.empty()) rmdir(root.c_str());
    }

    void Configure(const std::vector<long long> &free, int fuzz, int previous)
    {
        STATFS_t stats;
        ASSERT_EQ(FS_Stat(root.c_str(), &stats), 0);
        for (size_t i = 0; i < free.size(); ++i)
        {
            paths.push_back(root + "/disk" + std::to_string(i));
            ASSERT_EQ(mkdir(paths.back().c_str(), 0700), 0);
            int status = 0;
            disks.emplace_back(new XrdOssCache_FS(status, "allocation-test",
                paths.back().c_str(), XrdOssCache_FS::None));
            ASSERT_EQ(status, 0);
            snapshots.emplace_back(new XrdOssCache_FSData(
                paths.back().c_str(), stats, static_cast<dev_t>(i + 1)));
            snapshots.back()->frsz = free[i];
            disks.back()->fsdata = snapshots.back().get();
        }
        disks.front()->fsgroup->curr = disks[previous].get();
        ASSERT_EQ(XrdOssCache::Init(0, 0, fuzz), 0);
    }

    int Allocate(long long size = 1, int only = -1)
    {
        char pfn[1024];
        XrdOssCache::allocInfo request("/test.root", pfn, sizeof(pfn));
        request.cgName = "allocation-test";
        request.cgSize = size;
        if (only >= 0)
        {
            request.cgPath = disks[only]->path;
            request.cgPlen = disks[only]->plen;
        }
        int result = XrdOssCache::Alloc(request);
        if (result < 0) return result;
        for (size_t i = 0; i < disks.size(); ++i)
            if (request.cgFSp == disks[i].get()) return static_cast<int>(i);
        ADD_FAILURE() << "Allocator returned an unknown filesystem";
        return -1;
    }
};

TEST_F(OssCacheAllocation, IntermediateFuzzDoesNotReplaceWithFullerDisk)
{
    Configure({100000, 100}, 10, 1);
    for (int fuzz = 1; fuzz < 100; ++fuzz)
    {
        XrdOssCache::Init(0, 0, fuzz);
        disks.front()->fsgroup->curr = disks[1].get();
        EXPECT_EQ(Allocate(), 0) << "fuzz=" << fuzz;
    }
}

TEST_F(OssCacheAllocation, IntermediateFuzzFindsEmptierDiskLaterInScan)
{
    Configure({100, 1000}, 10, 1);
    EXPECT_EQ(Allocate(), 1);
}

TEST_F(OssCacheAllocation, IntermediateFuzzRotatesAcrossSimilarDisks)
{
    Configure({1000, 990, 980, 970, 960, 950, 940, 930, 920, 910, 900, 890}, 10, 11);
    for (int i = 0; i < 24; ++i) EXPECT_EQ(Allocate(), i % 12);
}

TEST_F(OssCacheAllocation, IntermediateFuzzRotatesWithinEmptierSet)
{
    Configure({1000, 100, 990, 90}, 10, 3);
    for (int i = 0; i < 12; ++i) EXPECT_EQ(Allocate(), i % 2 ? 2 : 0);
}

TEST_F(OssCacheAllocation, ZeroFuzzSelectsMostFreeSpace)
{
    Configure({100, 1000, 200}, 0, 0);
    EXPECT_EQ(Allocate(), 1);
}

TEST_F(OssCacheAllocation, FullFuzzRemainsRoundRobin)
{
    Configure({1000, 100, 500}, 100, 2);
    for (int i = 0; i < 6; ++i) EXPECT_EQ(Allocate(), i % 3);
}

TEST_F(OssCacheAllocation, InsufficientSpaceIsExcluded)
{
    Configure({1000, 4, 0}, 10, 0);
    EXPECT_EQ(Allocate(5), 0);
}

TEST_F(OssCacheAllocation, NoSuitableDiskReturnsNoSpace)
{
    Configure({4, 0}, 10, 1);
    EXPECT_EQ(Allocate(5), -ENOSPC);
}

TEST_F(OssCacheAllocation, ExplicitPartitionRestrictionIsPreserved)
{
    Configure({1000, 100}, 10, 1);
    EXPECT_EQ(Allocate(5, 1), 1);
}

} // namespace
