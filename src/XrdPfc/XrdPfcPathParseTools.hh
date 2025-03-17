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

   SplitParser(const std::string &s, const char *d) :
      f_str(strdup(s.c_str())), f_delim(d), f_state(f_str)
   {}
   ~SplitParser() { free(f_str); }

   bool is_first() const { return f_state == f_str; }

   const char* get_token()
   {
      if ( ! f_state) return 0;
      // Skip leading delimiters, if any.
      char *t = f_state + strspn(f_state, f_delim);
      if (*t == (char) 0) { f_state = 0; return 0; }
      // Advance state to the next delimeter, if any.
      f_state = strpbrk(t, f_delim);
      if (f_state) {
          *f_state = (char) 0;
          ++f_state;
      }
      return t;
   }

   std::string get_token_as_string()
   {
      const char *t = get_token();
      return std::string(t ? t : "");
   }

   const char* get_reminder_with_delim()
   {
      if (is_first()) { return f_str; }
      else            { *(f_state - 1) = f_delim[0]; return f_state - 1; }
   }

   const char *get_reminder()
   {
      return f_state ? f_state : "";
   }

   bool has_reminder()
   {
      return f_state && f_state[0] != 0;
   }

   int pre_count_n_tokens() {
      int n_tok = 0;
      char *p = f_state;
      while (*p) {
         p += strspn(p, f_delim);
         if (*p == (char) 0)
            break;
         ++n_tok;
         p = strpbrk(p, f_delim);
         if ( ! p)
            break;
         ++p;
      }
      return n_tok;
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
      m_dirs.reserve(std::min(pre_count_n_tokens(), max_depth));

      const char *t = 0;
      for (int i = 0; i < max_depth; ++i)
      {
         t = get_token();
         if (t == 0) break;
         m_dirs.emplace_back(t);
      }
      if (parse_as_lfn && ! has_reminder() && ! m_dirs.empty())
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
