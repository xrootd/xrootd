#include "XrdPfcFsTraversal.hh"
#include "XrdPfcDirState.hh"
#include "XrdPfc.hh"
#include "XrdPfcTrace.hh"

#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOss/XrdOssApi.hh"

// #define TRACE_PURGE(x) std::cout << "PURGE " << x << "\n"
#define TRACE_PURGE(x)

using namespace XrdPfc;

namespace
{
    XrdSysTrace* GetTrace() { return Cache::GetInstance().GetTrace(); }
}

const char *FsTraversal::m_traceID = "FsTraversal";

//----------------------------------------------------------------------------

FsTraversal::FsTraversal(XrdOss &oss) :
   m_oss(oss), m_oss_at(oss)
{}

FsTraversal::~FsTraversal()
{}

int FsTraversal::close_delete(XrdOssDF *&ossDF)
{
   int ret = 0;
   if (ossDF) {
      ret = ossDF->Close();
      delete ossDF;
   }
   ossDF = nullptr;
   return ret;
}

//----------------------------------------------------------------------------

bool FsTraversal::begin_traversal(DirState *root, const char *root_path)
{
   m_maintain_dirstate = true;
   m_root_dir_state = m_dir_state = root;

   bool ret = begin_traversal(root_path);

   return ret;
}

bool FsTraversal::begin_traversal(const char *root_path)
{
   static const char *trc_pfx = "FsTraversal::begin_traversal ";

   assert(root_path && strlen(root_path) > 0 && root_path[0] == '/');

   m_rel_dir_level = 0;
   m_current_path = root_path;

   XrdOssDF* dhp = m_oss.newDir("PfcFsTraversal");
   if (dhp->Opendir(root_path, m_env) != XrdOssOK) {
      delete dhp;
      TRACE(Error, trc_pfx << "could not opendir [" << root_path << "], " << XrdSysE2T(errno));
      return false;
   }
   m_dir_handle_stack.push_back(dhp);

   TRACE_PURGE("FPurgeState::begin_traversal cur_path '" << m_current_path << "', rel_level=" << m_rel_dir_level);

   slurp_current_dir();
   return true;
}

void FsTraversal::end_traversal()
{
   TRACE_PURGE("FPurgeState::end_traversal reporting for '" << m_current_path << "', re_level=" << m_rel_dir_level);

   for (auto &dhp : m_dir_handle_stack) {
      dhp->Close();
      delete dhp;
   }
   m_dir_handle_stack.clear();
   m_current_path.clear();
   m_current_dirs.clear();
   m_current_files.clear();

   m_rel_dir_level  = -1;
   m_root_dir_state = m_dir_state = nullptr;
   m_maintain_dirstate = false;
}

//----------------------------------------------------------------------------

bool FsTraversal::cd_down(const std::string &dir_name)
{
   static const char *trc_pfx = "FsTraversal::cd_down ";

   XrdOssDF *dhp = 0;
   if (m_oss_at.Opendir(*m_dir_handle_stack.back(), dir_name.c_str(), m_env, dhp) != XrdOssOK) {
      delete dhp;
      TRACE(Error, trc_pfx << "could not opendir [" << m_current_path << dir_name << "], " << XrdSysE2T(errno));
      return false;
   }
   m_dir_handle_stack.push_back(dhp);

   ++m_rel_dir_level;
   m_current_path.append(dir_name);
   m_current_path.append("/");

   if (m_maintain_dirstate)
      m_dir_state = m_dir_state->find_dir(dir_name, true);

   slurp_current_dir();
   return true;
}

void FsTraversal::cd_up()
{
   m_current_dirs.clear();
   m_current_files.clear();

   m_dir_handle_stack.back()->Close();
   delete m_dir_handle_stack.back();
   m_dir_handle_stack.pop_back();

   if (m_maintain_dirstate)
      m_dir_state = m_dir_state->get_parent();

   m_current_path.erase(m_current_path.find_last_of('/', m_current_path.size() - 2) + 1);
   --m_rel_dir_level;
}

//----------------------------------------------------------------------------

void FsTraversal::slurp_current_dir()
{
   static const char *trc_pfx = "FsTraversal::slurp_current_dir ";

   XrdOssDF &dh = *m_dir_handle_stack.back();
   slurp_dir_ll(dh, m_rel_dir_level, m_current_path.c_str(), trc_pfx);
}

//----------------------------------------------------------------------------

void FsTraversal::slurp_dir_ll(XrdOssDF &dh, int dir_level, const char *path, const char *trc_pfx)
{
   // Low-level implementation of slurp dir.

   char fname[256];
   struct stat fstat;

   dh.StatRet(&fstat);

   const char   *info_ext     = Info::s_infoExtension;
   const size_t  info_ext_len = Info::s_infoExtensionLen;

   m_current_dirs.clear();
   m_current_files.clear();

   while (true)
   {
      int rc = dh.Readdir(fname, 256);

      if (rc == -ENOENT)
      {
         TRACE_PURGE("  Skipping ENOENT dir entry [" << fname << "].");
         continue;
      }
      if (rc != XrdOssOK)
      {
         TRACE(Error, trc_pfx << "Readdir error at " << path << ", err " << XrdSysE2T(-rc) << ".");
         break;
      }

      TRACE_PURGE("  Readdir [" << fname << "]");

      if (fname[0] == 0)
      {
         TRACE_PURGE("  Finished reading dir [" << path << "]. Break loop.");
         break;
      }
      if (fname[0] == '.' && (fname[1] == 0 || (fname[1] == '.' && fname[2] == 0)))
      {
         TRACE_PURGE("  Skipping here or parent dir [" << fname << "]. Continue loop.");
         continue;
      }

      if (S_ISDIR(fstat.st_mode))
      {
         if (dir_level == 0 && m_protected_top_dirs.find(fname) != m_protected_top_dirs.end())
         {
            // Skip protected top-directories.
            continue;
         }
         m_current_dirs.push_back(fname);
      }
      else
      {
         size_t fname_len = strlen(fname);

         if (fname_len > info_ext_len && strncmp(&fname[fname_len - info_ext_len], info_ext, info_ext_len) == 0)
         {
            // truncate ".cinfo" away
            fname[fname_len - info_ext_len] = 0;
            m_current_files[fname].set_cinfo(fstat);
         }
         else
         {
            m_current_files[fname].set_data(fstat);
         }
      }
   }
}
