#include "XrdPfcDirStateSnapshot.hh"
#include "XrdPfcPathParseTools.hh"
#include "XrdPfc.hh"
#include "XrdPfcTrace.hh"

#include "XrdOuc/XrdOucJson.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOss/XrdOss.hh"

#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <fcntl.h>

// Redefine to also support ordered_json ... we want to keep variable order in JSON save files.
#define PFC_DEFINE_TYPE_NON_INTRUSIVE(Type, ...)                                                \
  inline void to_json(nlohmann::json &nlohmann_json_j, const Type &nlohmann_json_t) {           \
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__))                    \
  }                                                                                             \
  inline void from_json(const nlohmann::json &nlohmann_json_j, Type &nlohmann_json_t) {         \
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, __VA_ARGS__))                  \
  }                                                                                             \
  inline void to_json(nlohmann::ordered_json &nlohmann_json_j, const Type &nlohmann_json_t) {   \
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, __VA_ARGS__))                    \
  }                                                                                             \
  inline void from_json(const nlohmann::ordered_json &nlohmann_json_j, Type &nlohmann_json_t) { \
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_FROM, __VA_ARGS__))                  \
  }

namespace XrdPfc
{
PFC_DEFINE_TYPE_NON_INTRUSIVE(DirStats,
   m_NumIos, m_Duration, m_BytesHit, m_BytesMissed, m_BytesBypassed, m_BytesWritten, m_StBlocksAdded, m_NCksumErrors,
   m_StBlocksRemoved, m_NFilesOpened, m_NFilesClosed, m_NFilesCreated, m_NFilesRemoved, m_NDirectoriesCreated, m_NDirectoriesRemoved)
PFC_DEFINE_TYPE_NON_INTRUSIVE(DirUsage,
    m_LastOpenTime, m_LastCloseTime, m_StBlocks, m_NFilesOpen, m_NFiles, m_NDirectories)
PFC_DEFINE_TYPE_NON_INTRUSIVE(DirStateElement,
   m_dir_name, m_stats, m_usage,
   m_parent, m_daughters_begin, m_daughters_end)
PFC_DEFINE_TYPE_NON_INTRUSIVE(DataFsSnapshot, 
   m_sshot_stats_reset_time, m_usage_update_time, m_disk_total, m_disk_used, m_file_usage, m_meta_total, m_meta_used,
   m_dir_states)
}
/*
namespace
{
// Open file for writing, throw exception on failure.
void open_ofstream(std::ofstream &ofs, const std::string &fname, const char *pfx = nullptr)
{
   ofs.open(fname, std::ofstream::trunc);
   if (!ofs)
   {
      char m[2048];
      snprintf(m, 2048, "%s%sError opening %s for write: %m", pfx ? pfx : "", pfx ? " " : "", fname.c_str());
      throw std::runtime_error(m);
   }
}
}
*/
using namespace XrdPfc;

namespace
{
   XrdSysTrace* GetTrace() { return Cache::GetInstance().GetTrace(); }
   const char *m_traceID = "DirStateSnapshot";
}

void DataFsSnapshot::write_json_file(const std::string &file_path, XrdOss& oss, bool include_preamble)
{
  // Create the data file.const
   const Configuration &conf    =  Cache::Conf();
   const char          *myUser = conf.m_username.c_str();
   XrdOucEnv            myEnv;

   const char* size_str = "524288";
   myEnv.Put("oss.asize",  size_str); // advisory size
   myEnv.Put("oss.cgroup", conf.m_data_space.c_str()); // AMT: data or metadata space

   mode_t mode = 0644;

   int cret;
   if ((cret = oss.Create(myUser, file_path.c_str(), mode, myEnv, XRDOSS_mkpath)) != XrdOssOK)
   {
      TRACE(Error, "Create failed for data file " << file_path << ERRNO_AND_ERRSTR(-cret));
      return;
   }

   XrdOssDF *myFile = oss.newFile(myUser);
   if ((cret = myFile->Open(file_path.c_str(), O_RDWR, mode, myEnv)) != XrdOssOK)
   {
      TRACE(Error, "Open failed for data file " << file_path << ERRNO_AND_ERRSTR(-cret));
      delete myFile;
      return;
   }

   // Fill the data file.
   std::ostringstream os;

   if (include_preamble)
   {
      os << "{ \"dirstate_snapshot\": ";
   }

   nlohmann::ordered_json j;
   to_json(j, *this);

   os << std::setw(1);
   os << j;

   if (include_preamble)
   {
      os << " }";
   }

   os << "\n";
   myFile->Ftruncate(0);
   myFile->Write(os.str().c_str(), 0, os.str().size());
   myFile->Close();     delete myFile;

   // Create the info file.

   std::string cinfo_path(file_path + Info::s_infoExtension);

   if ((cret = oss.Create(myUser, cinfo_path.c_str(), mode, myEnv, XRDOSS_mkpath)) != XrdOssOK)
   {
      TRACE(Error, "Create failed for info file " << cinfo_path << ERRNO_AND_ERRSTR(-cret));
      myFile->Close(); delete myFile;
      return;
   }

   XrdOssDF *myInfoFile = oss.newFile(myUser);
   if ((cret = myInfoFile->Open(cinfo_path.c_str(), O_RDWR, mode, myEnv)) != XrdOssOK)
   {
      TRACE(Error, "Open failed for info file " << cinfo_path << ERRNO_AND_ERRSTR(-cret));
      delete myInfoFile;
      myFile->Close(); delete myFile;
      return;
   }

   // Fill up cinfo.

   Info myInfo(GetTrace(), false);
   myInfo.SetBufferSizeFileSizeAndCreationTime(512*1024, os.str().size());
   myInfo.SetAllBitsSynced();

   myInfo.Write(myInfoFile, cinfo_path.c_str());
   myInfoFile->Close();
   delete myInfoFile;
}

void DataFsSnapshot::dump()
{
   nlohmann::ordered_json j;
   to_json(j, *this);
   std::cout << j.dump(3) << "\n";
}
