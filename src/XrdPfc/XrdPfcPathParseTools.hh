#ifndef __XRDPFC_PATHPARSETOOLS_HH__
#define __XRDPFC_PATHPARSETOOLS_HH__

#include <string>
#include <vector>

#include <cstdio>
#include <cstring>

namespace XrdPfc {

struct SplitParser
{
   char       *f_str;
   const char *f_delim;
   char       *f_state;
   bool        f_first;

   SplitParser(const std::string &s, const char *d) :
      f_str(strdup(s.c_str())), f_delim(d), f_state(0), f_first(true)
   {}
   ~SplitParser() { free(f_str); }

   char* get_token()
   {
      if (f_first) { f_first = false; return strtok_r(f_str, f_delim, &f_state); }
      else         { return strtok_r(0, f_delim, &f_state); }
   }

   std::string get_token_as_string()
   {
      char *t = get_token();
      return std::string(t ? t : "");
   }

   char* get_reminder_with_delim()
   {
      if (f_first) { return f_str; }
      else         { *(f_state - 1) = f_delim[0]; return f_state - 1; }
   }

   char *get_reminder()
   {
      return f_first ? f_str : f_state;
   }

   bool has_reminder()
   {
      char *r = get_reminder();
      return r && r[0] != 0;
   }

   int fill_argv(std::vector<char*> &argv)
   {
      if (!f_first) return 0;
      int dcnt = 0; { char *p = f_str; while (*p) { if (*(p++) == f_delim[0]) ++dcnt; } }
      argv.reserve(dcnt + 1);
      int argc = 0;
      char *i = strtok_r(f_str, f_delim, &f_state);
      while (i)
      {
         ++argc;
         argv.push_back(i);
         // printf("  arg %d : '%s'\n", argc, i);
         i = strtok_r(0, f_delim, &f_state);
      }
      return argc;
   }
};

struct PathTokenizer : private SplitParser
{
   std::vector<const char*>  m_dirs;
   const char               *m_reminder;
   int                       m_n_dirs;

   PathTokenizer(const std::string &path, int max_depth, bool parse_as_lfn) :
      SplitParser(path, "/"),
      m_reminder (0),
      m_n_dirs   (0)
   {
      // max_depth - maximum number of directories to extract. If < 0, all path elements
      //             are extracted (well, up to 4096). The rest is in m_reminder.
      // If parse_as_lfn is true store final token into m_reminder, regardless of maxdepth.
      // This assumes the last token is a file name (and full path is lfn, including the file name).

      if (max_depth < 0)
         max_depth = 4096;
      m_dirs.reserve(std::min(8, max_depth));

      char *t = 0;
      for (int i = 0; i < max_depth; ++i)
      {
         t = get_token();
         if (t == 0) break;
         m_dirs.emplace_back(t);
      }
      if (parse_as_lfn && *get_reminder() == 0 && ! m_dirs.empty())
      {
         m_reminder = m_dirs.back();
         m_dirs.pop_back();
      }
      else
      {
         m_reminder = get_reminder();
      }
      m_n_dirs = (int) m_dirs.size();
   }

   int get_n_dirs()
   {
      return m_n_dirs;
   }

   const char *get_dir(int pos)
   {
      if (pos >= m_n_dirs) return 0;
      return m_dirs[pos];
   }

   std::string make_path()
   {
      std::string res;
      for (std::vector<const char*>::iterator i = m_dirs.begin(); i != m_dirs.end(); ++i)
      {
         res += "/";
         res += *i;
      }
      if (m_reminder != 0)
      {
         res += "/";
         res += m_reminder;
      }
      return res;
   }

   void print_debug()
   {
      printf("PathTokenizer::print_debug size=%d\n", m_n_dirs);
      for (int i = 0; i < m_n_dirs; ++i)
      {
         printf("   %2d: %s\n", i, m_dirs[i]);
      }
      printf("  rem: %s\n", m_reminder);
   }
};

}

#endif
