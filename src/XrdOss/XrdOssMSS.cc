/******************************************************************************/
/*                                                                            */
/*                          X r d O s s M S S . c c                           */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/
  
//         $Id$

const char *XrdOssMSSCVSID = "$Id$";

#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <iostream.h>
#include <signal.h>
#include <stdio.h>
#include <time.h>
#if defined(AIX) || defined(SUNCC)
#include <sys/vnode.h>
#include <sys/mode.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

#include "Experiment/Experiment.hh"
#include "XrdOss/XrdOssApi.hh"
#include "XrdOss/XrdOssConfig.hh"
#include "XrdOss/XrdOssError.hh"
#include "XrdOss/XrdOssTrace.hh"
#include "XrdOuc/XrdOucError.hh"
#include "XrdOuc/XrdOucPlatform.hh"
#include "XrdOuc/XrdOucSocket.hh"
#include "XrdOuc/XrdOucStream.hh"

/******************************************************************************/
/*                 S t o r a g e   S y s t e m   O b j e c t                  */
/******************************************************************************/
  
extern XrdOssSys XrdOssSS;

/******************************************************************************/
/*                  E r r o r   R o u t i n g   O b j e c t                   */
/******************************************************************************/
  
extern XrdOucError OssEroute;

extern XrdOucTrace OssTrace;

/******************************************************************************/
/*                               d e f i n e s                                */
/******************************************************************************/

#define NegVal(x) (x <= 0 ? (x < 0 ? x : -1) : -x)

/******************************************************************************/
/*                           f i l e   h a n d l e                            */
/******************************************************************************/

/* These are private data structures. They are allocated dynamically to the
   appropriate size (yes, that means dbx has a tough time).
*/

struct XrdOssHandle
       {int hflag;
        XrdOucStream *sp;

        XrdOssHandle(int type, XrdOucStream *newsp=0) {hflag = type; sp = newsp;}
       ~XrdOssHandle() {if (sp) delete sp; hflag = 0;}

       };

#define XRDOSS_HT_EOF    1
#define XRDOSS_HT_DIR    4

/******************************************************************************/
/*          M a s s   S t o r a g e   R e l a t e d   M e t h o d s           */
/******************************************************************************/
  
/******************************************************************************/
/*                               o p e n d i r                                */
/******************************************************************************/
  
int XrdOssSys::MSS_Opendir(char *dir_path) {
/*
  Function: Open the directory `path' and prepare for reading.

  Input:    path      - The fully qualified name of the directory to open.

  Output:   Returns a directory handle to be used for subsequent
            operations. If an error occurs, (-errno) is returned.
*/
     const char *epname = "MSS_Opendir";
     char cmdbuff[XrdOssMAX_PATH_LEN+32];
     int retc;
     struct XrdOssHandle *oh;
     XrdOucStream *sp;

     // Make sure the path is not too long.
     //
     if (strlen(dir_path) > XrdOssMAX_PATH_LEN)
        {OssEroute.Emsg(epname, "mss path too long - ", dir_path);
         return -ENAMETOOLONG;
        }

     // Construct the command to get the contents of the directory.
     //
     sprintf(cmdbuff, "%s %s", "dlist", dir_path);

     // Issue it now to trap any errors but defer reading the result until
     // readdir() is called. This does tie up a process, sigh.
     //
     if ( (retc = MSS_Xeq(cmdbuff, &sp, ENOENT)) ) return retc;

     // Allocate storage for the handle and return a copy of it.
     //
     if (!(oh = new XrdOssHandle(XRDOSS_HT_DIR, sp))) {delete sp; return -ENOMEM;}
     return (int)oh;
}

/******************************************************************************/
/*                               r e a d d i r                                */
/******************************************************************************/

