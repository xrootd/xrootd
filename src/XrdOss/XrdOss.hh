/*                     X r d O s s   &   X r d O s s D F                      */
/*                              X r d O s s D F                               */
                // Directory oriented methods
virtual int     Opendir(const char *, XrdOucEnv &)           {return -ENOTDIR;}
virtual int     Readdir(char *buff, int blen)                {(void)buff; (void)blen; return -ENOTDIR;}
virtual int     StatRet(struct stat *buff)                   {(void)buff; return -ENOTSUP;}

                // File oriented methods
virtual int     Fchmod(mode_t mode)                          {(void)mode; return -EISDIR;}
virtual int     Fstat(struct stat *)                         {return -EISDIR;}
virtual int     Fsync()                                      {return -EISDIR;}
virtual int     Fsync(XrdSfsAio *aiop)                       {(void)aiop; return -EISDIR;}
virtual int     Ftruncate(unsigned long long)                {return -EISDIR;}
virtual int     getFD()                                      {return -1;}
virtual off_t   getMmap(void **addr)                         {(void)addr; return 0;}
virtual int     isCompressed(char *cxidp=0)                  {(void)cxidp; return -EISDIR;}
virtual int     Open(const char *, int, mode_t, XrdOucEnv &) {return -EISDIR;}
virtual ssize_t Read(off_t, size_t)                          {return (ssize_t)-EISDIR;}
virtual ssize_t Read(void *, off_t, size_t)                  {return (ssize_t)-EISDIR;}
virtual int     Read(XrdSfsAio *aoip)                        {(void)aoip; return (ssize_t)-EISDIR;}
virtual ssize_t ReadRaw(    void *, off_t, size_t)           {return (ssize_t)-EISDIR;}
virtual ssize_t Write(const void *, off_t, size_t)           {return (ssize_t)-EISDIR;}
virtual int     Write(XrdSfsAio *aiop)                       {(void)aiop; return (ssize_t)-EISDIR;}

// Implemented in the header, as many folks will be happy with the default.
virtual ssize_t ReadV(XrdOucIOVec *readV, int n)
                     {ssize_t nbytes = 0, curCount = 0;
                      for (int i=0; i<n; i++)
                          {curCount = Read((void *)readV[i].data,
                                            (off_t)readV[i].offset,
                                           (size_t)readV[i].size);
                           if (curCount != readV[i].size)
                              {if (curCount < 0) return curCount;
                               return -ESPIPE;
                              }
                           nbytes += curCount;
                          }
                      return nbytes;
                     }

// Implemented in the header, as many folks will be happy with the default.
virtual ssize_t WriteV(XrdOucIOVec *writeV, int n)
                      {ssize_t nbytes = 0, curCount = 0;
                       for (int i=0; i<n; i++)
                           {curCount =Write((void *)writeV[i].data,
                                             (off_t)writeV[i].offset,
                                            (size_t)writeV[i].size);
                            if (curCount != writeV[i].size)
                               {if (curCount < 0) return curCount;
                                return -ESPIPE;
                               }
                            nbytes += curCount;
                           }
                       return nbytes;
                      }

                // Methods common to both
virtual int     Close(long long *retsz=0)=0;
inline  int     Handle() {return fd;}
virtual int     Fctl(int cmd, int alen, const char *args, char **resp=0)
{
  (void)cmd; (void)alen; (void)args; (void)resp;
  return -ENOTSUP;
}
                XrdOssDF() {fd = -1;}
int     fd;      // The associated file descriptor.
/*                                X r d O s s                                 */
// Class passed to StatVS()
class XrdOssVSInfo
{
public:
long long Total;   // Total bytes
long long Free;    // Total bytes free
long long Large;   // Total bytes in largest partition
long long LFree;   // Max   bytes free in contiguous chunk
long long Usage;   // Used  bytes (if usage enabled)
long long Quota;   // Quota bytes (if quota enabled)
int       Extents; // Number of partitions/extents
int       Reserved;

          XrdOssVSInfo() : Total(0),Free(0),Large(0),LFree(0),Usage(-1),
                           Quota(-1),Extents(0),Reserved(0) {}
         ~XrdOssVSInfo() {}
};
virtual int     Chmod(const char *, mode_t mode, XrdOucEnv *eP=0)=0;
virtual int     Create(const char *, const char *, mode_t, XrdOucEnv &, 
                       int opts=0)=0;
virtual int     Init(XrdSysLogger *, const char *)=0;
virtual int     Mkdir(const char *, mode_t mode, int mkpath=0,
                      XrdOucEnv *eP=0)=0;
virtual int     Reloc(const char *, const char *, const char *, const char *x=0)
                      {(void)x; return -ENOTSUP;}
virtual int     Remdir(const char *, int Opts=0, XrdOucEnv *eP=0)=0;
virtual int     Rename(const char *, const char *,
                       XrdOucEnv *eP1=0, XrdOucEnv *eP2=0)=0;
virtual int     Stat(const char *, struct stat *, int opts=0, XrdOucEnv *eP=0)=0;
virtual int     StatFS(const char *path, char *buff, int &blen, XrdOucEnv *eP=0)
{ (void)path; (void)buff; (void)blen; (void)eP; return -ENOTSUP;}
virtual int     StatLS(XrdOucEnv &env, const char *cgrp, char *buff, int &blen)
{ (void)env; (void)cgrp; (void)buff; (void)blen; return -ENOTSUP;}
virtual int     StatPF(const char *, struct stat *)
                      {return -ENOTSUP;}
virtual int     StatXA(const char *path, char *buff, int &blen, XrdOucEnv *eP=0)
{ (void)path; (void)buff; (void)blen; (void)eP; return -ENOTSUP;}
virtual int     StatXP(const char *path, unsigned long long &attr,
                       XrdOucEnv *eP=0)
{ (void)path; (void)attr; (void)eP; return -ENOTSUP;}
virtual int     Truncate(const char *, unsigned long long, XrdOucEnv *eP=0)=0;
virtual int     Unlink(const char *, int Opts=0, XrdOucEnv *eP=0)=0;

virtual int     Stats(char *bp, int bl) { (void)bp; (void)bl; return 0;}

virtual int     StatVS(XrdOssVSInfo *sP, const char *sname=0, int updt=0)
{ (void)sP; (void)sname; (void)updt; return -ENOTSUP;}

virtual int     Lfn2Pfn(const char *Path, char *buff, int blen)
                       {if ((int)strlen(Path) >= blen) return -ENAMETOOLONG;
                        strcpy(buff, Path); return 0;
                       }
const char     *Lfn2Pfn(const char *Path, char *buff, int blen, int &rc)
{ (void)buff; (void)blen; rc = 0; return Path;}

virtual int     FSctl(int cmd, int alen, const char *args, char **resp=0)
{ (void)cmd; (void)alen; (void)args; (void)resp; return -ENOTSUP;}
virtual void    EnvInfo(XrdOucEnv *envP) {(void)envP;}