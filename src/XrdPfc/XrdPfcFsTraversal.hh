#ifndef __XRDPFC_FSTRAVERSAL_HH__
#define __XRDPFC_FSTRAVERSAL_HH__

#include "XrdOss/XrdOssAt.hh"
#include "XrdOuc/XrdOucEnv.hh"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <sys/stat.h>

class XrdOss;
class XrdOssDF;

namespace XrdPfc {

struct DirState;

class FsTraversal
{
public:
   struct FilePairStat {
      struct stat stat_data, stat_cinfo;
      bool has_data  = false;
      bool has_cinfo = false;

      void set_data (const struct stat &s) { stat_data  = s; has_data  = true; }
      void set_cinfo(const struct stat &s) { stat_cinfo = s; has_cinfo = true; }
      bool has_both() const { return has_data && has_cinfo; }
   };

protected:
   XrdOss   &m_oss;
   XrdOssAt  m_oss_at;
   XrdOucEnv m_env;

   bool m_maintain_dirstate   = false;

public:
   DirState *m_root_dir_state = nullptr;
   DirState *m_dir_state      = nullptr; // current DirState

   int          m_rel_dir_level = -1; // dir level relative to root, 0 ~ at root
   std::string  m_current_path;       // Includes trailing '/' -- needed for printouts and PurgeCandidate creation.

   // Hmmh ... need a stack of those ... or not, if doing tail recursion.
   // Can not, OpenDirAt descend can not be like that, ie, i will  need the old handle.
   std::vector<XrdOssDF*> m_dir_handle_stack;

   std::vector<std::string>            m_current_dirs;  // swap out into local scope before recursion
   std::map<std::string, FilePairStat> m_current_files; // clear when done

   std::set<std::string> m_protected_top_dirs;          // directories that will NOT be traversed at relative level 0.

   static const char *m_traceID;

   void slurp_current_dir();
   void slurp_dir_ll(XrdOssDF &dh, int dir_level, const char *path, const char *trc_pfx);

public:
   FsTraversal(XrdOss &oss);
   ~FsTraversal();

   bool begin_traversal(DirState *root, const char *root_path);
   bool begin_traversal(const char *root_path);
   void end_traversal();

   bool cd_down(const std::string &dir_name);
   void cd_up();

   int open_at_ro(const char* fname, XrdOssDF *&ossDF) {
      return m_oss_at.OpenRO(*m_dir_handle_stack.back(), fname, m_env, ossDF);
   }
   int unlink_at(const char* fname) {
      return m_oss_at.Unlink(*m_dir_handle_stack.back(), fname);
   }
   int close_delete(XrdOssDF *&ossDF);

   XrdOucEnv& default_env() { return m_env; }
};

}

#endif