int XrdOssSys::MSS_Readdir(int dir_handle, char * buff, int blen) {
/*
  Function: Read the next entry if directory 'dir_handle'.

  Input:    dir_handle - The value returned by a successful opendir() call.
            buff       - Buffer to hold directory name.
            blen       - Size of the buffer.

  Output:   Upon success, places the contents of the next directory entry
            in buff. When the end of the directory is encountered buff
            will be set to a null string.

            Upon failure, returns a (-errno).
*/
    const char *epname = "MSS_Readdir";
    int retc;
    struct XrdOssHandle *oh = (struct XrdOssHandle *)dir_handle;
    char *resp;

    // Verify that the handle is correct.
    //
    if ( !(oh->hflag & XRDOSS_HT_DIR) )
       {OssEroute.Emsg(epname, "invalid mss handle"); return -EBADF;}

    // Read a record from the directory, if possible.
    //
    if (oh->hflag & XRDOSS_HT_EOF) *buff = '\0';
       else if (resp = oh->sp->GetLine())
               {if ( (strlen(resp)) >= blen )
                   {*buff = '\0';
                    return OssEroute.Emsg("XrdOssMSS_Readdir", -EOVERFLOW,
                                            "readdir rmt", resp);
                   }
                   strlcpy(buff, (const char *)resp, blen);
               } else {
                if (retc = oh->sp->LastError()) return NegVal(retc);
                   else {*buff = '\0'; oh->hflag |= XRDOSS_HT_EOF;}
               }
    return XrdOssOK;
}

/******************************************************************************/
/*                              c l o s e d i r                               */
/******************************************************************************/
  
int XrdOssSys::MSS_Closedir(int dir_handle) {
/*
  Function: Close the directory associated with handle "dir_handle".

  Input:    dir_handle - The handle returned by opendir().

  Output:   Returns 0 upon success and (-errno) upon failure.
*/
    const char *epname = "MSS_Closedir";
    struct XrdOssHandle *oh = (struct XrdOssHandle *)dir_handle;

    if ( !(oh->hflag & XRDOSS_HT_DIR) )
       {OssEroute.Emsg(epname, "invalid mss handle"); return -EBADF;}
    delete oh;
    return XrdOssOK;
}

/******************************************************************************/
/*                                c r e a t e                                 */
/******************************************************************************/

int XrdOssSys::MSS_Create(char *path, mode_t file_mode, XrdOucEnv &env)
/*
  Function: Create a file named `path' with 'file_mode' access mode bits set.

  Input:    path      - The fully qualified name of the file to create.
            file_mode - The Posix access mode bits to be assigned to the file.
                        These bits correspond to the standard Unix permission
                        bits (e.g., 744 == "rwxr--r--").
            env         Enviornmental information.

  Output:   Returns zero upon success and (-errno) otherwise.
*/
{
    const char *epname = "MSS_Create";
    char cmdbuff[XrdOssMAX_PATH_LEN+32];
    int retc;

    // Make sure the path is not too long.
    //
    if (strlen(path) > XrdOssMAX_PATH_LEN)
       {OssEroute.Emsg(epname, "mss path too long - ", path);
        return -ENAMETOOLONG;
       }

    // Construct the cmd to create the file. We currently don't support cosid.
    //
    sprintf(cmdbuff, "create %s %o", path, file_mode);

    // Create the file in in the mass store system
    //
    return MSS_Xeq(cmdbuff, 0, 0);
}

/******************************************************************************/
/*                                 s t a t                                    */
/******************************************************************************/

/*
  Function: Determine if file 'path' actually exists.

  Input:    path        - Is the fully qualified name of the file to be tested.
            buff        - pointer to a 'stat' structure to hold the attributes
                          of the file.

  Output:   Returns 0 upon success and -errno upon failure.
*/

int XrdOssSys::MSS_Stat(char *path, struct stat *buff)
{
    const char *epname = "MSS_Stat";
    char cmdbuff[XrdOssMAX_PATH_LEN+32];
    char ftype, mtype[10], *resp;
    int retc, xt_nlink;
    long xt_uid, xt_gid, atime, ctime, mtime, xt_blksize, xt_blocks;
    double xt_size; // Until Solaris 8, sigh.
    XrdOucStream *sfd;

    // Make sure the path is not too long.
    //
    if (strlen(path) > XrdOssMAX_PATH_LEN)
       {OssEroute.Emsg(epname, "mss path too long - ", path);
        return -ENAMETOOLONG;
       }

     // Construct the command to stat the file.
     //
     sprintf(cmdbuff, "statx %s", path);

    // issue the command.
    //
    if (retc = MSS_Xeq(cmdbuff, &sfd, ENOENT)) return retc;

    // Read in the results.
    //
    if ( !(resp = sfd ->GetLine()))
       return OssEroute.Emsg("XrdOssMSS_Stat",-XRDOSS_E8012,"processing ",path);

    // Extract data from the response.
    //
    sscanf(resp, "%c %9s %d %ld %ld %ld %ld %ld %lf %ld %ld", &ftype, mtype,
           &xt_nlink, &xt_uid, &xt_gid, &atime, &ctime, &mtime,
           &xt_size, &xt_blksize, &xt_blocks);

    // Set the stat buffer, appropriately.
    //
    memset( (char *)buff, 0, sizeof(struct stat) );
    buff->st_nlink = xt_nlink;
    buff->st_uid   = xt_uid;
    buff->st_gid   = xt_gid;
    buff->st_atime = atime;
    buff->st_ctime = ctime;
    buff->st_mtime = mtime;
    buff->st_size  = (off_t)xt_size;
    buff->st_blksize=xt_blksize;
    buff->st_blocks =xt_blocks;

    if (ftype == 'd') buff->st_mode |=  S_IFDIR;
       else if (ftype == 'l') buff->st_mode |= S_IFLNK;
               else buff->st_mode |= S_IFREG;

    buff->st_mode |= tranmode(&mtype[0]) << 6;
    buff->st_mode |= tranmode(&mtype[3]) << 3;
    buff->st_mode |= tranmode(&mtype[6]);

    delete sfd;
    return 0;
}

