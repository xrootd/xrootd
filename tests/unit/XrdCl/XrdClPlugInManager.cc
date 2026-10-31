#include <gtest/gtest.h>

#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClPlugInInterface.hh"
#include "XrdCl/XrdClPlugInManager.hh"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

using namespace XrdCl;

namespace
{
  class NullFactory : public PlugInFactory
  {
  public:
    FilePlugIn *CreateFile( const std::string & ) override { return nullptr; }
    FileSystemPlugIn *CreateFileSystem( const std::string & ) override { return nullptr; }
  };
}

// The fixture is a friend of PlugInManager, to pass its own list of config
// directories instead of /etc/xrootd/client.plugins.d and the home directory.
// The tests which call ProcessEnvironmentSettings use the PlugInConfDir setting
// (XRD_PLUGINCONFDIR), which has the highest priority.
class PlugInManagerFixture : public testing::Test
{
protected:
  // Suffixed with the pid because ctest runs each test in its own process,
  // and the processes share the temp directory.
  const std::filesystem::path config_dir{std::filesystem::temp_directory_path() /
      ("xrdcl-tests-plugins-" + std::to_string(getpid()))};

  void SetUp() override
  {
    std::filesystem::remove_all(config_dir);
    std::filesystem::create_directory(config_dir);
    DefaultEnv::GetEnv()->PutString("PlugInConfDir", config_dir.string());
  }

  void TearDown() override
  {
    std::filesystem::remove_all(config_dir);
  }

  std::string make_dir(const std::string &name)
  {
    std::filesystem::create_directory(config_dir / name);
    return (config_dir / name).string();
  }

  // The manager reads the configs of a directory in the order of their names.
  void write_config(const std::filesystem::path &dir, const std::string &name,
                    const std::string &url, const std::string &enable)
  {
    std::ofstream file(dir / name);
    file << "url = " << url << "\n"
         << "lib = libXrdClRecorder.so\n"
         << "enable = " << enable << "\n";
  }

  void write_config(const std::string &name, const std::string &enable)
  {
    write_config(config_dir, name, "*", enable);
  }

  // The first directory has the highest priority.
  void process(PlugInManager &manager, const std::vector<std::string> &dirs)
  {
    manager.ProcessConfigDirs(dirs);
  }
};

// Issue #2618: the recorder.conf shipped by the packages is disabled for all
// URLs. It must not prevent a user config from enabling the recorder.
TEST_F(PlugInManagerFixture, EnabledDefaultPlugInLoadsAfterADisabledOne)
{
  write_config("1-disabled.conf", "false");
  write_config("2-enabled.conf", "true");

  PlugInManager manager;
  manager.ProcessEnvironmentSettings();

  ASSERT_NE(manager.GetFactory("root://localhost:1094//file"), nullptr);
}

// A disabled default plug-in config must not hide a factory registered through
// the public API, e.g. by an application or a test.
TEST_F(PlugInManagerFixture, DisabledDefaultPlugInDoesNotShadowARegisteredFactory)
{
  write_config("1-disabled.conf", "false");

  PlugInManager manager;
  manager.ProcessEnvironmentSettings();

  auto *factory = new NullFactory;
  ASSERT_TRUE(manager.RegisterFactory("mock://*", factory));
  ASSERT_EQ(manager.GetFactory("mock://localhost:1094//file"), factory);
}

// A user can disable a default plug-in which the system config enables.
TEST_F(PlugInManagerFixture, DisabledDefaultPlugInInAHigherDirectoryWins)
{
  const auto user = make_dir("user"), system = make_dir("system");
  write_config(user, "recorder.conf", "*", "false");
  write_config(system, "recorder.conf", "*", "true");

  PlugInManager manager;
  process(manager, {user, system});

  ASSERT_EQ(manager.GetFactory("root://localhost:1094//file"), nullptr);
}

// Issue #2618: the system config disables the recorder, the user enables it.
TEST_F(PlugInManagerFixture, EnabledDefaultPlugInInAHigherDirectoryWins)
{
  const auto user = make_dir("user"), system = make_dir("system");
  write_config(user, "recorder.conf", "*", "true");
  write_config(system, "recorder.conf", "*", "false");

  PlugInManager manager;
  process(manager, {user, system});

  ASSERT_NE(manager.GetFactory("root://localhost:1094//file"), nullptr);
}

// A plug-in for a protocol follows the same order as a default plug-in.
TEST_F(PlugInManagerFixture, EnabledProtocolPlugInInAHigherDirectoryWins)
{
  const auto user = make_dir("user"), system = make_dir("system");
  write_config(user, "recorder.conf", "root://*", "true");
  write_config(system, "recorder.conf", "root://*", "false");

  PlugInManager manager;
  process(manager, {user, system});

  ASSERT_NE(manager.GetFactory("root://localhost:1094//file"), nullptr);
}

// A lower directory still applies to the URLs which no higher directory sets.
TEST_F(PlugInManagerFixture, LowerDirectoryAppliesToTheRemainingURLs)
{
  const auto user = make_dir("user"), system = make_dir("system");
  write_config(user, "recorder.conf", "root://*", "false");
  write_config(system, "recorder.conf", "root://*;roots://*", "true");

  PlugInManager manager;
  process(manager, {user, system});

  EXPECT_EQ(manager.GetFactory("root://localhost:1094//file"), nullptr);
  EXPECT_NE(manager.GetFactory("roots://localhost:1094//file"), nullptr);
}

// Issue #2618: XRD_PLUGINCONFDIR can point to the home directory, which the
// manager then reads twice.
TEST_F(PlugInManagerFixture, SameDirectoryTwiceKeepsTheDefaultPlugIn)
{
  const auto user = make_dir("user");
  write_config(user, "recorder.conf", "*", "true");

  PlugInManager manager;
  process(manager, {user, user});

  ASSERT_NE(manager.GetFactory("root://localhost:1094//file"), nullptr);
}
