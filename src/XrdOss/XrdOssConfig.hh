#ifndef _XRDOSS_CONFIG_H
#define _XRDOSS_CONFIG_H
/******************************************************************************/
/*                                                                            */
/*                       X r d O s s C o n f i g . h h                        */
/*                                                                            */
/* (C) 2003 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*                DE-AC03-76-SFO0515 with the Deprtment of Energy             */
/******************************************************************************/

//         $Id$
  
#define  XRDOSS_VERSION "1.0.0"

/* Constant to indicate all went well.
*/
#ifndef XrdOssOK
#define XrdOssOK 0
#endif

/* The following defines are used to control path name and directory name
   construction. They should be ste to Min(local filesystem, MSS) limits.
*/
#define XrdOssMAX_PATH_LEN       1024
  
#define XrdOssMAXDNAME           MAXNAMLEN

/* Minimum amount of free space required on selected filesystem
*/
#define XrdOssMINALLOC 0

/* Percentage of requested space to be added to the requested space amount
*/
#define XrdOssOVRALLOC 0

/* The percentage difference between two free spaces in order for them to be
   considered equivalent in terms of allocation. Specify 0 to 100, where:
     0 - Always uses cache with the most space
   100 - Always does round robin allocation regardless of free space
*/
#define XrdOssFUZALLOC 0

/* The location of the configuration file. This can be overidden by setting
   XrdOssCONFIGFN environmental variable to the name.
*/
#define XrdOssCONFIGFN (char *)""

/* Set the root path to be whatever it needs to be in the local file system.
   The path prefixes the local path to the file.
*/
#define XrdOssLOCALROOT (char *)""

/* Set the root path to be whatever it needs to be in the remote file system.
   The root path prefixes the local path to the file.
*/
#define XrdOssREMOTEROOT (char *)""

// Set the following value to establish the ulimit for FD numbers. Zero
// sets it to whatever the current hard limit is. Negative leaves it alone.
//
#define XrdOssFDLIMIT     -1
#define XrdOssFDMINLIM    64

/* Set the following to be the connection queue length when connecting to the
   gateway. The default is usually the maximum on most systems.
*/
#define XrdOssGWBACKLOG 5

/* The MAXDBSIZE value sets the maximum number of bytes a database can have
   (actually applies to the seek limit). A value of zero imposes no limit.
*/
#define XrdOssMAXDBSIZE 0

/* Set the following to be the path name to be used when creating/connecting
   to the gateway socket port. Make sure it's writable by the process.
*/
#define XrdOssMSSGWPATH (char *)"/tmp/gateway.mss"

/* Set the XrdOssXEQFLAGS to whatever default value needed. Combine:
   XrdOssREADONLY   - All files can only be read. Return an error if
                     a create or write open is attempted.
   XrdOssFORCERO    - Convert r/w opens to r/o opens (create still fails).
   XrdOssINPLACE    - Do not use extended cache for allocations.
   XrdOssNODREAD    - bypass actual directory reads. Use this flag *only*
                     on hosts that do not process journal files.
   XrdOssNOCHECK    - Allow creates w/o checking the remote filesystem.
   XrdOssNOSSDEC    - Do not decompress files on the server.
   XrdOssNOSTAGE    - Do not stage files back in (implies MIG).
   XrdOssRCREATE    - Perform a create function on a remote filesystem.
                     O/W only an existence check is performed (implies MIG).
   XrdOssCOMPCHK    - Databases may be compressed, check for it.
*/
#define XrdOssXEQFLAGS   0
#define XrdOssREADONLY   0x00000001
#define XrdOssFORCERO    0x00000002
#define XrdOssNOTRW      0x00000003
#define XrdOssNODREAD    0x00000004
#define XrdOssRCREATE    0x00000008
#define XrdOssNOCHECK    0x00000010
#define XrdOssNOSTAGE    0x00000020
#define XrdOssPRUNED     0x00000040
#define XrdOssINPLACE    0x00000080
#define XrdOssCOMPCHK    0x00000100
#define XrdOssNOSSDEC    0x00000200
#define XrdOssUSRPRTY    0x00004000
#define XrdOssREMOTE     0x00008000
#define XrdOssNEEDMSS    0x10000000

/* Set the following to correspond to the command that is to be issued to
   stage in a remote file to the local filesystem. The command is invoked as:
            <XrdOssSTAGE_cmd> <remote_filename> <local_filename>
*/
#define XrdOssSTAGECMD (char *)"./XrdOssStage"

/* Set the following:
   XrdOssSCANINT    - Number of seconds between cache scans
   XrdOssXFRTHREADS - maximum number of threads used for staging
   XrdOssXFRSPEED   - average bytes/second to transfer a file
   XrdOssXFROVHD    - minimum number of seconds to get a file
   XrdOssXFRWAIT    - number of seconds to hold a failing stage request
*/
#define XrdOssCSCANINT    600
#define XrdOssXFRTHREADS    1
#define XrdOssXFRSPEED      9*1024*1024
#define XrdOssXFROVHD      30
#define XrdOssXFRHOLD       3*60*60

/******************************************************************************/
/*               r e m o t e   f l i s t   d e f i n i t i o n                */
/******************************************************************************/

/* The RPlist object defines an entry in the remote file list which is anchored
   at XrdOssSys::RPlist. There is one entry in the list for each remotepath
   directive. When a request comes in, the named path is compared with entries
   in the RPList. If no prefix match is found, the request is treated as being
   directed to the local filesystem. No path prefixing occurs and no remote
   filesystem is invoked. The list is searched in reverse of specification.
   No defaults can be specified for this list.
*/
#endif