int XrdOssSys::tranmode(char *mode) {
    int mbits = 0;
    if (mode[0] == 'r') mbits |= S_IROTH;
    if (mode[1] == 'w') mbits |= S_IWOTH;
    if (mode[2] == 'x') mbits |= S_IXOTH;
    return mbits;
}

/******************************************************************************/
/*                                r e m o v e                                 */
/******************************************************************************/

/*
  Function: Delete a file from the namespace and release it's data storage.

  Input:    path      - Is the fully qualified name of the file to be removed.

  Output:   Returns 0 upon success and -errno upon failure.
*/
int XrdOssSys::MSS_Unlink(char *path) {
    const char *epname = "MSS_Unlink";
    char cmdbuff[XrdOssMAX_PATH_LEN+32];

    // Make sure the path is not too long.
    //
    if (strlen(path) > XrdOssMAX_PATH_LEN)
       {OssEroute.Emsg(epname, "mss path too long - ", path);
        return -ENAMETOOLONG;
       }

    // Construct the command to remove the file.
    //
    sprintf(cmdbuff, "rm %s", path);

    // Remove the file in Mass Store System.
    //
    return MSS_Xeq(cmdbuff, 0, ENOENT);
}

/******************************************************************************/
/*                                r e n a m e                                 */
/******************************************************************************/

/*
  Function: Renames a file with name 'old_name' to 'new_name'.

  Input:    old_name  - Is the fully qualified name of the file to be renamed.
            new_name  - Is the fully qualified name that the file is to have.

  Output:   Returns 0 upon success and -errno upon failure.
*/
int XrdOssSys::MSS_Rename(char *oldname, char *newname) {
    const char *epname = "MSS_Rename";
    char cmdbuff[XrdOssMAX_PATH_LEN*2+32];

    // Make sure the path is not too long.
    //
    if (strlen(oldname) > XrdOssMAX_PATH_LEN
    ||  strlen(newname) > XrdOssMAX_PATH_LEN)
       {OssEroute.Emsg(epname,"mss path too long - ", oldname, newname);
        return -ENAMETOOLONG;
       }

    // Construct the command to rename the file.
    //
    sprintf(cmdbuff, "mv %s %s", oldname, newname);

    // Rename the file in Mass Store System
    //
    return MSS_Xeq(cmdbuff, 0, 0);
}

/******************************************************************************/
/*                     P R I V A T E    S E C T I O N                         */
/******************************************************************************/

/******************************************************************************/
/*                              M S S _ I n i t                               */
/******************************************************************************/
  
int XrdOssSys::MSS_Init(int Warm) {
/*
  Function: Perform one-time-only initialization at start-up.

  Input:    None.

  Output:   Zero is returned upon success; otherwise an error code is returned.
*/
   const char *epname = "MSS_Init";
   int retc, child, NoGo = 0;
   struct sockaddr_un USock;

// Tell the world what we are doing (but only do it once)
//
   if (Warm) return 0;
   OssEroute.Emsg("MSS_Init","Mass storage interface initialization started.");

// Make sure the socket path exists and is not too long.
//
   retc = strlen(MSSgwPath);
   if (!retc || retc > sizeof(USock.sun_path))
      {OssEroute.Emsg("XrdOssMSS_Init", "gatheway path unspecified or too long.");
       return -1;
      }

// fork a child to handle all external communications.
//
   if ( (child = fork()) < 0 )
      return OssEroute.Emsg("MSS_Init", -errno, "forking process");
   if (!child) MSS_Gateway();

// On parent, return we are done.
//
   DEBUG("external gateway initiated; process " <<child);
   OssEroute.Emsg("MSS_Init", "Mass storage interface initialized.");
   return 0;
}

/******************************************************************************/
/*                               M S S _ X e q                                */
/******************************************************************************/

int XrdOssSys::MSS_Xeq(char *cmd, XrdOucStream **xfd, int okerr) {
    const char *epname = "MSS_Xeq";
    char mss_cmd[XrdOssMAX_PATH_LEN*2+1024], *resp;
    int retc, cmd_len;
    XrdOucStream *sp;
    XrdOucSocket Sock(&OssEroute);

    // Close the file if we got a null command buffer.
    //
    if ((!cmd || !cmd[0]) && *xfd) {(*xfd)->Close(); return 0;}

    // Construct the command that we will use to execute the mss subroutine.
    //
    if ( (cmd_len = MSSgwCmdLen + strlen(cmd) +2) > sizeof(mss_cmd))
       return OssEroute.Emsg("XrdOssMSS_Xeq", -XRDOSS_E8013, "executing", cmd);
    sprintf(mss_cmd, "%s %s", MSSgwCmd, cmd);

    // Get a connection to the gateway server.
    //
   if (Sock.Open(MSSgwPath) < 0) return NegVal(Sock.LastError());

   // Allocate a stream for this connection.
   //
   if (!(sp = new XrdOucStream(&OssEroute)))
      {Sock.Close();
       return OssEroute.Emsg("XrdOssMSS_Xeq",-ENOMEM,"creating stream for",mss_cmd);
      }
   sp->Attach(Sock.Detach());

   // Send the command to the gateway.
   //
   DEBUG("sending to mss '" <<mss_cmd <<"'");
   if (sp->Put(mss_cmd, cmd_len) < 0)
      {retc = sp->LastError(); delete sp; return NegVal(retc);}
   sp->Flush(); // Make sure it gets there

    // Read back the first record. The first records must be the return code
    // from the command followed by any output. Make sure that this is the case.
    //
    if ( !(resp = sp->GetLine()) ) retc = EPROTO;
       else 
       {DEBUG("received '" <<resp <<"'");
        if (sscanf(resp, "%d", &retc) <= 0) retc = EPROTO;
       }
    if (retc)
       {if (retc != -okerr)
           OssEroute.Emsg("XrdOssMSS_Xeq", NegVal(retc), "executing", cmd);
        delete sp;
        return NegVal(retc);
       }

     // If the caller wants the stream pointer; return it. Otherwise close it.
     //
     if (xfd) *xfd = sp;
        else delete sp;
     return 0;
}

/******************************************************************************/
/*                           M S S _ G a t e w a y                            */
/******************************************************************************/
  
void XrdOssSys::MSS_Gateway(void) {
     const char *epname = "MSS_Gateway";
     XrdOucStream IOStream(&OssEroute);
     XrdOucSocket IOSock(&OssEroute);
     int InSock, pidstat;
     char *request;
     pid_t parent = getppid();
     int   timeout = 1000*60*3; // Check for parent every 3 minutes

// Allocate a socket.
//
   if (IOSock.Open(MSSgwPath, -1, 
                   XrdOucSOCKET_SERVER | (XrdOucSOCKET_BKLG & gwBacklog)) < 0)
      {OssEroute.Emsg("XrdOssMSS_Gateway", IOSock.LastError(),
                 "creating gateway socket; terminating gateway");
       exit(4);
      }

// Now enter an accept loop to process requests. On every successful accept,
// we attach the socket to a stream, read one line, execute on that stream,
// and close it on our end, the output will appear on the requestor's side.
//
   while(1) {if ((InSock = IOSock.Accept(timeout)) >= 0)
                {IOStream.Attach(InSock);
                 if (request = IOStream.GetLine()) 
                    {DEBUG("received '" <<request <<"'");
                     IOStream.Exec(request);
                    }
                 IOStream.Close(1);                // Close but do not kill
                 waitpid(-1, &pidstat, WNOHANG);   // Reap any previous child
                 continue;
                }
             if (kill(parent, 0) < 0 && errno == ESRCH)
                {OssEroute.Emsg("XrdOssMSS_Gateway",
                            "parent process terminated; terminating gateway");
                 unlink((const char *)MSSgwPath);
                 exit(4);
                }
             waitpid(-1, &pidstat, WNOHANG);   // Reap any previous child
            }
}
